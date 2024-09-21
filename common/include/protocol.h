#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>

#define LOGIN 1
#define MESSAGE 2
#define PRIVATE_MESSAGE 3
#define LOGOUT 4
#define ERROR 5

#define PORT 12345
#define BUFFER_SIZE 1024
#define CLIENT_HASH_LENGTH 64

/**
 * The message structure. This structure is used to determine structure of sent and received data between the client and the server.
 * 
 * @param recipient_uid The recipient hash, unique identifier.
 * @param message The message.
 */
typedef struct
{
    char recipient_uid[CLIENT_HASH_LENGTH];
    char message[BUFFER_SIZE - CLIENT_HASH_LENGTH - 1];
} message_t;

/**
 * Get the hash of a password. This function is used to get the hash of a password using SHA-256 from the OpenSSL library.
 * 
 * @param password The password.
 * @return The hash of the password.
 */
const unsigned char *get_hash(const char *password);

/**
 * Get the current timestamp. This function is used to get the current timestamp using time.h.
 * 
 * @return The current timestamp.
 */
const char *get_timestamp();

/**
 * Generate a unique user ID. This function is used to generate a unique user ID based on the username.
 * 
 * @param username The username.
 * @return The unique user ID.
 */
const char *generate_unique_user_id(const char *username);

#endif
