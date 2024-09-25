#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/select.h>

#include "protocol.h"

#define NK_IMPLEMENTATION
#include "nuklear.h"

#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define MAX_MEMORY 4096 * 1024 // 4MB

int quit_flag = 0;
int reconnect_flag = 0;
pthread_t recv_thread;
int sock = -1;

void* receive_messages(void* socket_desc)
{
    int sock = *(int*)socket_desc;
    char buffer[BUFFER_SIZE];
    message_t msg;
    int nbytes;

    while (!quit_flag && !reconnect_flag)
    {
        memset(buffer, 0, sizeof(buffer));
        nbytes = recv(sock, buffer, sizeof(buffer), 0);
        if (nbytes <= 0)
        {
            if (nbytes == 0)
            {
                printf("Server disconnected.\n");
            }
            else
            {
                perror("Receive failed");
            }
            close(sock);
            reconnect_flag = 1;
            pthread_exit(NULL);
        }

        buffer[nbytes] = '\0';
        msg.payload[0] = '\0';
        msg.payload_length = 0;

        parse_message(&msg, buffer);
        printf("%s: %s\n", msg.sender_uid, msg.payload);
    }

    pthread_exit(NULL);
}

int connect_to_server(struct sockaddr_in* server_addr)
{
    int sock;
    int connection_attempts = 0;

    while (connection_attempts < SERVER_RECONNECTION_ATTEMPTS)
    {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }

        if (connect(sock, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0)
        {
            printf("Connection failed.\nRetrying in %d seconds... (Attempt %d/%d)\n",
                SERVER_RECONNECTION_INTERVAL, connection_attempts + 1, SERVER_RECONNECTION_ATTEMPTS);
            close(sock);
            connection_attempts++;
            sleep(SERVER_RECONNECTION_INTERVAL);
        }
        else
        {
            printf("Connected to server\n");
            return sock;
        }
    }

    return -1;
}

void run_client()
{
    struct sockaddr_in server_addr;
    char input[MAX_PAYLOAD_SIZE];
    message_t msg;
    fd_set read_fds;
    int max_fd;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address/Address not supported");
        exit(EXIT_FAILURE);
    }

    while (!quit_flag)
    {
        sock = connect_to_server(&server_addr);
        if (sock < 0)
        {
            printf("Failed to connect to server after %d attempts. Exiting.\n", SERVER_RECONNECTION_ATTEMPTS);
            exit(EXIT_FAILURE);
        }
        reconnect_flag = 0;
        sleep(1);

        if (pthread_create(&recv_thread, NULL, receive_messages, (void*)&sock) != 0)
        {
            perror("Thread creation failed");
            close(sock);
            exit(EXIT_FAILURE);
        }

        while (!quit_flag && !reconnect_flag)
        {
            FD_ZERO(&read_fds);
            FD_SET(STDIN_FILENO, &read_fds);
            FD_SET(sock, &read_fds);

            max_fd = sock;

            int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

            if (activity < 0 && !quit_flag)
            {
                perror("select error");
                reconnect_flag = 1;
                break;
            }

            if (FD_ISSET(STDIN_FILENO, &read_fds))
            {
                memset(input, 0, sizeof(input));
                fgets(input, sizeof(input), stdin);
                input[strcspn(input, "\n")] = '\0';

                if (strlen(input) == 0)
                {
                    continue;
                }

                if (strcmp(input, "!exit") == 0)
                {
                    quit_flag = 1;
                    break;
                }

                create_message(&msg, MESSAGE_TEXT, "client", "server", input);

                if (send_message(sock, &msg) != MESSAGE_SEND_SUCCESS)
                {
                    perror("Send failed. Server might be disconnected.");
                    reconnect_flag = 1;
                    break;
                }
            }

            if (FD_ISSET(sock, &read_fds))
            {
                continue;
            }
        }

        if (reconnect_flag)
        {
            printf("Attempting to reconnect...\n");
            close(sock);
            pthread_cancel(recv_thread);
            pthread_join(recv_thread, NULL);
        }
    }

    printf("Client is shutting down.\n");
    close(sock);
    pthread_cancel(recv_thread);
    pthread_join(recv_thread, NULL);
    exit(EXIT_SUCCESS);
}
