#ifndef __SERVER_H
#define __SERVER_H

#include <pthread.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 100

typedef struct
{
    struct sockaddr_in address;
    int socket;
    int uid;
} client_t;

struct clients_t
{
    pthread_mutex_t mutex;
    client_t *array[MAX_CLIENTS];
};

void *handle_client(void *arg);
void run_server();

#endif
