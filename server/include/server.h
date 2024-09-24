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
#define DATABASE_CONNECTION_SUCCESS 1200
#define DATABASE_OPEN_FAILURE 1201

// The database setup result codes.
#define DATABASE_CREATE_SUCCESS 1300
#define DATABASE_CREATE_USERS_TABLE_FAILURE 1301
#define DATABASE_CREATE_GROUPS_TABLE_FAILURE 1302
#define DATABASE_CREATE_GROUP_MEMBERSHIP_TABLE_FAILURE 1303
#define DATABASE_CREATE_MESSAGES_TABLE_FAILURE 1304
#define DATABASE_CREATE_MESSAGE_RECIPIENTS_TABLE_FAILURE 1305

// The user authentication result codes.
#define USER_AUTHENTICATION_SUCCESS 1400
#define USER_AUTHENTICATION_FAILURE 1401
#define USER_AUTHENTICATION_DATABASE_CONNECTION_FAILURE 1402
#define USER_AUTHENTICATION_USERNAME_RECEIVE_FAILURE 1403
#define USER_AUTHENTICATION_USERNAME_QUERY_FAILURE 1404
#define USER_AUTHENTICATION_PASSWORD_RECEIVE_FAILURE 1405
#define USER_AUTHENTICATION_PASSWORD_QUERY_FAILURE 1406
#define USER_AUTHENTICATION_PASSWORD_SUCCESS 1407
#define USER_AUTHENTICATION_CHOICE_RECEIVE_FAILURE 1408
#define USER_AUTHENTICATION_PASSWORD_CONFIRMATION_RECEIVE_FAILURE 1409
#define USER_AUTHENTICATION_USER_CREATION_QUERY_FAILURE 1410
#define USER_AUTHENTICATION_USER_CREATION_FAILURE 1411
#define USER_AUTHENTICATION_ATTEMPTS_EXCEEDED 1412
#define USER_AUTHENTICATION_PASSWORD_MISMATCH 1413
#define USER_AUTHENTICATION_USERNAME_DOES_NOT_EXIST 1414
#define USER_AUTHENTICATION_INVALID_CHOICE 1415
#define USER_AUTHENTICATION_USER_AUTHENTICATION_QUERY_FAILURE 1416
#define USER_AUTHENTICATION_USER_AUTHENTICATION_FAILURE 1417
#define USER_AUTHENTICATION_MEMORY_ALLOCATION_FAILURE 1418
#define USER_AUTHENTICATION_LAST_LOGIN_UPDATE_FAILURE 1419

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
 * @param timestamp The client creation timestamp.
 * @param id The ID of the client.
 * @param uid The unique ID of the client.
 */
typedef struct
{
    request_t* request;
    char* timestamp;
    int id;
    char* uid;
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
 * Database setup. This function is used to set up a SQLite3 database and create its tables if they do not exist.
 *
 * @param db The SQLite3 database.
 * @param db_name The name of the database.
 * @return The database setup exit code.
 */
int setup_db(sqlite3** db, char* db_name);

/**
 * Authenticates a request. This function is used to authenticate a user request. Returns the authentication result code.
 *
 * @param req The request to authenticate.
 * @param uid The obtained unique ID of the client.
 */
int user_auth(request_t* req, client_t* cl);

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
