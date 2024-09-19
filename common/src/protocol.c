#include "protocol.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "protocol.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <openssl/evp.h>

const unsigned char *get_hash(const char *password)
{
    unsigned char *hash = (unsigned char*)malloc(EVP_MAX_MD_SIZE);
    EVP_Digest(password, sizeof(password), hash, NULL, EVP_sha256(), NULL);
    return hash;
}

const char *get_timestamp()
{
    static char timestamp_str[20];
    time_t now = time(NULL);
    strftime(timestamp_str, sizeof(timestamp_str), "%Y%m%d%H%M%S", localtime(&now));
    return timestamp_str;
}

const char *generate_unique_user_id(const char *username)
{
    char input_str[BUFFER_SIZE];
    snprintf(input_str, BUFFER_SIZE, "%s%s", username, get_timestamp());
    const unsigned char *hash = get_hash(input_str);
    
    static char user_id[65];
    for(int i = 0; i < 32; ++i)
    {
        snprintf(user_id + (i * 2), 3, "%02x", hash[i]);
    }
    user_id[64] = '\0';
    return user_id;
}
