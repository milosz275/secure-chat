#ifndef __SERVER_H
#define __SERVER_H

#include <pthread.h>
#include <arpa/inet.h>
#include <sqlite3.h>
#include "protocol.h"

#define MAX_CLIENTS 100

#define DB_NAME "sqlite.db"
#define DB_PATH_LENGTH 256

// The database connection result codes.
#define DATABASE_CONNECTION_SUCCESS 200
#define DATABASE_OPEN_FAILURE 201
#define DATABASE_CREATE_USERS_TABLE_FAILURE 202
#define DATABASE_CREATE_GROUPS_TABLE_FAILURE 203
#define DATABASE_CREATE_GROUP_MEMBERSHIP_TABLE_FAILURE 204
#define DATABASE_CREATE_MESSAGES_TABLE_FAILURE 205
#define DATABASE_CREATE_MESSAGE_RECIPIENTS_TABLE_FAILURE 206

// The user authentication result codes.
#define USER_AUTHENTICATION_SUCCESS 300
#define USER_AUTHENTICATION_FAILURE 301
#define USER_AUTHENTICATION_DATABASE_CONNECTION_FAILURE 302
#define USER_AUTHENTICATION_USERNAME_RECEIVE_FAILURE 303
#define USER_AUTHENTICATION_USERNAME_QUERY_FAILURE 304
#define USER_AUTHENTICATION_PASSWORD_RECEIVE_FAILURE 305
#define USER_AUTHENTICATION_PASSWORD_QUERY_FAILURE 306
#define USER_AUTHENTICATION_PASSWORD_SUCCESS 307
#define USER_AUTHENTICATION_CHOICE_RECEIVE_FAILURE 308
#define USER_AUTHENTICATION_PASSWORD_CONFIRMATION_RECEIVE_FAILURE 309
#define USER_AUTHENTICATION_USER_CREATION_QUERY_FAILURE 310
#define USER_AUTHENTICATION_USER_CREATION_FAILURE 311
#define USER_AUTHENTICATION_ATTEMPTS_EXCEEDED 312

/**
 * The singular request structure. This structure is used to store server connection data.
 *
 * @param timestamp The request timestamp.
 * @param address The request address.
 * @param socket The server returned socket.
 */
typedef struct
{
    char* timestamp;
    struct sockaddr_in address;
    int socket;
} request_t;

/**
 * The singular client structure. This structure is used to store information about a client connected to the server.
 *
 * @param request The client request.
 * @param id The ID of the client.
 * @param uid The unique ID of the client.
 */
typedef struct
{
    request_t* request;
    int id;
    int uid;
} client_t;

/**
 * The clients base structure. This structure is used to store information about all clients connected to the server.
 *
 * @param mutex The mutex to lock the clients array.
 * @param array The array of clients.
 */
struct clients_t
{
    pthread_mutex_t mutex;
    client_t* array[MAX_CLIENTS];
};

/**
 * Database connector. This function is used to connect to a SQLite3 database and create it with its tables if it does not exist.
 *
 * @param db The SQLite3 database.
 * @param db_name The name of the database.
 * @return The database connection exit code.
 */
int connect_db(sqlite3** db, char* db_name);

/**
 * Authenticates a request. This function is used to authenticate a user request. Returns the authentication result code.
 *
 * @param req The request to authenticate.
 */
int user_auth(request_t* req);

/**
 * Client handler. This function is used to handle a client connected to the server and is supposed to run in a separate thread.
 *
 * @param arg The client structure.
 */
void* handle_client(void* arg);

/**
 * Runs the server. This function is meant to be called by the main function of the server program.
 *
 * @return The exit code of the server.
 */
int run_server();

#endif
