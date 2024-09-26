#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "protocol.h"
#include "servercli.h"
#include "serverdb.h"
#include "serverauth.h"

volatile sig_atomic_t quit_flag = 0;

static struct clients_t clients = { PTHREAD_MUTEX_INITIALIZER, {NULL} };

static struct server_t server = { 0, {0}, 0, PTHREAD_MUTEX_INITIALIZER, {0} };

void usleep(unsigned int usec);

int srv_exit(char** args)
{
    quit_flag = 1;
    exit(0);
    if (args[0] != NULL) {}
    return 1;
}

int srv_ban(char** args)
{
    // [ ] Implement ban command
    if (args[0] != NULL) {}
    return 1;
}

int srv_kick(char** args)
{
    // [ ] Implement kick command
    if (args[0] != NULL) {}
    return 1;
}

int srv_mute(char** args)
{
    // [ ] Implement mute command
    if (args[0] != NULL) {}
    return 1;
}

void* handle_client(void* arg)
{
    char buffer[BUFFER_SIZE];
    int nbytes;
    request_t* req_ptr = ((request_t*)arg);
    request_t req = *req_ptr;
    message_t msg;
    client_t cl;

    if (user_auth(&req, &cl) != USER_AUTHENTICATION_SUCCESS)
    {
        close(req.socket);
        pthread_exit(NULL);
    }

    pthread_mutex_lock(&clients.mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (!clients.array[i])
        {
            cl.id = i;
            printf("Client %d connected\n", cl.id);
            clients.array[i] = &cl;
            break;
        }
    }
    pthread_mutex_unlock(&clients.mutex);

    pthread_mutex_lock(&clients.mutex);
    char welcome_message[MAX_PAYLOAD_SIZE];
    sprintf(welcome_message, "Welcome to the chat server, your ID is %d\n", cl.id);
    create_message(&msg, MESSAGE_TEXT, "server", "client", welcome_message);
    send_message(cl.request->socket, &msg);

    char join_message[MAX_PAYLOAD_SIZE];
    sprintf(join_message, "Client %d has joined the chat\n", cl.id);
    create_message(&msg, MESSAGE_TEXT, "server", "client", join_message);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients.array[i] && clients.array[i] != &cl)
        {
            create_message(&msg, MESSAGE_TEXT, "server", "client", join_message);
            send_message(clients.array[i]->request->socket, &msg);
        }
    }
    pthread_mutex_unlock(&clients.mutex);

    while ((nbytes = recv(cl.request->socket, buffer, sizeof(buffer), 0)) > 0)
    {
        if (quit_flag)
        {
            break;
        }

        if (nbytes > 0)
        {
            parse_message(&msg, buffer);
            printf("Received message from client %d, %s: %s\n", cl.id, msg.sender_uid, msg.payload);

            char payload[MAX_PAYLOAD_SIZE];
            strncpy(payload, msg.payload, MAX_PAYLOAD_SIZE);
            payload[MAX_PAYLOAD_SIZE - 1] = '\0';

            pthread_mutex_lock(&clients.mutex);
            for (int i = 0; i < MAX_CLIENTS; ++i)
            {
                if (clients.array[i] && clients.array[i] != &cl)
                {
                    char id_str[20];
                    snprintf(id_str, sizeof(id_str), "%d", cl.id);
                    create_message(&msg, MESSAGE_TEXT, id_str, "client", payload);
                    send_message(clients.array[i]->request->socket, &msg);
                }
            }
            pthread_mutex_unlock(&clients.mutex);
        }
        else
        {
            printf("Received malformed message from client %d\n", cl.id);
        }
    }

    close(cl.request->socket);
    pthread_exit(NULL);
}

void* handle_cli(void* arg)
{
    char* line = NULL;
    int status;
    while (!quit_flag)
    {
        line = srv_read_line();
        status = srv_exec_line(line);
        if (line)
            free(line);
        if (status == 1)
        {
            break;
        }
    }
    if (arg) {}
    pthread_exit(NULL);
}

int run_server()
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    request_t req;
    sqlite3* db;
    if (setup_db(&db, DB_NAME) != DATABASE_CREATE_SUCCESS)
    {
        return DATABASE_SETUP_FAILURE;
    }

    pthread_t cli_thread;
    if (pthread_create(&cli_thread, NULL, handle_cli, (void*)NULL) != 0)
    {
        perror("CLI thread creation failed");
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    int attempts = 0;
    int bind_success = 0;
    while (attempts < PORT_BIND_ATTEMPTS)
    {
        if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
        {
            perror("Bind failed");
            printf("Retrying in %d seconds... (Attempt %d/%d)\n",
                PORT_BIND_INTERVAL, attempts + 1, PORT_BIND_ATTEMPTS);
            attempts++;
            if (attempts < PORT_BIND_ATTEMPTS)
            {
                if (quit_flag)
                {
                    close(server_socket);
                    printf("Server shutting down...\n");
                    exit(EXIT_SUCCESS);
                }
                sleep(PORT_BIND_INTERVAL);
            }
            else
            {
                printf("Could not bind to the server address\n");
                close(server_socket);
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            bind_success = 1;
            break;
        }
    }
    if (!bind_success)
    {
        printf("Could not bind to the server address\n");
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

    while (!quit_flag)
    {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0)
        {
            if (quit_flag)
            {
                close(server_socket);
                printf("Server shutting down...\n");
                exit(EXIT_SUCCESS);
            }
            perror("Accept failed");
            continue;
        }

        // req.timestamp = (char*)get_timestamp();
        req.socket = client_socket;
        req.address = client_addr;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, (void*)&req) != 0)
        {
            perror("Thread creation failed");
            close(client_socket);
            continue;
        }
        pthread_mutex_lock(&server.thread_count_mutex);
        if (server.thread_count < MAX_CLIENTS)
        {
            server.threads[server.thread_count++] = tid;
        }
        pthread_mutex_unlock(&server.thread_count_mutex);

        usleep(200000); // 200 ms
    }

    for (int i = 0; i < server.thread_count; ++i)
    {
        pthread_join(server.threads[i], NULL);
    }

    close(server_socket);
    pthread_join(cli_thread, NULL);
    printf("Server shutting down...\n");
    exit(EXIT_SUCCESS);
}
