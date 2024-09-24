#include "protocol.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "protocol.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <arpa/inet.h>
#include <openssl/evp.h>

extern int quit_flag;

int create_message(message_t* msg, message_type_t type, char* sender_uid, char* recipient_uid, char* payload)
{
    if (msg == NULL || sender_uid == NULL || recipient_uid == NULL || payload == NULL)
    {
        return MESSAGE_CREATION_FAILURE;
    }

    msg->payload[0] = '\0';
    snprintf(msg->message_uid, MESSAGE_HASH_LENGTH, "%s", generate_uid(payload, MESSAGE_HASH_LENGTH));
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
    {
        return MESSAGE_SEND_FAILURE;
    }

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
    {
        return NULL;
    }

    unsigned int length = 0;
    EVP_Digest(input, strlen(input), hash, &length, EVP_sha256(), NULL);

    return hash;
}

char* generate_password_hash(const char* password)
{
    const unsigned char* hash = get_hash(password);
    if (hash == NULL)
    {
        return NULL;
    }

    int hash_length = 32;
    char* hash_str = (char*)malloc((hash_length * 2) + 1);
    if (hash_str == NULL)
    {
        return NULL;
    }

    for (int i = 0; i < hash_length; ++i)
    {
        snprintf(hash_str + (i * 2), 3, "%02x", hash[i]);
    }
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

char* generate_uid(const char* text, int hash_length)
{
    static char input_str[BUFFER_SIZE];
    snprintf(input_str, BUFFER_SIZE, "%s%s", text, get_timestamp());

    const unsigned char* hash = get_hash(input_str);
    if (hash == NULL)
    {
        return NULL;
    }

    static char uid[BUFFER_SIZE];
    if (hash_length * 2 + 1 > BUFFER_SIZE)
    {
        return NULL;
    }

    for (int i = 0; i < hash_length; ++i)
    {
        snprintf(uid + (i * 2), 3, "%02x", hash[i]);
    }
    uid[hash_length * 2] = '\0';

    free((void*)hash);
    return uid;
}

char* generate_unique_user_id(const char* username)
{
    return generate_uid(username, USERNAME_HASH_LENGTH);
}

void int_handler(int sig)
{
    char input[10];
    signal(sig, SIG_IGN);
    printf(" Do you want to quit? [Y/n] ");
    if (fgets(input, sizeof(input), stdin) != NULL)
    {
        input[strcspn(input, "\n")] = 0;
        if (input[0] == 'y' || input[0] == 'Y' || input[0] == '\0')
        {
            quit_flag = 1;
        }
        else
        {
            signal(SIGINT, int_handler);
        }
    }
}
