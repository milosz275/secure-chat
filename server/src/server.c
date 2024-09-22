#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sqlite3.h>
#include <ctype.h>

#include "protocol.h"

static struct clients_t clients = { PTHREAD_MUTEX_INITIALIZER, {NULL} };

int connect_db(sqlite3** db, char* db_name)
{
    char db_path[256];
    snprintf(db_path, sizeof(db_path), "../database/%s", db_name);
    if (sqlite3_open(db_path, db) != SQLITE_OK)
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(*db));
        fprintf(stderr, "Trying again...\n"); // this is for Docker to work
        sqlite3_close(*db);
        if (sqlite3_open(db_name, db) != SQLITE_OK)
        {
            fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(*db));
            sqlite3_close(*db);
            return DATABASE_OPEN_FAILURE;
        }
        fprintf(stderr, "Database opened in the server directory\n");
    }

    // users
    char* sql = "CREATE TABLE IF NOT EXISTS users ("
        "user_id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "username TEXT NOT NULL UNIQUE, "
        "password_hash TEXT NOT NULL, "
        "created_at TEXT DEFAULT CURRENT_TIMESTAMP, "
        "last_login TEXT"
        ");";
    if (sqlite3_exec(*db, sql, NULL, 0, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "Can't create users table: %s\n", sqlite3_errmsg(*db));
        sqlite3_close(*db);
        return DATABASE_CREATE_USERS_TABLE_FAILURE;
    }

    // groups
    sql = "CREATE TABLE IF NOT EXISTS groups ("
        "group_id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "group_name TEXT NOT NULL UNIQUE, "
        "created_by INTEGER NOT NULL, "
        "created_at TEXT DEFAULT CURRENT_TIMESTAMP, "
        "FOREIGN KEY (created_by) REFERENCES users(user_id) ON DELETE CASCADE"
        ");";
    if (sqlite3_exec(*db, sql, NULL, 0, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "Can't create groups table: %s\n", sqlite3_errmsg(*db));
        sqlite3_close(*db);
        return DATABASE_CREATE_GROUPS_TABLE_FAILURE;
    }

    // group_membership
    sql = "CREATE TABLE IF NOT EXISTS group_membership ("
        "membership_id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "group_id INTEGER NOT NULL, "
        "user_id INTEGER NOT NULL, "
        "joined_at TEXT DEFAULT CURRENT_TIMESTAMP, "
        "FOREIGN KEY (group_id) REFERENCES groups(group_id) ON DELETE CASCADE, "
        "FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE, "
        "UNIQUE (group_id, user_id)"
        ");";
    if (sqlite3_exec(*db, sql, NULL, 0, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "Can't create group_membership table: %s\n", sqlite3_errmsg(*db));
        sqlite3_close(*db);
        return DATABASE_CREATE_GROUP_MEMBERSHIP_TABLE_FAILURE;
    }

    // messages
    sql = "CREATE TABLE IF NOT EXISTS messages ("
        "message_id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "sender_id INTEGER NOT NULL, "
        "content TEXT NOT NULL, "
        "created_at TEXT DEFAULT CURRENT_TIMESTAMP, "
        "is_group_message BOOLEAN DEFAULT FALSE, "
        "FOREIGN KEY (sender_id) REFERENCES users(user_id) ON DELETE CASCADE"
        ");";
    if (sqlite3_exec(*db, sql, NULL, 0, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "Can't create messages table: %s\n", sqlite3_errmsg(*db));
        sqlite3_close(*db);
        return DATABASE_CREATE_MESSAGES_TABLE_FAILURE;
    }

    // message_recipients
    sql = "CREATE TABLE IF NOT EXISTS message_recipients ("
        "recipient_id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "message_id INTEGER NOT NULL, "
        "recipient_user_id INTEGER, "
        "recipient_group_id INTEGER, "
        "FOREIGN KEY (message_id) REFERENCES messages(message_id) ON DELETE CASCADE, "
        "FOREIGN KEY (recipient_user_id) REFERENCES users(user_id) ON DELETE CASCADE, "
        "FOREIGN KEY (recipient_group_id) REFERENCES groups(group_id) ON DELETE CASCADE, "
        "CHECK (recipient_user_id IS NOT NULL OR recipient_group_id IS NOT NULL), "
        "CHECK (recipient_user_id IS NULL OR recipient_group_id IS NULL)"
        ");";
    if (sqlite3_exec(*db, sql, NULL, 0, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "Can't create message_recipients table: %s\n", sqlite3_errmsg(*db));
        sqlite3_close(*db);
        return DATABASE_CREATE_MESSAGE_RECIPIENTS_TABLE_FAILURE;
    }

    return DATABASE_CONNECTION_SUCCESS;
}

void trim_whitespace(char* str)
{
    char* end;
    while (isspace((unsigned char)*str)) str++;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
}

