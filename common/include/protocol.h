#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>

#define TIMESTAMP_LENGTH 20

// The message creation result codes. These are used to determine the exit code of the create_message function.
#define MESSAGE_CREATED 100
#define INVALID_UID_LENGTH 101
#define INVALID_MESSAGE_LENGTH 102

// The server port and message buffer size. These are used to define the port and buffer size for the server and client.
#define PORT 12345
#define BUFFER_SIZE 4096
#define CLIENT_HASH_LENGTH 64
#define MAX_MES_SIZE (BUFFER_SIZE - CLIENT_HASH_LENGTH - 1)

/**
 * The message structure. This structure is used to determine structure of sent and received data between the client and the server.
 *
 * @param recipient_uid The recipient hash, unique identifier.
 * @param message The message.
 */
typedef struct
{
    char recipient_uid[CLIENT_HASH_LENGTH];
    char message[MAX_MES_SIZE];
} message_t;

/**
 * Get the hash of a password. This function is used to get the hash of a password using SHA-256 from the OpenSSL library.
 *
 * @param password The password.
 * @return The hash of the password.
 */
const unsigned char* get_hash(const char* password);

/**
 * Get the current timestamp. This function is used to get the current timestamp using time.h.
 *
 * @return The current timestamp.
 */
const char* get_timestamp();

/**
 * Generate a unique ID. This function is used to generate a unique ID based on the text and the current timestamp.
 *
 * @param text The text.
 * @param hash_length The length of the hash.
 * @return The unique ID.
 */
const char* generate_uid(const char* text, int hash_length);

/**
 * Generate a unique user ID. This function is used to generate a unique user ID based on the username.
 *
 * @param username The username.
 * @return The unique user ID.
 */
const char* generate_unique_user_id(const char* username);

/**
 * Interrupt handler. This function is used to handle the SIGINT signal and is supposed to be called by the signal function.
 *
 * @param sig The signal number.
 */
void int_handler(int sig);

#endif
