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
static struct client cl = { -1, NULL, CLIENT_DEFAULT_NAME, NULL, NULL, "\0" };
static struct client_state cl_state = { 1, 0, 0, 0, 0, 0, 0, -1, 0 };
int server_answer = 0;
char log_filename[256];

void* receive_messages(void* arg)
{
    while (!quit_flag)
    {
        if (reconnect_flag)
        {
            sleep(SERVER_RECONNECTION_INTERVAL);
            continue;
        }
        else if (!cl_state.is_connected)
        {
            sleep(1);
            continue;
        }
        else if (!cl.uid)
        {
            log_message(T_LOG_ERROR, log_filename, __FILE__, "Recv thread: Client UID is NULL");
            sleep(5);
            continue;
        }
        else if (!cl.ssl)
        {
            log_message(T_LOG_ERROR, log_filename, __FILE__, "Recv thread: SSL object is NULL");
            reconnect_flag = 1;
            continue;
        }

        char buffer[BUFFER_SIZE];
        message msg;
        memset(buffer, 0, sizeof(buffer));
        int nbytes = SSL_read(cl.ssl, buffer, sizeof(buffer));

        if (nbytes <= 0)
        {
            int err = SSL_get_error(cl.ssl, nbytes);
            if (err == SSL_ERROR_ZERO_RETURN)
                printf("Server disconnected.\n");
            reconnect_flag = 1;
            continue;
        }

        buffer[nbytes] = '\0';
        msg.payload[0] = '\0';
        msg.payload_length = 0;

        parse_message(&msg, buffer);
        handle_message(&msg, &cl, &cl_state, &reconnect_flag, &quit_flag, &server_answer, log_filename);
    }
    if (arg) {}
    pthread_exit(NULL);
}

void* attempt_reconnection(void* arg)
{
    while (!quit_flag)
    {
        if (reconnect_flag || !cl_state.is_connected)
        {
            cl_state.is_connected = 0;
            reset_state(&cl_state);
            if (reconnect_flag)
            {
                printf("Attempting to reconnect...\n");
                if (cl.uid)
                {
                    free(cl.uid);
                    cl.uid = NULL;
                }
            }

            if (connect_to_server((struct sockaddr_in*)arg) == 0)
            {
                reconnect_flag = 0;  // successfully reconnected
                if (!cl_state.is_connected)
                    log_message(T_LOG_INFO, log_filename, __FILE__, "Connected to server");
                else
                    log_message(T_LOG_INFO, log_filename, __FILE__, "Reconnected to server");
                cl_state.is_connected = 1;
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
        else if (!cl.ssl)
        {
            log_message(T_LOG_WARN, log_filename, __FILE__, "Ping thread: SSL object is NULL");
            if (cl_state.is_connected)
                reconnect_flag = 1;
            sleep(1);
            continue;
        }

        if (cl_state.is_authenticated)
        {
            message msg;
            create_message(&msg, MESSAGE_PING, cl.uid, "server", "PING");

            if (send_message(cl.ssl, &msg) != MESSAGE_SEND_SUCCESS)
            {
                cl_state.is_connected = 0;
                reconnect_flag = 1;
                continue;
            }

            server_answer = 0;
            sleep(15);

            if (!server_answer)
            {
                log_message(T_LOG_WARN, log_filename, __FILE__, "No response to PING from server");
                // log_message(T_LOG_WARN, log_filename, __FILE__, "No response to PING. Reconnecting...");
                // cl_state.is_connected = 0;
                // reconnect_flag = 1;
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
    if (cl.ssl)
    {
        SSL_shutdown(cl.ssl);
        SSL_free(cl.ssl);
        cl.ssl = NULL;
    }
    if (cl.ssl_ctx)
    {
        SSL_CTX_free(cl.ssl_ctx);
        cl.ssl_ctx = NULL;
    }

    if (cl.sock != -1)
    {
        close(cl.sock);
        cl.sock = -1;
    }

    if (cl.uid)
    {
        free(cl.uid);
        cl.uid = NULL;
    }
}

int connect_to_server(struct sockaddr_in* server_addr)
{
    if (quit_flag)
        return -1;

    cl.sock = socket(AF_INET, SOCK_STREAM, 0);
    if (cl.sock < 0)
    {
        perror("Socket creation failed");
        return -1;
    }

    if (connect(cl.sock, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0)
    {
        close(cl.sock);
        cl.sock = -1;
        return -1;
    }

    int ssl_result = init_ssl(&cl);
    if (ssl_result == OPENSSL_SSL_CTX_CREATION_FAILURE)
    {
        log_message(T_LOG_ERROR, log_filename, __FILE__, "Failed to create SSL context");
        cleanup_client_connection();
        return -1;
    }
    else if (ssl_result == OPENSSL_SSL_OBJECT_FAILURE)
    {
        log_message(T_LOG_ERROR, log_filename, __FILE__, "Failed to create SSL object");
        cleanup_client_connection();
        return -1;
    }
    else if (ssl_result != OPENSSL_INIT_SUCCESS)
    {
        log_message(T_LOG_ERROR, log_filename, __FILE__, "Failed to initialize OpenSSL");
        cleanup_client_connection();
        return -1;
    }
    log_message(T_LOG_INFO, log_filename, __FILE__, "SSL initialization successful");
    if (SSL_connect(cl.ssl) <= 0)
    {
        ERR_print_errors_fp(stderr);
        SSL_free(cl.ssl);
        close(cl.sock);
        return -1;
    }

    SSL_set_mode(cl.ssl, SSL_MODE_AUTO_RETRY);
    SSL_set_fd(cl.ssl, cl.sock);
    return 0;
}

void run_client()
{
    struct sockaddr_in server_addr;
    char timestamp[15];
    get_timestamp(timestamp, sizeof(timestamp));
    snprintf(log_filename, sizeof(log_filename), "client-%s.log", timestamp);
    init_logging(log_filename);
    log_message(T_LOG_INFO, log_filename, __FILE__, "Client started");

    cl.uid = (char*)malloc(HASH_HEX_OUTPUT_LENGTH);
    if (!cl.uid)
    {
        log_message(T_LOG_ERROR, log_filename, __FILE__, "Memory allocation failed for UID");
        cleanup_client_connection();

    }
    strcpy(cl.uid, CLIENT_DEFAULT_NAME);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
    {
        log_message(T_LOG_ERROR, log_filename, __FILE__, "Invalid address/Address not supported");
        finish_logging();
        exit(EXIT_FAILURE);
    }
    init_ui(&cl_state);
    log_message(T_LOG_INFO, log_filename, __FILE__, "UI initialized");

    pthread_t reconnect_thread, recv_thread, ping_thread;
    pthread_create(&reconnect_thread, NULL, attempt_reconnection, (void*)&server_addr);
    pthread_create(&recv_thread, NULL, receive_messages, NULL);
    pthread_create(&ping_thread, NULL, handle_server_ping, NULL);

    while (!WindowShouldClose())
        ui_cycle(&cl, &cl_state, &reconnect_flag, &quit_flag, log_filename);

    enable_cli_input();
    pthread_join(reconnect_thread, NULL);
    pthread_join(recv_thread, NULL);
    pthread_join(ping_thread, NULL);

    cleanup_client_connection();
    finish_logging();
    exit(EXIT_SUCCESS);
}
