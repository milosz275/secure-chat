#include "protocol.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "protocol.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <openssl/evp.h>

const unsigned char* get_hash(const char* password)
{
    unsigned char* hash = (unsigned char*)malloc(EVP_MAX_MD_SIZE);
    EVP_Digest(password, sizeof(password), hash, NULL, EVP_sha256(), NULL);
    return hash;
}

const char* get_timestamp()
{
    static char timestamp_str[TIMESTAMP_LENGTH];
    time_t now = time(NULL);
    strftime(timestamp_str, sizeof(timestamp_str), "%Y%m%d%H%M%S", localtime(&now));
    return timestamp_str;
}

const char* generate_uid(const char* text, int hash_length)
{
    static char input_str[BUFFER_SIZE];
    snprintf(input_str, BUFFER_SIZE, "%s%s", text, get_timestamp());
    const unsigned char* hash = get_hash(input_str);

    char* uid = (char*)malloc((hash_length * 2) + 1);
    if (uid == NULL)
    {
        return NULL;
    }

    for (int i = 0; i < hash_length; ++i)
    {
        snprintf(uid + (i * 2), 3, "%02x", hash[i]);
    }
    uid[hash_length * 2] = '\0';
    return uid;
}

const char* generate_unique_user_id(const char* username)
{
    return generate_uid(username, CLIENT_HASH_LENGTH);
}

void int_handler(int sig)
{
    char c;
    signal(sig, SIG_IGN);
    printf(" Do you want to quit? [y/n] ");
    c = getchar();
    if (c == 'y' || c == 'Y')
        exit(0);
    else
        signal(SIGINT, int_handler);
    getchar();
}
