#include "protocol.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "protocol.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <openssl/evp.h>

int create_message(char* recipient_uid, char* message_buf, message_t* message) 
{
    if (strlen(recipient_uid) > CLIENT_HASH_LENGTH)
        return INVALID_UID_LENGTH;

    if (strlen(message_buf) > MAX_MES_SIZE)
        return INVALID_MESSAGE_LENGTH;

    strncpy(message->recipient_uid, recipient_uid, CLIENT_HASH_LENGTH - 1);
    strncpy(message->message, message_buf, MAX_MES_SIZE - 1);

    return MESSAGE_CREATED;
}

const unsigned char* get_hash(const char* password)
{
    unsigned char* hash = (unsigned char*)malloc(EVP_MAX_MD_SIZE);
    EVP_Digest(password, sizeof(password), hash, NULL, EVP_sha256(), NULL);
    return hash;
}

const char* get_timestamp()
{
    static char timestamp_str[20];
    time_t now = time(NULL);
    strftime(timestamp_str, sizeof(timestamp_str), "%Y%m%d%H%M%S", localtime(&now));
    return timestamp_str;
}

const char* generate_unique_user_id(const char* username)
{
    char input_str[BUFFER_SIZE];
    snprintf(input_str, BUFFER_SIZE, "%s%s", username, get_timestamp());
    const unsigned char* hash = get_hash(input_str);

    static char user_id[CLIENT_HASH_LENGTH + 1];
    for (int i = 0; i < CLIENT_HASH_LENGTH / 2; ++i)
    {
        snprintf(user_id + (i * 2), 3, "%02x", hash[i]);
    }
    user_id[CLIENT_HASH_LENGTH] = '\0';
    return user_id;
}
