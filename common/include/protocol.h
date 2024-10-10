#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>

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
#define MESSAGE_SEND_RETRY 2302

#define MESSAGE_TYPE_UNKNOWN "UNKNOWN"
#define MESSAGE_CODE_UNKNOWN "UNKNOWN"

#define MESSAGE_SIGNAL_QUIT "QUIT"
#define MESSAGE_SIGNAL_EXIT "EXIT"

#define CLIENT_DEFAULT_NAME "client"

/**
 * The message type enumeration. This enumeration is used to define the type of message that is being sent.
 *
 * @param MESSAGE_TEXT Regular chat plaintext message
 * @param MESSAGE_TOAST Toast message, a full-screen notification message
 * @param MESSAGE_CHOICE User choice message
 * @param MESSAGE_NOTIFICATION Notification message
 * @param MESSAGE_BROADCAST Broadcast message, system-wide message
 * @param MESSAGE_TEXT_RSA_ENCRYPTED Encrypted chat message (RSA)
 * @param MESSAGE_TEXT_AES_ENCRYPTED Encrypted chat message (AES)
 * @param MESSAGE_PING Ping message (check if client/server is alive)
 * @param MESSAGE_ACK Acknowledgment of a message
 * @param MESSAGE_SIGNAL Control signal (server sending commands like shutdown or to poll database)
 * @param MESSAGE_AUTH Authentication request/response
 * @param MESSAGE_AUTH_ATTEMPS Current authentication attempts count
 * @param MESSAGE_ERROR Error response
 * @param MESSAGE_USER_JOIN User joined server
 * @param MESSAGE_USER_LEAVE User left server
 * @param MESSAGE_SYSTEM Server maintenance message
 * @return The message type enumeration.
 */
typedef int32_t message_type_t;
enum
{
    MESSAGE_TEXT = 1234,
    MESSAGE_TOAST,
    MESSAGE_CHOICE,
    MESSAGE_NOTIFICATION,
    MESSAGE_BROADCAST,
    MESSAGE_TEXT_RSA_ENCRYPTED,
    MESSAGE_TEXT_AES_ENCRYPTED,
    MESSAGE_PING,
    MESSAGE_ACK,
    MESSAGE_SIGNAL,
    MESSAGE_AUTH,
    MESSAGE_AUTH_ATTEMPS,
    MESSAGE_UID,
    MESSAGE_ERROR,
    MESSAGE_USER_JOIN,
    MESSAGE_USER_LEAVE,
    MESSAGE_SYSTEM,
};

/**
 * The message code enumeration. This enumeration is used to define the message codes that are used for message specification and control.
 *
 * @param MESSAGE_CODE_WELCOME Welcome message
 * @param MESSAGE_CODE_ENTER_USERNAME Enter your username
 * @param MESSAGE_CODE_ENTER_PASSWORD Enter your password
 * @param MESSAGE_CODE_ENTER_PASSWORD_CONFIRMATION Confirm your password
 * @param MESSAGE_CODE_INVALID_USERNAME Invalid username
 * @param MESSAGE_CODE_INVALID_PASSWORD Invalid password
 * @param MESSAGE_CODE_PASSWORDS_DO_NOT_MATCH Passwords do not match
 * @param MESSAGE_CODE_TRY_AGAIN Try again
 * @param MESSAGE_CODE_USER_JOIN User joined
 * @param MESSAGE_CODE_USER_LEAVE User left
 * @param MESSAGE_CODE_USER_DOES_NOT_EXIST User does not exist
 * @param MESSAGE_CODE_USER_ALREADY_ONLINE User already online
 * @param MESSAGE_CODE_USER_REGISTER_INFO User registration information
 * @param MESSAGE_CODE_USER_REGISTER_CHOICE User registration choice
 * @param MESSAGE_CODE_USER_CREATED User created
 * @param MESSAGE_CODE_USER_AUTHENTICATED User authenticated
 * @param MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_THREE User authentication 3 attempts left
 * @param MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_TWO User authentication 2 attempts left
 * @param MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_ONE User authentication 1 attempt left
 * @param MESSAGE_CODE_USER_AUTHENTICATION_FAILURE User authentication failed
 * @param MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_EXCEEDED User authentication attempts exceeded
 * @param MESSAGE_CODE_USER_AUTHENTICATION_SUCCESS User authentication success
 * @param MESSAGE_CODE_UID Unique message ID
 * @param MESSAGE_CODE_UNKNOWN Unknown message code
 */
