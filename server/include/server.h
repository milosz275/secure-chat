#ifndef __SERVER_H
#define __SERVER_H

#include <pthread.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 100

/**
 * The singular client structure. This structure is used to store information about a client connected to the server.
 * 
 * @param address The address of the client.
 * @param socket The socket of the client.
 * @param id The ID of the client.
 * @param uid The unique ID of the client.
 */
typedef struct
{
    struct sockaddr_in address;
    int socket;
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
    client_t *array[MAX_CLIENTS];
};

/**
 * Client handler. This function is used to handle a client connected to the server and is supposed to run in a separate thread.
 * 
 * @param arg The client structure.
 */
void *handle_client(void *arg);

/**
 * Run the server. This function is meant to be called by the main function of the server program.
 */
void run_server();

#endif
