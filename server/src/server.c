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
    char db_path[DB_PATH_LENGTH];
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
    sqlite3_exec(*db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);

    return DATABASE_CONNECTION_SUCCESS;
}

int setup_db(sqlite3** db, char* db_name)
{
    if (connect_db(db, db_name) != DATABASE_CONNECTION_SUCCESS)
    {
        return DATABASE_OPEN_FAILURE;
    }

    // users
    char* sql = "CREATE TABLE IF NOT EXISTS users ("
        "user_id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "username TEXT NOT NULL UNIQUE, "
        "uid TEXT NOT NULL UNIQUE, "
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

    return DATABASE_CREATE_SUCCESS;
}

int user_auth(request_t* req, client_t* cl)
{
    message_t msg;
    char buffer[BUFFER_SIZE];
    int nbytes;

    sqlite3* db;
    sqlite3_stmt* stmt;
    int db_result = connect_db(&db, DB_NAME);
    if (db_result != DATABASE_CONNECTION_SUCCESS)
    {
        close(req->socket);
        return USER_AUTHENTICATION_DATABASE_CONNECTION_FAILURE;
    }

    create_message(&msg, MESSAGE_TEXT, "server", "client", "Enter your username: ");
    send_message(req->socket, &msg);

    nbytes = recv(req->socket, buffer, sizeof(buffer), 0);
    if (nbytes <= 0)
    {
        close(req->socket);
        return USER_AUTHENTICATION_USERNAME_RECEIVE_FAILURE;
    }

    parse_message(&msg, buffer);
    printf("Received username: %s\n", msg.payload);

    char username[MAX_USERNAME_LENGTH + 1];
    snprintf(username, MAX_USERNAME_LENGTH + 1, "%.*s", MAX_USERNAME_LENGTH, msg.payload);
    username[MAX_USERNAME_LENGTH] = '\0';

    char* sql = "SELECT uid FROM users WHERE username = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
    {
        fprintf(stderr, "Can't prepare username query: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        close(req->socket);
        return USER_AUTHENTICATION_USERNAME_QUERY_FAILURE;
    }

    if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK)
    {
        fprintf(stderr, "Can't bind username parameter: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        close(req->socket);
        return USER_AUTHENTICATION_USERNAME_QUERY_FAILURE;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        // register new user
        create_message(&msg, MESSAGE_TEXT, "server", "client", "Username does not exist, would you like to register? [y/n]: ");
        send_message(req->socket, &msg);

        nbytes = recv(req->socket, buffer, sizeof(buffer), 0);
        if (nbytes <= 0)
        {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            close(req->socket);
            return USER_AUTHENTICATION_CHOICE_RECEIVE_FAILURE;
        }

        parse_message(&msg, buffer);
        printf("Received choice: %s\n", msg.payload);

        if (strcmp(msg.payload, "y") == 0 || strcmp(msg.payload, "Y") == 0)
        {
            create_message(&msg, MESSAGE_TEXT, "server", "client", "Enter your password: ");
            send_message(req->socket, &msg);

            nbytes = recv(req->socket, buffer, sizeof(buffer), 0);
            if (nbytes <= 0)
            {
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                close(req->socket);
                return USER_AUTHENTICATION_PASSWORD_RECEIVE_FAILURE;
            }

            parse_message(&msg, buffer);
            printf("Received password: %s\n", msg.payload);

            char password[MAX_PASSWORD_LENGTH];
            snprintf(password, MAX_PASSWORD_LENGTH + 1, "%.*s", MAX_PASSWORD_LENGTH, msg.payload);

            create_message(&msg, MESSAGE_TEXT, "server", "client", "Confirm your password: ");
            send_message(req->socket, &msg);

            nbytes = recv(req->socket, buffer, sizeof(buffer), 0);
            if (nbytes <= 0)
            {
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                close(req->socket);
                return USER_AUTHENTICATION_PASSWORD_CONFIRMATION_RECEIVE_FAILURE;
            }

            parse_message(&msg, buffer);
            printf("Received password confirmation: %s\n", msg.payload);

            char password_confirmation[MAX_PASSWORD_LENGTH];
            snprintf(password_confirmation, MAX_PASSWORD_LENGTH + 1, "%.*s", MAX_PASSWORD_LENGTH, msg.payload);

            if (strcmp(password, password_confirmation) == 0)
            {
                cl->uid = (char*)generate_unique_user_id(username);
                printf("Generated UID for %s: %s\n", username, cl->uid);

                char* password_hash = generate_password_hash(password);

                char* sql = "INSERT INTO users (username, uid, password_hash, last_login) VALUES (?, ?, ?, CURRENT_TIMESTAMP);";

                if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
                {
                    fprintf(stderr, "Can't prepare user creation query: %s\n", sqlite3_errmsg(db));
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    close(req->socket);
                    return USER_AUTHENTICATION_USER_CREATION_QUERY_FAILURE;
                }

                if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK)
                {
                    fprintf(stderr, "Can't bind username parameter: %s\n", sqlite3_errmsg(db));
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    close(req->socket);
                    return USER_AUTHENTICATION_USER_CREATION_QUERY_FAILURE;
                }

                if (sqlite3_bind_text(stmt, 2, cl->uid, -1, SQLITE_STATIC) != SQLITE_OK)
                {
                    fprintf(stderr, "Can't bind UID parameter: %s\n", sqlite3_errmsg(db));
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    close(req->socket);
                    return USER_AUTHENTICATION_USER_CREATION_QUERY_FAILURE;
                }

                if (sqlite3_bind_text(stmt, 3, (char*)password_hash, PASSWORD_HASH_LENGTH, SQLITE_STATIC) != SQLITE_OK)
                {
                    fprintf(stderr, "Can't bind password hash parameter: %s\n", sqlite3_errmsg(db));
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    close(req->socket);
                    return USER_AUTHENTICATION_USER_CREATION_QUERY_FAILURE;
                }

                if (sqlite3_step(stmt) != SQLITE_DONE)
                {
                    fprintf(stderr, "Can't create user: %s\n", sqlite3_errmsg(db));
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    close(req->socket);
                    return USER_AUTHENTICATION_USER_CREATION_FAILURE;
                }
            }
            else
            {
                create_message(&msg, MESSAGE_TEXT, "server", "client", "Passwords do not match, please try again");
                send_message(req->socket, &msg);

                sqlite3_finalize(stmt);
                sqlite3_close(db);
                close(req->socket);
                return USER_AUTHENTICATION_PASSWORD_MISMATCH;
            }
        }
        else if (strcmp(msg.payload, "n") == 0 || strcmp(msg.payload, "N") == 0)
        {
            create_message(&msg, MESSAGE_TEXT, "server", "client", "Enter your username : ");
            send_message(req->socket, &msg);

            sqlite3_finalize(stmt);
            sqlite3_close(db);
            close(req->socket);
            return USER_AUTHENTICATION_USERNAME_DOES_NOT_EXIST;
        }
        else
        {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            close(req->socket);
            return USER_AUTHENTICATION_INVALID_CHOICE;
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }
    else
    {
        // authenticate user
        create_message(&msg, MESSAGE_TEXT, "server", "client", "Enter your password: ");
        send_message(req->socket, &msg);

        nbytes = recv(req->socket, buffer, sizeof(buffer), 0);
        if (nbytes <= 0)
        {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            close(req->socket);
            return USER_AUTHENTICATION_PASSWORD_RECEIVE_FAILURE;
        }

        parse_message(&msg, buffer);
        printf("Received password: %s\n", msg.payload);

        char password[MAX_PASSWORD_LENGTH];
        snprintf(password, MAX_PASSWORD_LENGTH + 1, "%.*s", MAX_PASSWORD_LENGTH, msg.payload);

        char* sql = "SELECT uid FROM users WHERE username = ? AND password_hash = ?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
        {
            fprintf(stderr, "Can't prepare user authentication query: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            close(req->socket);
            return USER_AUTHENTICATION_USER_AUTHENTICATION_QUERY_FAILURE;
        }

        if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK)
        {
            fprintf(stderr, "Can't bind username parameter: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            close(req->socket);
            return USER_AUTHENTICATION_USER_AUTHENTICATION_QUERY_FAILURE;
        }

        char* password_hash = generate_password_hash(password);

        if (sqlite3_bind_text(stmt, 2, (char*)password_hash, PASSWORD_HASH_LENGTH, SQLITE_STATIC) != SQLITE_OK)
        {
            fprintf(stderr, "Can't bind password hash parameter: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            close(req->socket);
            return USER_AUTHENTICATION_USER_AUTHENTICATION_QUERY_FAILURE;
        }

        if (sqlite3_step(stmt) != SQLITE_ROW)
        {
            fprintf(stderr, "Authentication failed: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            close(req->socket);
            return USER_AUTHENTICATION_USER_AUTHENTICATION_FAILURE;
        }

        const char* uid = (const char*)sqlite3_column_text(stmt, 0);

        cl->uid = (char*)malloc(strlen(uid) + 1);
        if (cl->uid == NULL)
        {
            fprintf(stderr, "Memory allocation failed for cl->uid\n");
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            close(req->socket);
            return USER_AUTHENTICATION_MEMORY_ALLOCATION_FAILURE;
        }
        strcpy(cl->uid, uid);
        printf("Authenticated user %s with UID %s\n", username, cl->uid);
        sqlite3_finalize(stmt);

        const char* update_sql = "UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE uid = ?;";
        if (sqlite3_prepare_v2(db, update_sql, -1, &stmt, 0) != SQLITE_OK)
        {
            fprintf(stderr, "Can't prepare last login update query: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            close(req->socket);
            return USER_AUTHENTICATION_LAST_LOGIN_UPDATE_FAILURE;
        }

        if (sqlite3_bind_text(stmt, 1, cl->uid, -1, SQLITE_STATIC) != SQLITE_OK)
        {
            fprintf(stderr, "Can't bind uid parameter for last login update: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            close(req->socket);
            return USER_AUTHENTICATION_LAST_LOGIN_UPDATE_FAILURE;
        }

        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            fprintf(stderr, "Failed to update last login: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            close(req->socket);
            return USER_AUTHENTICATION_LAST_LOGIN_UPDATE_FAILURE;
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    sqlite3_close(db);

    cl->request = req;
    cl->timestamp = (char*)get_timestamp();

    return USER_AUTHENTICATION_SUCCESS;
}

void* handle_client(void* arg)
{
    char buffer[BUFFER_SIZE];
    int nbytes;
    request_t* req_ptr = ((request_t*)arg);
    request_t req = *req_ptr;
    message_t msg;
    client_t cl;

    if (user_auth(&req, &cl) != USER_AUTHENTICATION_SUCCESS)
    {
        close(req.socket);
        pthread_exit(NULL);
    }

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

    pthread_mutex_lock(&clients.mutex);
    char welcome_message[MAX_PAYLOAD_SIZE];
    sprintf(welcome_message, "Welcome to the chat server, your ID is %d\n", cl.id);
    create_message(&msg, MESSAGE_TEXT, "server", "client", welcome_message);
    send_message(cl.request->socket, &msg);

    char join_message[MAX_PAYLOAD_SIZE];
    sprintf(join_message, "Client %d has joined the chat\n", cl.id);
    create_message(&msg, MESSAGE_TEXT, "server", "client", join_message);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients.array[i] && clients.array[i] != &cl)
        {
            send_message(clients.array[i]->request->socket, &msg);
        }
    }
    pthread_mutex_unlock(&clients.mutex);

    while ((nbytes = recv(cl.request->socket, buffer, sizeof(buffer), 0)) > 0)
    {
        if (nbytes > 0)
        {
            parse_message(&msg, buffer);
            printf("Received message from client %d, %s: %s\n", cl.id, msg.sender_uid, msg.payload);

            char payload[MAX_PAYLOAD_SIZE];
            strncpy(payload, msg.payload, MAX_PAYLOAD_SIZE);
            payload[MAX_PAYLOAD_SIZE - 1] = '\0';

            pthread_mutex_lock(&clients.mutex);
            for (int i = 0; i < MAX_CLIENTS; ++i)
            {
                if (clients.array[i] && clients.array[i] != &cl)
                {
                    char id_str[20];
                    snprintf(id_str, sizeof(id_str), "%d", cl.id);
                    create_message(&msg, MESSAGE_TEXT, id_str, "client", payload);
                    send_message(clients.array[i]->request->socket, &msg);
                }
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
    setup_db(&db, DB_NAME);
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

    int attempts = 0;
    int max_attempts = 30;
    int bind_success = 0;
    while (attempts < max_attempts)
    {
        if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
        {
            perror("Bind failed");
            attempts++;
            if (attempts < max_attempts)
            {
                sleep(2);
            }
            else
            {
                printf("Could not bind to the server address\n");
                close(server_socket);
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            bind_success = 1;
            break;
        }
    }
    if (!bind_success)
    {
        printf("Could not bind to the server address\n");
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

        req.timestamp = (char*)get_timestamp();
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
