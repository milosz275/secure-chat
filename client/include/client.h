#ifndef __CLIENT_H
#define __CLIENT_H

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "protocol.h"

#define SERVER_RECONNECTION_INTERVAL 1
#define SERVER_RECONNECTION_ATTEMPTS 120

#define MAX_INPUT_LENGTH 128
#define MAX_MESSAGE_LENGTH 147

/**
 * The client structure. This structure is used to define the client structure.
 *
 * @param socket The client socket.
 * @param uid The client unique ID.
 * @param username The client username.
 * @param ssl_ctx The SSL context.
 * @param ssl The SSL object.
 * @param input The client input.
 */
typedef struct client_t
{
    int socket;
    char* uid;
    char username[MAX_USERNAME_LENGTH + 1];
    SSL_CTX* ssl_ctx;
    SSL* ssl;
    char input[MAX_INPUT_LENGTH];
} client_t;

/**
 * Connect to the server. This function is used to connect to the server.
 *
 * @param server_addr The server address.
 * @return The exit code.
 */
int connect_to_server(struct sockaddr_in* server_addr);

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
