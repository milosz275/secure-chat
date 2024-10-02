#ifndef __CLIENT_H
#define __CLIENT_H

#include "protocol.h"

#define SERVER_RECONNECTION_INTERVAL 1
#define SERVER_RECONNECTION_ATTEMPTS 120

/**
 * The client structure. This structure is used to define the client structure.
 *
 * @param socket The client socket.
 * @param uid The client unique ID.
 * @param username The client username.
 * @param is_authenticated The client authentication status.
 */
typedef struct client_t
{
    int socket;
    char* uid;
    char username[MAX_USERNAME_LENGTH + 1];
    int is_authenticated;
} client_t;

/**
 * Receive messages from the server. This function is meant to be run in a separate thread and handle the reception of messages from the server.
 *
 * @param arg The socket descriptor.
 */
void* receive_messages(void* arg);

/**
 * Run the client. This function is meant to be called by the main function of the client program.
 */
void run_client();

#endif
