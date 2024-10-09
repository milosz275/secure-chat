#ifndef __SERVER_H
#define __SERVER_H

#include <pthread.h>
#include <arpa/inet.h>
#include <sqlite3.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "protocol.h"
#include "sts_queue.h"
#include "hash_map.h"

#define MAX_CLIENTS 100
#define MAX_THREADS 100 // overrides max clients

#define DB_NAME "sqlite.db"
#define DB_PATH_LENGTH 256
#define USER_LOGIN_ATTEMPTS 3 // if you want to increase this number, you should add message codes for each attempt
#define SERVER_CLI_HISTORY "server_history.txt"

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

// Other
# define SINGLE_CORE_SYSTEM 1500

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
 * @param message_queue The message queue.
 * @param client_map The client hash map.
 * @param ssl_ctx The SSL context.
 * @param ssl The SSL object.
 * @param start_time The server start time.
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
    sts_header* message_queue;
    hash_map* client_map;
    SSL_CTX* ssl_ctx;
    SSL* ssl;
    time_t start_time;
};

/**
 * Print client. This function is used to print a client connection.
 *
 * @param cl The client connection pointer.
 */
void print_client(client_connection_t* cl);

/**
 * Send ping message. This function is used to send a ping message to a client.
 * The function is meant to be used with the hash map.
 *
 * @param cl The client connection pointer.
 */
void send_ping(client_connection_t* cl);

/**
 * Send quit signal. This function is used to send a quit signal to a client.
 * The function is meant to be used with the hash map.
 *
 * @param cl The client connection pointer.
 */
void send_quit_signal(client_connection_t* cl);

/**
 * Send broadcast message. This function is used to send a broadcast message to a client.
 * The function is meant to be used with the hash map.
 *
 * @param cl The client connection pointer.
 * @param arg The message to send.
 */
void send_broadcast(client_connection_t* cl, void* arg);

/**
 * Send join message. This function is used to send a join message to a client, as long as it is not the new client.
 * The function is meant to be used with the hash map.
 *
 * @param cl The client connection pointer.
 * @param arg The new client connection pointer.
 */
void send_join_message(client_connection_t* cl, void* arg);

/**
 * Client handler. This function is used to handle a request, create and connect user to the server.
 * The function is meant to be run in a separate thread.
 *
 * @param arg The request structure.
 */
void* handle_client(void* arg);

/**
 * Command line interface handler. This function is used to handle the server command line interface.
 * The function is meant to be run in a separate thread.
 *
 * @param arg Not used.
 */
void* handle_cli(void* arg);

/**
 * Message queue handler. This function is used to handle the message queue.
 * The function is meant to be run in a separate thread.
 *
 * @param arg Not used.
 */
void* handle_msg_queue(void* arg);

/**
 * Information update handler. This function is used to update the server information and log it.
 * The function is meant to be run in a separate thread.
 *
 * @param arg Not used.
 */
void* handle_info_update(void* arg);

/**
 * Runs the server. This function is meant to be called by the main function of the server program.
 *
 * @return The exit code of the server.
 */
int run_server();

#endif
