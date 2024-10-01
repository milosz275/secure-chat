#ifndef __SERVER_H
#define __SERVER_H

#include <pthread.h>
#include <arpa/inet.h>
#include <sqlite3.h>
#include "protocol.h"

#define MAX_CLIENTS 100
#define MAX_THREADS 100 // overrides max clients

#define DB_NAME "sqlite.db"
#define DB_PATH_LENGTH 256
#define USER_LOGIN_ATTEMPTS 3 // if you want to increase this number, you should add message codes for each attempt

#define PORT_BIND_INTERVAL 1
#define PORT_BIND_ATTEMPTS 120

#define DATABASE_CONNECTION_INTERVAL 5 
#define DATABASE_CONNECTION_ATTEMPTS 10 

// The database connection result codes.
#define DATABASE_CONNECTION_SUCCESS 1200
#define DATABASE_OPEN_FAILURE 1201
#define DATABASE_SETUP_FAILURE 1202

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

# define SINGLE_CORE_SYSTEM 1500

/**
 * The singular request structure. This structure is used to store server connection data.
 *
 * @param address The request address.
 * @param socket The server returned socket.
 */
typedef struct
{
    struct sockaddr_in address;
    int socket;
} request_t;

/**
 * The singular client structure. This structure is used to store information about a client connected to the server.
 *
 * @param request The client request.
 * @param id The ID of the client.
 * @param uid The unique ID of the client.
 * @param username The username of the client.
 * @param is_ready The client readiness status.
 */
typedef struct
{
    request_t* request;
    int id;
    char* uid;
    char username[MAX_USERNAME_LENGTH + 1];
    int is_ready;
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
 * The server structure. This structure is used to store information about the server.
 *
 * @param socket The server socket.
 * @param address The server address.
 * @param requests_handled The number of requests handled.
 * @param client_logins_handled The number of client logins handled.
 * @param thread_count The number of all allocated threads.
 * @param thread_count_mutex The mutex to lock the thread count.
 * @param threads The array of threads.
 */
struct server_t
{
    int socket;
    struct sockaddr_in address;
    int requests_handled;
    int client_logins_handled;
    int thread_count;
    pthread_mutex_t thread_count_mutex;
    pthread_t threads[MAX_CLIENTS];
};

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
