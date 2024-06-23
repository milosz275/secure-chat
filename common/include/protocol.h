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

typedef struct
{
    uint8_t type;
    uint8_t length;
    char data[BUFFER_SIZE];
} packet_t;

const unsigned char *get_hash(const char *password);
const char *get_timestamp();
const char *generate_unique_user_id(const char *username);

#endif
