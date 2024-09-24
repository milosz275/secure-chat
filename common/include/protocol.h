#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>

#define TIMESTAMP_LENGTH 20

// The server port and message components size. These are used to define the port and buffer size for the server and client.
#define PORT 12345
#define BUFFER_SIZE 4096
#define MESSAGE_HASH_LENGTH 32
#define USERNAME_HASH_LENGTH 32
#define PASSWORD_HASH_LENGTH 32
#define MAX_PAYLOAD_SIZE 2048
#define MAX_USERNAME_LENGTH 16
#define MAX_PASSWORD_LENGTH 16
#define MAX_GROUP_NAME_LENGTH 16
#define MAX_GROUP_MEMBERS 16
#define MESSAGE_DELIMITER "|"

// The message creation result codes. These are used to determine the exit code of the create_message function.
#define MESSAGE_CREATION_SUCCESS 2100
#define MESSAGE_CREATION_FAILURE 2101
#define MESSAGE_CREATION_INVALID_UID_LENGTH 2102
#define MESSAGE_CREATION_INVALID_MESSAGE 2103
#define MESSAGE_CREATION_INVALID_MESSAGE_LENGTH 2104
#define MESSAGE_CREATION_INVALID_RECIPIENT_UID 2105
#define MESSAGE_CREATION_INVALID_RECIPIENT_UID_LENGTH 2106
#define MESSAGE_CREATION_INVALID_PAYLOAD 2107
#define MESSAGE_CREATION_INVALID_PAYLOAD_LENGTH 2108

// The message parsing result codes. These are used to determine the exit code of the parse_message function.
#define MESSAGE_PARSING_SUCCESS 2200
#define MESSAGE_PARSING_FAILURE 2201
#define MESSAGE_PARSING_INVALID_MESSAGE 2202
#define MESSAGE_PARSING_INVALID_MESSAGE_LENGTH 2203
#define MESSAGE_PARSING_INVALID_MESSAGE_UID 2204
#define MESSAGE_PARSING_INVALID_MESSAGE_UID_LENGTH 2205
#define MESSAGE_PARSING_INVALID_MESSAGE_TYPE 2206
#define MESSAGE_PARSING_INVALID_SENDER_UID 2207
#define MESSAGE_PARSING_INVALID_SENDER_UID_LENGTH 2208
#define MESSAGE_PARSING_INVALID_RECIPIENT_UID 2209
#define MESSAGE_PARSING_INVALID_RECIPIENT_UID_LENGTH 2210

// The message send result codes. These are used to determine the exit code of the send_message function.
#define MESSAGE_SEND_SUCCESS 2300
#define MESSAGE_SEND_FAILURE 2301

/**
 * The message type enumeration. This enumeration is used to define the type of message that is being sent.
 *
 * @param MESSAGE_TEXT Regular chat message
 * @param MESSAGE_PING Ping message (check if client/server is alive)
 * @param MESSAGE_SIGNAL Control signal (server sending commands like shutdown or poll database)
 * @param MESSAGE_AUTH Authentication request/response
 * @param MESSAGE_ACK Acknowledgment of message receipt
 * @param MESSAGE_ERROR Error response
 * @param MESSAGE_USER_JOIN User joined a group or session
 * @param MESSAGE_USER_LEAVE User left a group or session
 * @param MESSAGE_GROUP Group-related message (create, join, leave)
 * @param MESSAGE_SYSTEM Server maintenance or system-wide messages
 * @return The message type enumeration.
 */
typedef enum
{
    MESSAGE_TEXT,
    MESSAGE_PING,
    MESSAGE_SIGNAL,
    MESSAGE_AUTH,
    MESSAGE_ACK,
    MESSAGE_ERROR,
    MESSAGE_USER_JOIN,
    MESSAGE_USER_LEAVE,
    MESSAGE_GROUP,
    MESSAGE_SYSTEM,
} message_type_t;

/**
 * The message structure. This structure is used to store message data.
 *
 * @param message_uid The unique message ID.
 * @param type The type of the message.
 * @param sender_username The sender's username.
 * @param recipient_uid The recipient's unique ID (could be a user or group UID).
 * @param payload_length The length of the payload.
 * @param payload The message content or control data.
 * @return The message structure.
 */
typedef struct
{
    char message_uid[MESSAGE_HASH_LENGTH];
    message_type_t type;
    char sender_uid[USERNAME_HASH_LENGTH];
    char recipient_uid[USERNAME_HASH_LENGTH];
    uint32_t payload_length;
    char payload[MAX_PAYLOAD_SIZE];
} message_t;

/**
 * Create a message. This function is used to create a message structure based on the message type, sender, recipient, and payload.
 *
 * @param type The type of the message.
 * @param sender_uid The sender's unique ID.
 * @param recipient_uid The recipient's unique ID (could be a user or group UID).
 * @param payload The message content or control data.
 * @return The message creation result code.
 */
int create_message(message_t* msg, message_type_t type, char* sender_uid, char* recipient_uid, char* payload);

/**
 * Parse a message. This function is used to parse a message structure and return the message content.
 *
 * @param msg The message structure.
 * @param buffer The message buffer.
 * @return The message parsing result code.
 */
void parse_message(message_t* msg, const char* buffer);

/**
 * Send a message. This function is used to send a message using a socket.
 *
 * @param socket The socket to send the message to.
 * @param msg The message to send.
 * @return The message send result code.
 */
int send_message(int socket, message_t* msg);

/**
 * Get the message type as a string. This function is used to get the message type as a string.
 *
 * @param type The message type.
 * @return The message type as a string.
 */
const char* message_type_to_string(message_type_t type);

/**
 * Get the hash of an input. This function is used to get the hash of given input using SHA-256 from the OpenSSL library.
 *
 * @param input The input to hash.
 * @return The hash of the password.
 */
unsigned char* get_hash(const char* input);

/**
 * Generate a password hash. This function is used to generate a password hash using SHA-256 from the OpenSSL library.
 *
 * @param password The password to hash.
 * @return The password hash.
 */
char* generate_password_hash(const char* password);

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
char* generate_uid(const char* text, int hash_length);

/**
 * Generate a unique user ID. This function is used to generate a unique user ID based on the username.
 *
 * @param username The username.
 * @return The unique user ID.
 */
char* generate_unique_user_id(const char* username);

/**
 * Interrupt handler. This function is used to handle the SIGINT signal and is supposed to be called by the signal function.
 *
 * @param sig The signal number.
 */
void int_handler(int sig);

#endif
