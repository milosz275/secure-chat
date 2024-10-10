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
#include "client_openssl.h"
#include "log.h"

volatile sig_atomic_t quit_flag = 0;
volatile sig_atomic_t reconnect_flag = 0;
static struct client_t client = { -1, NULL, CLIENT_DEFAULT_NAME, NULL, NULL, "\0" };
static struct client_state_t client_state = { 1, 0, 0, 0, 0, 0, 0 };

void* receive_messages(void* arg)
{
    int sock = *(int*)arg;
    char buffer[BUFFER_SIZE];
    message_t msg;
    int nbytes;

    while (!quit_flag && !reconnect_flag)
    {
        memset(buffer, 0, sizeof(buffer));
        nbytes = SSL_read(client.ssl, buffer, sizeof(buffer));
        if (nbytes <= 0)
        {
            int err = SSL_get_error(client.ssl, nbytes);
            if (err == SSL_ERROR_ZERO_RETURN)
                printf("Server disconnected.\n");
            else
                perror("Receive failed");
            SSL_shutdown(client.ssl);
            close(sock);
            reconnect_flag = 1;
            pthread_exit(NULL);
        }

        buffer[nbytes] = '\0';
        msg.payload[0] = '\0';
        msg.payload_length = 0;

        parse_message(&msg, buffer);
#ifdef _DEBUG
        printf("(debug) Received message: type=%s, sender_uid=%s, recipient_uid=%s, payload=\"%s\"\n",
            message_type_to_text(msg.type), msg.sender_uid, msg.recipient_uid, msg.payload);
#endif
        // dropping messages addressed to other users
        if (!client_state.is_authenticated && strcmp(client.uid, CLIENT_DEFAULT_NAME))
        {
            // unauthenticated user should be addressed as CLIENT_DEFAULT_NAME
            printf("(11111) Server: %s\n", msg.payload);
            continue;
        }
        else if (client_state.is_authenticated && strncmp((char*)msg.recipient_uid, client.uid, HASH_HEX_OUTPUT_LENGTH - 1) && msg.type != MESSAGE_UID)
        {
            // authenticated user should be addressed by their UID
            printf("(11112) Server: %s\n", msg.payload);
            continue;
        }

        if (msg.type == MESSAGE_PING)
        {
            create_message(&msg, MESSAGE_ACK, client.uid, "server", "ACK");
#ifdef _DEBUG
            printf("(debug) %s: %s\n", client.username, "ACK sent to server");
#endif
            if (send_message(client.ssl, &msg) != MESSAGE_SEND_SUCCESS)
            {
                perror("Send failed");
                close(sock);
                reconnect_flag = 1;
                pthread_exit(NULL);
            }
        }
        else if (msg.type == MESSAGE_AUTH && !strcmp(msg.payload, message_code_to_string(MESSAGE_CODE_ENTER_USERNAME)))
        {
            client_state.just_joined = 0;
            client_state.is_entering_username = 1;
            int code = atoi(msg.payload);
            const char* text = message_code_to_text(code);
            printf("(0%s) Server: %s\n", msg.payload, text);
        }
        else if (msg.type == MESSAGE_AUTH && !strcmp(msg.payload, message_code_to_string(MESSAGE_CODE_ENTER_PASSWORD)))
        {
            client_state.is_entering_password = 1;
            int code = atoi(msg.payload);
            const char* text = message_code_to_text(code);
            printf("(0%s) Server: %s\n", msg.payload, text);
        }
        else if (msg.type == MESSAGE_AUTH && !strcmp(msg.payload, message_code_to_string(MESSAGE_CODE_ENTER_PASSWORD_CONFIRMATION)))
        {
            client_state.is_confirming_password = 1;
            int code = atoi(msg.payload);
            const char* text = message_code_to_text(code);
            printf("(0%s) Server: %s\n", msg.payload, text);
        }
        else if (msg.type == MESSAGE_CHOICE && !strcmp(msg.payload, message_code_to_string(MESSAGE_CODE_USER_REGISTER_CHOICE)))
        {
            client_state.is_choosing_register = 1;
            int code = atoi(msg.payload);
            const char* text = message_code_to_text(code);
            printf("(0%s) Server: %s\n", msg.payload, text);
        }
        else if (msg.type == MESSAGE_AUTH && !strcmp(msg.payload, message_code_to_string(MESSAGE_CODE_USER_ALREADY_ONLINE)))
        {
            reset_state(&client_state);
            int code = atoi(msg.payload);
            const char* text = message_code_to_text(code);
            printf("(0%s) Server: %s\n", msg.payload, text);
        }
        else if (msg.type == MESSAGE_AUTH && (!strcmp(msg.payload, message_code_to_string(MESSAGE_CODE_USER_AUTHENTICATED)) || !strcmp(msg.payload, message_code_to_string(MESSAGE_CODE_USER_CREATED))))
        {
            client_state.is_authenticated = 1;
        }
        else if (msg.type == MESSAGE_UID)
        {
            if (strlen(msg.payload) != HASH_HEX_OUTPUT_LENGTH - 1)
            {
                printf("Invalid UID received from server\n");
                continue;
            }
            printf("(0%d) Server: %s%s\n", MESSAGE_CODE_UID, message_code_to_text(MESSAGE_CODE_UID), msg.payload);
            if (client.uid != NULL)
                strcpy(client.uid, msg.payload);
            else
            {
                client.uid = malloc(strlen(msg.payload) + 1);
                strcpy(client.uid, msg.payload);
            }
        }
        else if (msg.type == MESSAGE_ACK)
        {
            // [ ] handle ACK
        }
        else if (msg.type == MESSAGE_SIGNAL)
        {
            if ((!strcmp(msg.payload, MESSAGE_SIGNAL_QUIT)) || (!strcmp(msg.payload, MESSAGE_SIGNAL_EXIT)))
            {
                if (strcmp(msg.sender_uid, "server"))
                    log_message(T_LOG_WARN, CLIENT_LOG, __FILE__, "Received quit signal that is not from server");
                else
                {
                    log_message(T_LOG_INFO, CLIENT_LOG, __FILE__, "Received quit signal from server");
                    reconnect_flag = 0;
                    quit_flag = 1;
                    exit(EXIT_SUCCESS);
                }
            }
        }
        else if (msg.type == MESSAGE_AUTH_ATTEMPS)
        {
            switch (atoi(msg.payload))
            {
            case 3:
                printf("(0%d) Server: %s\n", MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_THREE, message_code_to_text(MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_THREE));
                break;
            case 2:
                printf("(0%d) Server: %s\n", MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_TWO, message_code_to_text(MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_TWO));
                break;
            case 1:
                printf("(0%d) Server: %s\n", MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_ONE, message_code_to_text(MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_ONE));
                break;
            default:
                printf("(00001) Server: %s\n", msg.payload); // unknown code
                break;
            }
        }
        else if (msg.type == MESSAGE_USER_JOIN)
        {
            printf("(0%d) Server: %s%s\n", MESSAGE_CODE_USER_JOIN, msg.payload, message_code_to_text(MESSAGE_CODE_USER_JOIN));
        }
        else if (msg.type == MESSAGE_USER_LEAVE)
        {
            printf("(0%d) Server: %s%s\n", MESSAGE_CODE_USER_LEAVE, msg.payload, message_code_to_text(MESSAGE_CODE_USER_LEAVE));
        }
        else
        {
            int code = atoi(msg.payload);
            if (!code)
            {
                printf("(00000) Server: %s\n", msg.payload); // unknown code
                char message[MAX_MESSAGE_LENGTH];
                snprintf(message, MAX_MESSAGE_LENGTH, "%s: %s", msg.sender_uid, msg.payload);
                add_message(message);
            }
            else
            {
                const char* text = message_code_to_text(code);
                printf("(0%s) Server: %s\n", msg.payload, text);
            }
        }
    }

    pthread_exit(NULL);
}

