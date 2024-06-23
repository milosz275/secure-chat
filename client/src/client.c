#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "protocol.h"

void *receive_messages(void *socket_desc)
{
    int sock = *(int *)socket_desc;
    char buffer[BUFFER_SIZE];
    int nbytes;

    while ((nbytes = recv(sock, buffer, sizeof(buffer), 0)) > 0)
    {
        buffer[nbytes] = '\0';
        printf("%s\n", buffer);
    }

    if (nbytes == 0)
    {
        printf("Server disconnected\n");
    }
    else
    {
        perror("Recv failed");
    }

    pthread_exit(NULL);
}

void run_client()
{
    int sock;
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];
    pthread_t recv_thread;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address/Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
    printf("Connected to server\n");

    if (pthread_create(&recv_thread, NULL, receive_messages, (void *)&sock) != 0)
    {
        perror("Thread creation failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        fgets(message, BUFFER_SIZE, stdin);
        if (send(sock, message, strlen(message), 0) < 0)
        {
            perror("Send failed");
            break;
        }
    }

    close(sock);
}
