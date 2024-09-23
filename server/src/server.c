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

    return DATABASE_CONNECTION_SUCCESS;
}

int user_auth(request_t* req, client_t* cl)
{
    message_t msg;
    char buffer[BUFFER_SIZE];
    int nbytes;

    sqlite3* db;
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

    // [ ] Query the database for the username, etc.

    // for no warnings
    cl->request = req;
    cl->uid = "test";
    printf("User ID: %s\n", cl->uid);
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
    char welcome_message[BUFFER_SIZE];
    sprintf(welcome_message, "Welcome to the chat server, your ID is %d\n", cl.id);
    create_message(&msg, MESSAGE_TEXT, "server", "client", welcome_message);
    send_message(cl.request->socket, &msg);

    char join_message[BUFFER_SIZE];
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
            printf("Received message from client %d: %s\n", cl.id, msg.payload);

            pthread_mutex_lock(&clients.mutex);
            for (int i = 0; i < MAX_CLIENTS; ++i)
            {
                if (clients.array[i] && clients.array[i] != &cl)
                {
                    msg.type = MESSAGE_TEXT;
                    msg.payload_length = strlen(msg.payload);
                    send_message(clients.array[i]->request->socket, &msg); // it is bugged here of on the client side to append part of ID to the message
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
    connect_db(&db, DB_NAME);
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
