#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/select.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <time.h>
#include <raylib.h>

#include "protocol.h"
#include "client_gui.h"
#include "client_msg_handler.h"
#include "client_openssl.h"
#include "log.h"

volatile sig_atomic_t quit_flag = 0;
volatile sig_atomic_t reconnect_flag = 0;
static struct client_t client = { -1, NULL, CLIENT_DEFAULT_NAME, NULL, NULL, "\0" };
static struct client_state_t client_state = { 1, 0, 0, 0, 0, 0, 0, -1, 0 };
int server_answer = 0;

void* receive_messages(void* arg)
{
    while (!quit_flag)
    {
        if (reconnect_flag)
        {
            sleep(SERVER_RECONNECTION_INTERVAL);
            continue;
        }
        else if (!client_state.is_connected)
        {
            sleep(1);
            continue;
        }
        else if (!client.ssl)
        {
            log_message(T_LOG_ERROR, CLIENT_LOG, __FILE__, "Recv thread: SSL object is NULL");
            reconnect_flag = 1;
            continue;
        }

        char buffer[BUFFER_SIZE];
        message_t msg;
        memset(buffer, 0, sizeof(buffer));
        int nbytes = SSL_read(client.ssl, buffer, sizeof(buffer));

        if (nbytes <= 0)
        {
            int err = SSL_get_error(client.ssl, nbytes);
            if (err == SSL_ERROR_ZERO_RETURN)
                printf("Server disconnected.\n");
            reconnect_flag = 1;
            continue;
        }

        buffer[nbytes] = '\0';
        msg.payload[0] = '\0';
        msg.payload_length = 0;

        parse_message(&msg, buffer);
        handle_message(&msg, &client, &client_state, &reconnect_flag, &quit_flag, &server_answer);
    }
    if (arg) {}
    pthread_exit(NULL);
}

void* attempt_reconnection(void* arg)
{
    while (!quit_flag)
    {
        if (reconnect_flag || !client_state.is_connected)
        {
            if (reconnect_flag)
                printf("Attempting to reconnect...\n");
            if (connect_to_server((struct sockaddr_in*)arg) == 0)
            {
                reconnect_flag = 0;  // successfully reconnected
                if (!client_state.is_connected)
                    log_message(T_LOG_INFO, CLIENT_LOG, __FILE__, "Connected to server");
                else
                    log_message(T_LOG_INFO, CLIENT_LOG, __FILE__, "Reconnected to server");
                client_state.is_connected = 1;
            }
            else
                printf("Reconnection failed. Retrying...\n");
            sleep(SERVER_RECONNECTION_INTERVAL);  // wait before trying again
        }
        else
            sleep(1);  // sleep if not reconnecting
    }
    pthread_exit(NULL);
}

void* handle_server_ping(void* arg)
{
    while (!quit_flag)
    {
        if (reconnect_flag)
        {
            sleep(SERVER_RECONNECTION_INTERVAL);  // sleep during reconnection
            continue;
        }
        else if (!client.ssl)
        {
            log_message(T_LOG_WARN, CLIENT_LOG, __FILE__, "Ping thread: SSL object is NULL");
            if (client_state.is_connected)
                reconnect_flag = 1;
            sleep(1);
            continue;
        }

        if (client_state.is_authenticated)
        {
            message_t msg;
            create_message(&msg, MESSAGE_PING, client.uid, "server", "PING");

            if (send_message(client.ssl, &msg) != MESSAGE_SEND_SUCCESS)
            {
                client_state.is_connected = 0;
                reconnect_flag = 1;
                continue;
            }

            server_answer = 0;
            sleep(5);

            if (!server_answer)
            {
                log_message(T_LOG_WARN, CLIENT_LOG, __FILE__, "No response to PING. Reconnecting...");
                client_state.is_connected = 0;
                reconnect_flag = 1;
            }
        }
        else
            sleep(5);  // sleep if not authenticated
    }
    if (arg) {}
    pthread_exit(NULL);
}

void cleanup_client_connection()
{
    if (client.ssl)
    {
        SSL_shutdown(client.ssl);
        SSL_free(client.ssl);
        client.ssl = NULL;
    }
    if (client.ssl_ctx)
    {
        SSL_CTX_free(client.ssl_ctx);
        client.ssl_ctx = NULL;
    }

    if (client.socket != -1)
    {
        close(client.socket);
        client.socket = -1;
    }

    if (client.uid)
    {
        free(client.uid);
        client.uid = NULL;
    }
}

int connect_to_server(struct sockaddr_in* server_addr)
{
    if (quit_flag)
        return -1;

    client.socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client.socket < 0)
    {
        perror("Socket creation failed");
        return -1;
    }

    if (connect(client.socket, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0)
    {
        close(client.socket);
        client.socket = -1;
        return -1;
    }

    init_ssl(&client);
    if (SSL_connect(client.ssl) <= 0)
    {
        ERR_print_errors_fp(stderr);
        SSL_free(client.ssl);
        close(client.socket);
        return -1;
    }

    SSL_set_mode(client.ssl, SSL_MODE_AUTO_RETRY);
    SSL_set_fd(client.ssl, client.socket);
    return 0;
}

void run_client()
{
    struct sockaddr_in server_addr;
    init_logging(CLIENT_LOG);
    log_message(T_LOG_INFO, CLIENT_LOG, __FILE__, LOG_CLIENT_STARTED);

    client.uid = (char*)malloc(HASH_HEX_OUTPUT_LENGTH);
    if (!client.uid)
    {
        log_message(T_LOG_ERROR, CLIENT_LOG, __FILE__, "Memory allocation failed for UID");
        cleanup_client_connection();

    }
    strcpy(client.uid, CLIENT_DEFAULT_NAME);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
    {
        log_message(T_LOG_ERROR, CLIENT_LOG, __FILE__, "Invalid address/Address not supported");
        finish_logging();
        exit(EXIT_FAILURE);
    }
    init_ui(&client_state);

    pthread_t reconnect_thread, recv_thread, ping_thread;
    pthread_create(&reconnect_thread, NULL, attempt_reconnection, (void*)&server_addr);
    pthread_create(&recv_thread, NULL, receive_messages, NULL);
    pthread_create(&ping_thread, NULL, handle_server_ping, NULL);

    while (!WindowShouldClose())
        ui_cycle(&client, &client_state, &reconnect_flag, &quit_flag);

    pthread_cancel(reconnect_thread);
    pthread_cancel(recv_thread);
    pthread_cancel(ping_thread);

    pthread_join(reconnect_thread, NULL);
    pthread_join(recv_thread, NULL);
    pthread_join(ping_thread, NULL);

    cleanup_client_connection();
    finish_logging();
    exit(EXIT_SUCCESS);
}