typedef int32_t message_code_t;
enum
{
    MESSAGE_CODE_WELCOME = 2345,
    MESSAGE_CODE_ENTER_USERNAME,
    MESSAGE_CODE_ENTER_PASSWORD,
    MESSAGE_CODE_ENTER_PASSWORD_CONFIRMATION,
    MESSAGE_CODE_INVALID_USERNAME,
    MESSAGE_CODE_INVALID_PASSWORD,
    MESSAGE_CODE_PASSWORDS_DO_NOT_MATCH,
    MESSAGE_CODE_TRY_AGAIN,
    MESSAGE_CODE_USER_JOIN,
    MESSAGE_CODE_USER_LEAVE,
    MESSAGE_CODE_USER_DOES_NOT_EXIST,
    MESSAGE_CODE_USER_ALREADY_ONLINE,
    MESSAGE_CODE_USER_REGISTER_INFO,
    MESSAGE_CODE_USER_REGISTER_CHOICE,
    MESSAGE_CODE_USER_CREATED,
    MESSAGE_CODE_USER_AUTHENTICATED,
    MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_THREE,
    MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_TWO,
    MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_ONE,
    MESSAGE_CODE_USER_AUTHENTICATION_FAILURE,
    MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_EXCEEDED,
    MESSAGE_CODE_USER_AUTHENTICATION_SUCCESS,
    MESSAGE_CODE_UID,
};

/**
 * The message structure. This structure is used to store message data.
 *
 * @param message_uid The unique message ID.
 * @param type The type of the message.
 * @param sender_uid The sender's unique ID.
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
 * The singular request structure. This structure is used to store server connection data.
 *
 * @param address The request address.
 * @param socket The server returned socket.
 * @param ssl The SSL object for the connection.
 */
typedef struct
{
    struct sockaddr_in address;
    int socket;
    SSL* ssl;
} request_t;

/**
 * The singular client structure. This structure is used to store information about a client connected to the server.
 *
 * @param request The client request.
 * @param id The ID of the client.
 * @param uid The unique ID of the client.
 * @param username The username of the client.
 * @param is_ready The client readiness status.
 * @param is_inserted The client hash map insertion status.
 */
typedef struct client_connection_t
{
    request_t* request;
    int id;
    char* uid;
    char username[MAX_USERNAME_LENGTH + 1];
    int is_ready;
    int is_inserted;
} client_connection_t;

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
 * Send a message. This function is used to send a message using a secure SSL connection.
 *
 * @param ssl The SSL object.
 * @param msg The message to send.
 * @return The message send result code.
 */
int send_message(SSL* ssl, message_t* msg);

/**
 * Get the message type as a text string. This function is used to get the message type as a text string, for example "MESSAGE_TOAST".
 *
 * @param type The message type.
 * @return The message type as a text string.
 */
const char* message_type_to_text(message_type_t type);

/**
 * Get the message code as a text string. This function is used to get the message code as a text string, for example "Welcome!".
 *
 * @param code The message code.
 * @return The message code as a text string.
 */
const char* message_code_to_text(message_code_t code);

/**
 * Get the message code as a string. This function is used to get the message code as a string, for example "1234".
 *
 * @param code The message code.
 * @return The message code as a string.
 */
const char* message_code_to_string(message_code_t code);

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

/**
 * Clear the command line interface. This function is used to clear the command line interface.
 */
void clear_cli();

#endif