int connect_to_server(struct sockaddr_in* server_addr)
{
    if (quit_flag)
        return -1;

    int connection_attempts = 0;

    while (connection_attempts < SERVER_RECONNECTION_ATTEMPTS)
    {
        client.socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client.socket < 0)
        {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }

        if (connect(client.socket, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0)
        {
            printf("Connection failed.\nRetrying in %d seconds... (Attempt %d/%d)\n",
                SERVER_RECONNECTION_INTERVAL, connection_attempts + 1, SERVER_RECONNECTION_ATTEMPTS);
            close(client.socket);
            connection_attempts++;
            sleep(SERVER_RECONNECTION_INTERVAL);
        }
        else
        {
            init_ssl(&client);
            if (SSL_connect(client.ssl) <= 0)
            {
                fprintf(stderr, "SSL handshake failed.\n");
                ERR_print_errors_fp(stderr);
                SSL_free(client.ssl);
                close(client.socket);
                return -1;
            }
            printf("Connected to server with SSL/TLS\n");
            SSL_CTX_set_verify(client.ssl_ctx, SSL_VERIFY_NONE, NULL);
            SSL_set_verify(client.ssl, SSL_VERIFY_NONE, NULL);
            SSL_set_mode(client.ssl, SSL_MODE_AUTO_RETRY);
            SSL_set_fd(client.ssl, client.socket);
            return 0;
        }
    }

    return -1;
}

