#include "protocol.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <openssl/evp.h>

struct tm* localtime_r(const time_t* timer, struct tm* buf);

int create_message(message_t* msg, message_type_t type, const char* sender_uid, const char* recipient_uid, const char* payload)
{
    if (msg == NULL || sender_uid == NULL || recipient_uid == NULL || payload == NULL)
        return MESSAGE_CREATION_FAILURE;
    if (strlen(payload) > MAX_PAYLOAD_SIZE)
        return MESSAGE_CREATION_PAYLOAD_SIZE_EXCEEDED;
    else if (strlen(sender_uid) > HASH_HEX_OUTPUT_LENGTH || strlen(recipient_uid) > HASH_HEX_OUTPUT_LENGTH)
        return MESSAGE_CREATION_USERNAME_SIZE_EXCEEDED;
    else if (strlen(payload) == 0)
        return MESSAGE_CREATION_PAYLOAD_EMPTY;

    // clear message structure
    msg->message_uid[0] = '\0';
    msg->sender_uid[0] = '\0';
    msg->recipient_uid[0] = '\0';
    msg->payload[0] = '\0';

    // message uid is hash of the timestamp and payload
    char uid[HASH_HEX_OUTPUT_LENGTH];
    char timestamp[TIMESTAMP_LENGTH];
    get_timestamp(timestamp, TIMESTAMP_LENGTH);
    snprintf(uid, HASH_HEX_OUTPUT_LENGTH, "%s%s", timestamp, payload);
    if (get_hash((unsigned char*)uid, msg->message_uid) != 0)
        return MESSAGE_CREATION_HASH_FAILURE;
    snprintf(msg->message_uid, HASH_HEX_OUTPUT_LENGTH, "%s", uid);
    msg->type = type;
    snprintf(msg->sender_uid, HASH_HEX_OUTPUT_LENGTH, "%s", sender_uid);
    snprintf(msg->recipient_uid, HASH_HEX_OUTPUT_LENGTH, "%s", recipient_uid);
    msg->payload_length = strlen(payload);
    snprintf(msg->payload, MAX_PAYLOAD_SIZE, "%s", payload);
    msg->payload[msg->payload_length] = '\0';

    return MESSAGE_CREATION_SUCCESS;
}

void parse_message(message_t* msg, const char* buffer)
{
    char format_string[128];
    snprintf(format_string, sizeof(format_string), "%%%d[^|]|%%d|%%%d[^|]|%%%d[^|]|%%u|%%%d[^\n]",
        HASH_HEX_OUTPUT_LENGTH - 1, HASH_HEX_OUTPUT_LENGTH - 1, HASH_HEX_OUTPUT_LENGTH - 1, MAX_PAYLOAD_SIZE - 1);
    sscanf(buffer, format_string,
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

int get_hash(const unsigned char* input, char* output)
{
    EVP_MD_CTX* mdCtx = EVP_MD_CTX_new();
    unsigned char hash[HASH_LENGTH];
    unsigned int mdLen, i;

    if (!mdCtx)
    {
        printf("Failed to create message digest context.\n");
        return 1;
    }

    if (!EVP_DigestInit_ex(mdCtx, EVP_sha512(), NULL))
    {
        printf("Message digest initialization failed.\n");
        EVP_MD_CTX_free(mdCtx);
        return 1;
    }

    if (!EVP_DigestUpdate(mdCtx, input, strlen((const char*)input)))
    {
        printf("Message digest update failed.\n");
        EVP_MD_CTX_free(mdCtx);
        return 1;
    }

    if (!EVP_DigestFinal_ex(mdCtx, hash, &mdLen))
    {
        printf("Message digest finalization failed.\n");
        EVP_MD_CTX_free(mdCtx);
        return 1;
    }

    EVP_MD_CTX_free(mdCtx);

    for (i = 0; i < mdLen; i++)
    {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }

    output[mdLen * 2] = '\0';

    return 0;
}

void get_timestamp(char* buffer, size_t buffer_size)
{
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    strftime(buffer, buffer_size, "%Y%m%d%H%M%S", &tm_now);
}

void get_formatted_timestamp(char* buffer, size_t buffer_size)
{
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &tm_now);
}

void format_uptime(long seconds, char* buffer, size_t buffer_size)
{
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    snprintf(buffer, buffer_size, "%02d:%02d:%02d", hours, minutes, secs);
}
