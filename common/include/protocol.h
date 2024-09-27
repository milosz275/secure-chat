#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#define TIMESTAMP_LENGTH 20

// The server port and message components size. These are used to define the port and buffer size for the server and client.
#define PORT 12345
#define BUFFER_SIZE 4096
#define HASH_LENGTH 64 // SHA-512 hash (64 bytes)
#define HASH_HEX_OUTPUT_LENGTH (HASH_LENGTH * 2 + 1) // hex output length (2 chars per byte + \0) for SHA-512 it will be 129 (128 characters)
#define MAX_PAYLOAD_SIZE 2048 // 2 KB
#define MAX_USERNAME_LENGTH 16
#define MAX_PASSWORD_LENGTH 16
#define MAX_GROUP_NAME_LENGTH 16
#define MAX_GROUP_MEMBERS 16
#define MESSAGE_DELIMITER "|"
#define TIMESTAMP_LENGTH 20

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
#define MESSAGE_CREATION_PAYLOAD_SIZE_EXCEEDED 2109
#define MESSAGE_CREATION_USERNAME_SIZE_EXCEEDED 2110
#define MESSAGE_CREATION_PAYLOAD_EMPTY 2111
#define MESSAGE_CREATION_HASH_FAILURE 2112

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
    char message_uid[HASH_HEX_OUTPUT_LENGTH];
    message_type_t type;
    char sender_uid[HASH_HEX_OUTPUT_LENGTH];
    char recipient_uid[HASH_HEX_OUTPUT_LENGTH];
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
int create_message(message_t* msg, message_type_t type, const char* sender_uid, const char* recipient_uid, const char* payload);

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
 * Get the hash of an input. Calculates the SHA-512 hash of the input string and converts it to a hex string in a thread-safe manner.
 *
 * @param input The input string for which the hash will be calculated.
 * @param output Buffer to store the resulting hex string (must be at least HASH_HEX_OUTPUT_LENGTH).
 * @returns Exit code of the function.
 */
int get_hash(const unsigned char* input, char* output);

/**
 * Get the timestamp. This function is used to get the timestamp in a YYYYMMDDHHMMSS format.
 *
 * @param buffer The buffer to store the timestamp.
 * @param buffer_size The size of the buffer.
 */
void get_timestamp(char* buffer, size_t buffer_size);

/**
 * Get the formatted timestamp. This function is used to get the formatted timestamp in a YYYY-MM-DD HH:MM:SS format.
 *
 * @param buffer The buffer to store the formatted timestamp.
 * @param buffer_size The size of the buffer.
 */
void get_formatted_timestamp(char* buffer, size_t buffer_size);

/**
 * Format the uptime. This function is used to format the uptime in a hh:mm:ss format.
 *
 * @param seconds The number of seconds.
 * @param buffer The buffer to store the formatted uptime.
 * @param buffer_size The size of the buffer.
 */
void format_uptime(long seconds, char* buffer, size_t buffer_size);

#endif
