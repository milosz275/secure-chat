#ifndef __CLIENT_MSG_HANDLER_H
#define __CLIENT_MSG_HANDLER_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include <signal.h>
#include <raylib.h>

#include "protocol.h"
#include "client.h"
#include "client_gui.h"

#define SERVER_RECONNECTION_INTERVAL 1
#define SERVER_RECONNECTION_ATTEMPTS 120

#define MAX_INPUT_LENGTH 128
#define MAX_MESSAGE_LENGTH 147

/**
 * Handle a message. This function is used to handle a message received from the server.
 *
 * @param msg The message to handle.
 * @param client The client structure.
 * @param state The client state structure.
 * @param reconnect_flag The reconnect flag.
 * @param quit_flag The quit flag.
 * @param server_answer The server answer flag.
 * @param log_filename The log filename.
 */
void handle_message(message_t* msg, client_t* client, client_state_t* state, volatile sig_atomic_t* reconnect_flag, volatile sig_atomic_t* quit_flag, volatile sig_atomic_t* server_answer, const char* log_filename);

#endif
