#include "protocol.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <openssl/evp.h>

int create_message(message_t* msg, message_type_t type, char* sender_uid, char* recipient_uid, char* payload)
{
    if (msg == NULL || sender_uid == NULL || recipient_uid == NULL || payload == NULL)
        return MESSAGE_CREATION_FAILURE;
    if (strlen(payload) > MAX_PAYLOAD_SIZE)
        return MESSAGE_CREATION_PAYLOAD_SIZE_EXCEEDED;
    else if (strlen(sender_uid) > USERNAME_HASH_LENGTH || strlen(recipient_uid) > USERNAME_HASH_LENGTH)
        return MESSAGE_CREATION_USERNAME_SIZE_EXCEEDED;
    else if (strlen(payload) == 0)
        return MESSAGE_CREATION_PAYLOAD_EMPTY;

    msg->payload[0] = '\0';
    char* uid = generate_uid(payload, MESSAGE_HASH_LENGTH);
    if (uid == NULL)
        return MESSAGE_CREATION_FAILURE;
    snprintf(msg->message_uid, MESSAGE_HASH_LENGTH, "%s", uid);
    msg->type = type;
    snprintf(msg->sender_uid, USERNAME_HASH_LENGTH, "%s", sender_uid);
    snprintf(msg->recipient_uid, USERNAME_HASH_LENGTH, "%s", recipient_uid);
    msg->payload_length = strlen(payload);
    snprintf(msg->payload, MAX_PAYLOAD_SIZE, "%s", payload);
    msg->payload[msg->payload_length] = '\0';

    return MESSAGE_CREATION_SUCCESS;
}

void parse_message(message_t* msg, const char* buffer)
{
    sscanf(buffer, "%63[^|]|%d|%63[^|]|%63[^|]|%u|%1023[^\n]",
        msg->message_uid, (int*)&msg->type, msg->sender_uid, msg->recipient_uid, &msg->payload_length, msg->payload);
    msg->payload[msg->payload_length] = '\0';
}

int send_message(int socket, message_t* msg)
{
    if (msg == NULL)
        return MESSAGE_SEND_FAILURE;

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, BUFFER_SIZE, "%s%s%d%s%s%s%s%s%u%s%s",
        msg->message_uid, MESSAGE_DELIMITER,
        msg->type, MESSAGE_DELIMITER,
        msg->sender_uid, MESSAGE_DELIMITER,
        msg->recipient_uid, MESSAGE_DELIMITER,
        msg->payload_length, MESSAGE_DELIMITER,
        msg->payload);
    send(socket, buffer, strlen(buffer), 0);
    msg->payload[0] = '\0';
    msg->payload_length = 0;

    return MESSAGE_SEND_SUCCESS;
}

const char* message_type_to_string(message_type_t type)
{
    switch (type)
    {
    case MESSAGE_TEXT: return "MESSAGE_TEXT";
    case MESSAGE_PING: return "MESSAGE_PING";
    case MESSAGE_SIGNAL: return "MESSAGE_SIGNAL";
    case MESSAGE_AUTH: return "MESSAGE_AUTH";
    case MESSAGE_ACK: return "MESSAGE_ACK";
    case MESSAGE_ERROR: return "MESSAGE_ERROR";
    case MESSAGE_USER_JOIN: return "MESSAGE_USER_JOIN";
    case MESSAGE_USER_LEAVE: return "MESSAGE_USER_LEAVE";
    case MESSAGE_GROUP: return "MESSAGE_GROUP";
    case MESSAGE_SYSTEM: return "MESSAGE_SYSTEM";
    default: return "UNKNOWN";
    }
}

unsigned char* get_hash(const char* input)
{
    unsigned char* hash = (unsigned char*)malloc(EVP_MAX_MD_SIZE);
    if (hash == NULL)
        return NULL;

    unsigned int length = 0;
    EVP_Digest(input, strlen(input), hash, &length, EVP_sha256(), NULL);
    if (length <= 0)
    {
        free((void*)hash);
        return NULL;
    }
    hash[length] = '\0';

    return hash;
}

char* generate_password_hash(const char* password)
{
    int hash_length = PASSWORD_HASH_LENGTH;
    const unsigned char* hash = get_hash(password);

    if (hash == NULL || hash_length <= 0 || hash_length > EVP_MAX_MD_SIZE)
        return NULL;

    int actual_hash_len = strlen((const char*)hash);

    if (actual_hash_len < hash_length)
    {
        unsigned char* extended_hash = (unsigned char*)malloc(hash_length);
        if (extended_hash == NULL)
        {
            free((void*)hash);
            return NULL;
        }

        for (int i = 0; i < hash_length; ++i)
            extended_hash[i] = hash[i % actual_hash_len];

        free((void*)hash);
        hash = extended_hash;
    }

    char* hash_str = (char*)malloc((hash_length * 2) + 1);
    if (hash_str == NULL)
    {
        free((void*)hash);
        return NULL;
    }

    for (int i = 0; i < hash_length; ++i)
        snprintf(hash_str + (i * 2), 3, "%02x", hash[i]);

    hash_str[hash_length * 2] = '\0';

    free((void*)hash);
    return hash_str;
}


const char* get_timestamp()
{
    static char timestamp_str[TIMESTAMP_LENGTH];
    time_t now = time(NULL);
    strftime(timestamp_str, sizeof(timestamp_str), "%Y%m%d%H%M%S", localtime(&now));
    return timestamp_str;
}

const char* get_formatted_timestamp()
{
    static char timestamp_str[TIMESTAMP_LENGTH];
    time_t now = time(NULL);
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return timestamp_str;
}

char* generate_uid(const char* text, int hash_length)
{
    if (text == NULL || hash_length == 0)
        return NULL;

    char input_str[BUFFER_SIZE];
    char timestamp[TIMESTAMP_LENGTH];
    get_timestamp(timestamp, sizeof(timestamp));

    snprintf(input_str, BUFFER_SIZE, "%s%s", text, timestamp);

    const unsigned char* hash = get_hash(input_str);
    if (hash == NULL || hash_length <= 0 || hash_length > EVP_MAX_MD_SIZE)
        return NULL;

    static char uid[BUFFER_SIZE];
    for (int i = 0; i < hash_length; ++i)
        snprintf(uid + (i * 2), 3, "%02x", hash[i]);

    uid[hash_length * 2] = '\0';
    free((void*)hash);

    return uid;
}

char* generate_unique_user_id(const char* username)
{
    return generate_uid(username, USERNAME_HASH_LENGTH);
}

void format_uptime(long seconds, char* buffer, size_t buffer_size)
{
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    snprintf(buffer, buffer_size, "%02d:%02d:%02d", hours, minutes, secs);
}