void run_client()
{
    struct sockaddr_in server_addr;
    init_logging(CLIENT_LOG);
    log_message(T_LOG_INFO, CLIENT_LOG, __FILE__, LOG_CLIENT_STARTED);
    char log_msg[MAX_LOG_LENGTH];

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
    {
        log_message(T_LOG_ERROR, CLIENT_LOG, __FILE__, "Invalid address/Address not supported");
        finish_logging();
        exit(EXIT_FAILURE);
    }

    init_ui();

    pthread_t recv_thread;
    while (!quit_flag && !reconnect_flag)
    {
        if (connect_to_server(&server_addr))
        {
            log_msg[0] = '\0';
            sprintf(log_msg, "Failed to connect to server after %d attempts. Exiting.", SERVER_RECONNECTION_ATTEMPTS);
            log_message(T_LOG_ERROR, CLIENT_LOG, __FILE__, log_msg);
            finish_logging();
            exit(EXIT_FAILURE);
        }

        reconnect_flag = 0;
        client.uid = (char*)malloc(HASH_HEX_OUTPUT_LENGTH);
        if (client.uid == NULL)
        {
            log_message(T_LOG_ERROR, CLIENT_LOG, __FILE__, "Memory allocation failed");
            finish_logging();
            close(client.socket);
            exit(EXIT_FAILURE);
        }
        strcpy(client.uid, CLIENT_DEFAULT_NAME);
        sleep(1);

        if (pthread_create(&recv_thread, NULL, receive_messages, (void*)&client.socket) != 0)
        {
            log_message(T_LOG_ERROR, CLIENT_LOG, __FILE__, "Thread creation failed");
            finish_logging();
            close(client.socket);
            exit(EXIT_FAILURE);
        }

        while (!quit_flag && !reconnect_flag && !WindowShouldClose())
        {
            ui_cycle(&client, &client_state, &reconnect_flag, &quit_flag);
        }
        if (reconnect_flag)
        {
            log_message(T_LOG_WARN, CLIENT_LOG, __FILE__, "Attempting to reconnect...");
            client_state.is_authenticated = 0;
            client.username[0] = '\0';
            if (client.uid)
            {
                free(client.uid);
                client.uid = NULL;
            }
            close(client.socket);
        }
        pthread_cancel(recv_thread);
        pthread_join(recv_thread, NULL);
    }

    CloseWindow();
    destroy_ssl(&client);
    if (client.uid)
        free(client.uid);
    if (close(client.socket) == -1)
        perror("Error closing socket");
    if (pthread_cancel(recv_thread) != 0)
        perror("Error cancelling thread");
    if (pthread_join(recv_thread, NULL) != 0)
        perror("Error joining thread");
    log_message(T_LOG_INFO, CLIENT_LOG, __FILE__, "Client is shutting down.");
    finish_logging();
    exit(EXIT_SUCCESS);
}