int user_auth(request_t* req)
{
    char buffer[BUFFER_SIZE];
    int nbytes;
    sqlite3_stmt* stmt;
    sqlite3* db;
    int login_attempts = 0;

    if (connect_db(&db, "sqlite.db") != DATABASE_CONNECTION_SUCCESS)
    {
        fprintf(stderr, "Database connection failed in user_auth\n");
        return USER_AUTHENTICATION_DATABASE_CONNECTION_FAILURE;
    }

    while (login_attempts < 3)
    {
        char username[64];
        char password[64];
        char password_confirm[64];
        char* password_hash;
        char* sql;
        int user_exists = 0;

        const char* username_prompt = "Enter your username: ";
        send(req->socket, username_prompt, strlen(username_prompt), 0);

        nbytes = recv(req->socket, buffer, sizeof(buffer), 0);
        if (nbytes <= 0)
        {
            close(req->socket);
            sqlite3_close(db);
            return USER_AUTHENTICATION_USERNAME_RECEIVE_FAILURE;
        }

        buffer[nbytes] = '\0';
        trim_whitespace(buffer);
        strcpy(username, buffer);

        sql = "SELECT user_id FROM users WHERE username = ?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        {
            fprintf(stderr, "Can't prepare statement: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return USER_AUTHENTICATION_USERNAME_QUERY_FAILURE;
        }
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            user_exists = 1;
        }
        sqlite3_finalize(stmt);

        if (user_exists)
        {
            const char* password_prompt = "Enter your password: ";
            send(req->socket, password_prompt, strlen(password_prompt), 0);

            nbytes = recv(req->socket, buffer, sizeof(buffer), 0);
            if (nbytes <= 0)
            {
                close(req->socket);
                sqlite3_close(db);
                return USER_AUTHENTICATION_PASSWORD_RECEIVE_FAILURE;
            }
            buffer[nbytes] = '\0';
            trim_whitespace(buffer);
            strcpy(password, buffer);

            password_hash = (char*)get_hash(password);

            sql = "SELECT user_id FROM users WHERE username = ? AND password_hash = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
            {
                fprintf(stderr, "Can't prepare statement: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                return USER_AUTHENTICATION_PASSWORD_QUERY_FAILURE;
            }
            sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, password_hash, -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return USER_AUTHENTICATION_SUCCESS;
            }
            else
            {
                const char* invalid_msg = "Invalid credentials\n";
                send(req->socket, invalid_msg, strlen(invalid_msg), 0);
                login_attempts++;
            }
            sqlite3_finalize(stmt);
        }
        else
        {
            const char* no_user_prompt = "User does not exist. Do you want to create a new account? [y/n]: ";
            send(req->socket, no_user_prompt, strlen(no_user_prompt), 0);

            nbytes = recv(req->socket, buffer, sizeof(buffer), 0);
            if (nbytes <= 0)
            {
                close(req->socket);
                sqlite3_close(db);
                return USER_AUTHENTICATION_CHOICE_RECEIVE_FAILURE;
            }
            buffer[nbytes] = '\0';
            trim_whitespace(buffer);

            if (strcmp(buffer, "y") == 0 || strcmp(buffer, "Y") == 0)
            {
                const char* password_prompt = "Enter a password: ";
                send(req->socket, password_prompt, strlen(password_prompt), 0);

                nbytes = recv(req->socket, buffer, sizeof(buffer), 0);
                if (nbytes <= 0)
                {
                    close(req->socket);
                    sqlite3_close(db);
                    return USER_AUTHENTICATION_PASSWORD_RECEIVE_FAILURE;
                }
                buffer[nbytes] = '\0';
                trim_whitespace(buffer);
                strcpy(password, buffer);

                const char* confirm_password_prompt = "Confirm your password: ";
                send(req->socket, confirm_password_prompt, strlen(confirm_password_prompt), 0);

                nbytes = recv(req->socket, buffer, sizeof(buffer), 0);
                if (nbytes <= 0)
                {
                    close(req->socket);
                    sqlite3_close(db);
                    return USER_AUTHENTICATION_PASSWORD_CONFIRMATION_RECEIVE_FAILURE;
                }
                buffer[nbytes] = '\0';
                trim_whitespace(buffer);
                strcpy(password_confirm, buffer);

                if (strcmp(password, password_confirm) != 0)
                {
                    const char* mismatch_msg = "Passwords do not match.\n";
                    send(req->socket, mismatch_msg, strlen(mismatch_msg), 0);
                    login_attempts++;
                    continue;
                }

                password_hash = (char*)get_hash(password);
                fprintf(stderr, "Password hash: %s\n", password_hash);

                sql = "INSERT INTO users (username, password_hash) VALUES (?, ?);";
                if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
                {
                    fprintf(stderr, "Can't prepare statement: %s\n", sqlite3_errmsg(db));
                    sqlite3_close(db);
                    return USER_AUTHENTICATION_USER_CREATION_QUERY_FAILURE;
                }
                sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, password_hash, -1, SQLITE_STATIC);

                int step_result = sqlite3_step(stmt);
                if (step_result == SQLITE_DONE)
                {
                    const char* success_msg = "Account created successfully.\n";
                    send(req->socket, success_msg, strlen(success_msg), 0);
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    return USER_AUTHENTICATION_SUCCESS;
                }
                else
                {
                    const char* failure_msg = "Failed to create account.\n";
                    send(req->socket, failure_msg, strlen(failure_msg), 0);
                    fprintf(stderr, "Failed to create account: %s\n", sqlite3_errmsg(db));
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    return USER_AUTHENTICATION_USER_CREATION_FAILURE;
                }
            }
            else if (strcmp(buffer, "n") == 0 || strcmp(buffer, "N") == 0)
            {
                const char* retry_msg = "Retrying login...\n";
                send(req->socket, retry_msg, strlen(retry_msg), 0);
                login_attempts++;
            }
            else
            {
                const char* invalid_input_msg = "Invalid input. Retrying login...\n";
                fprintf(stderr, "Invalid input: %s\n", buffer);
                send(req->socket, invalid_input_msg, strlen(invalid_input_msg), 0);
                login_attempts++;
            }
        }

        if (login_attempts >= 3)
        {
            const char* failure_msg = "Too many failed login attempts\n";
            send(req->socket, failure_msg, strlen(failure_msg), 0);
            close(req->socket);
            sqlite3_close(db);
            return USER_AUTHENTICATION_ATTEMPTS_EXCEEDED;
        }
    }

    sqlite3_close(db);
    return USER_AUTHENTICATION_FAILURE;
}

