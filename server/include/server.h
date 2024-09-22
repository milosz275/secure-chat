#ifndef __SERVER_H
#define __SERVER_H

#include <pthread.h>
#include <arpa/inet.h>
#include <sqlite3.h>

#define MAX_CLIENTS 100

/**
 * The database connection exit codes.
 */
#define DATABASE_CONNECTION_SUCCESS 200
#define DATABASE_OPEN_FAILURE 201
#define DATABASE_CREATE_USERS_TABLE_FAILURE 202
#define DATABASE_CREATE_MESSAGES_TABLE_FAILURE 203

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
int connect_db(sqlite3 **db, char *db_name);

/**
 * Client handler. This function is used to handle a client connected to the server and is supposed to run in a separate thread.
 *
 * @param arg The client structure.
 */
void *handle_client(void *arg);

/**
 * Runs the server. This function is meant to be called by the main function of the server program.
 * 
 * @return The exit code of the server.
 */
int run_server();

#endif
