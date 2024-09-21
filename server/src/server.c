#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "protocol.h"

static struct clients_t clients = {PTHREAD_MUTEX_INITIALIZER, {NULL}};

void *handle_client(void *arg)
{
    char buffer[BUFFER_SIZE];
    int nbytes;
    client_t *cl = (client_t *)arg;
    message_t msg;

    while ((nbytes = recv(cl->socket, buffer, sizeof(buffer), 0)) > 0)
    {
        if (nbytes == sizeof(message_t))
        {
            memcpy(&msg, buffer, sizeof(message_t));
            printf("Received message from client %d to recipient %s: %s\n", cl->id, msg.recipient_uid, msg.message);
            pthread_mutex_lock(&clients.mutex);
            int recipient_found = 0;

            for (int i = 0; i < MAX_CLIENTS; ++i)
            {
                // if (clients.array[i] && strcmp(clients.array[i]->uid, msg.recipient_uid) == 0)

                // temporary use id instead of uid
                char id_str[64];
                sprintf(id_str, "%d", clients.array[i]->id);
                if (clients.array[i] && strcmp(id_str, msg.recipient_uid) == 0)
                {
                    send(clients.array[i]->socket, msg.message, strlen(msg.message), 0);
                    recipient_found = 1;
                    break;
                }
            }

            if (!recipient_found)
            {
                char error_message[BUFFER_SIZE];
                sprintf(error_message, "Recipient with ID %s not found.\n", msg.recipient_uid);
                send(cl->socket, error_message, strlen(error_message), 0);
            }

            pthread_mutex_unlock(&clients.mutex);
        }
        else
        {
            printf("Received malformed message from client %d\n", cl->id);
        }
    }

    pthread_mutex_lock(&clients.mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients.array[i] == cl)
        {
            printf("Client %d disconnected\n", cl->id);
            clients.array[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients.mutex);

    close(cl->socket);
    free(cl);
    pthread_exit(NULL);
}

int run_server()
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) < 0)
    {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", PORT);

    while (1)
    {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0)
        {
            perror("Accept failed");
            continue;
        }

        client_t *cl = (client_t *)malloc(sizeof(client_t));
        cl->socket = client_socket;
        cl->address = client_addr;

        pthread_mutex_lock(&clients.mutex);
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (!clients.array[i])
            {
                cl->id = i;
                printf("Client %d connected\n", cl->id);
                clients.array[i] = cl;
                break;
            }
        }
        pthread_mutex_unlock(&clients.mutex);

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, (void *)cl) != 0)
        {
            perror("Thread creation failed");
            close(client_socket);
            free(cl);
            continue;
        }

        char welcome_message[BUFFER_SIZE];
        sprintf(welcome_message, "Welcome to the chat server, your ID is %d\n", cl->id);
        send(cl->socket, welcome_message, strlen(welcome_message), 0);

        char join_message[BUFFER_SIZE];
        sprintf(join_message, "Client %d has joined the chat\n", cl->id);
        pthread_mutex_lock(&clients.mutex);

        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (clients.array[i] && clients.array[i] != cl)
            {
                send(clients.array[i]->socket, join_message, strlen(join_message), 0);
            }
        }
        pthread_mutex_unlock(&clients.mutex);
    }
    close(server_socket);

    return 0;
}