void* handle_client(void* arg)
{
    char buffer[BUFFER_SIZE];
    int nbytes;
    request_t req = *((request_t*)arg);
    message_t msg;

    int auth_result = user_auth(&req);
    if (auth_result != USER_AUTHENTICATION_SUCCESS)
    {
        close(req.socket);
        pthread_exit(NULL);
    }

    client_t cl;
    cl.request = &req;

    pthread_mutex_lock(&clients.mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (!clients.array[i])
        {
            cl.id = i;
            printf("Client %d connected\n", cl.id);
            clients.array[i] = &cl;
            break;
        }
    }
    pthread_mutex_unlock(&clients.mutex);

    char welcome_message[BUFFER_SIZE];
    sprintf(welcome_message, "Welcome to the chat server, your ID is %d\n", cl.id);
    send(cl.request->socket, welcome_message, strlen(welcome_message), 0);

    char join_message[BUFFER_SIZE];
    sprintf(join_message, "Client %d has joined the chat\n", cl.id);

    pthread_mutex_lock(&clients.mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients.array[i] && clients.array[i] != &cl)
        {
            send(clients.array[i]->request->socket, join_message, strlen(join_message), 0);
        }
    }
    pthread_mutex_unlock(&clients.mutex);

    while ((nbytes = recv(cl.request->socket, buffer, sizeof(buffer), 0)) > 0)
    {
        if (nbytes == sizeof(message_t))
        {
            memcpy(&msg, buffer, sizeof(message_t));
            printf("Received message from client %d to recipient %s: %s\n", cl.id, msg.recipient_uid, msg.message);
            pthread_mutex_lock(&clients.mutex);
            int recipient_found = 0;

            for (int i = 0; i < MAX_CLIENTS; ++i)
            {
                // if (clients.array[i] && strcmp(clients.array[i]->uid, msg.recipient_uid) == 0)

                // temporary use id instead of uid
                char id_str[64];
                sprintf(id_str, "%d", clients.array[i]->id);
                if (clients.array[i] && strcmp(id_str, msg.recipient_uid) == 0)
                {
                    send(clients.array[i]->request->socket, msg.message, strlen(msg.message), 0);
                    recipient_found = 1;
                    break;
                }
            }

            if (!recipient_found)
            {
                char error_message[BUFFER_SIZE];
                sprintf(error_message, "Recipient with ID %s not found.\n", msg.recipient_uid);
                send(cl.request->socket, error_message, strlen(error_message), 0);
            }

            pthread_mutex_unlock(&clients.mutex);
        }
        else
        {
            printf("Received malformed message from client %d\n", cl.id);
        }
    }

    pthread_mutex_lock(&clients.mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients.array[i] == &cl)
        {
            printf("Client %d disconnected\n", cl.id);
            clients.array[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients.mutex);

    close(cl.request->socket);
    pthread_exit(NULL);
}

int run_server()
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    request_t req;
    sqlite3* db;
    connect_db(&db, "sqlite.db");
    sqlite3_close(db);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) < 0)
    {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", PORT);

    while (1)
    {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0)
        {
            perror("Accept failed");
            continue;
        }

        req.socket = client_socket;
        req.address = client_addr;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, (void*)&req) != 0)
        {
            perror("Thread creation failed");
            close(client_socket);
            continue;
        }
    }
    close(server_socket);

    return 0;
}
