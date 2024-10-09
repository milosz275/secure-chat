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

#include "protocol.h"
#include "client_openssl.h"
#include "log.h"

volatile sig_atomic_t quit_flag = 0;
volatile sig_atomic_t reconnect_flag = 0;
static struct client_t client = { -1, NULL, CLIENT_DEFAULT_NAME, NULL, NULL };
static struct client_state_t client_state = { 0, 0, 0, 0 };

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
            message_type_to_string(msg.type), msg.sender_uid, msg.recipient_uid, msg.payload);
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

        char enter_username_code_str[12];
        sprintf(enter_username_code_str, "%d", MESSAGE_CODE_ENTER_USERNAME);
        char enter_password_code_str[12];
        sprintf(enter_password_code_str, "%d", MESSAGE_CODE_ENTER_PASSWORD);
        char confirm_password_code_str[12];
        sprintf(confirm_password_code_str, "%d", MESSAGE_CODE_ENTER_PASSWORD_CONFIRMATION);
        char user_created_code_str[12];
        sprintf(user_created_code_str, "%d", MESSAGE_CODE_USER_CREATED);
        char user_authenticated_code_str[12];
        sprintf(user_authenticated_code_str, "%d", MESSAGE_CODE_USER_AUTHENTICATED);

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
        else if (msg.type == MESSAGE_AUTH && !strcmp(msg.payload, enter_username_code_str))
        {
            client_state.is_entering_username = 1;
            int code = atoi(msg.payload);
            const char* text = message_code_to_string(code);
            printf("(0%s) Server: %s\n", msg.payload, text);
        }
        else if (msg.type == MESSAGE_AUTH && !strcmp(msg.payload, enter_password_code_str))
        {
            client_state.is_entering_password = 1;
            int code = atoi(msg.payload);
            const char* text = message_code_to_string(code);
            printf("(0%s) Server: %s\n", msg.payload, text);
        }
        else if (msg.type == MESSAGE_AUTH && !strcmp(msg.payload, confirm_password_code_str))
        {
            client_state.is_confirming_password = 1;
            int code = atoi(msg.payload);
            const char* text = message_code_to_string(code);
            printf("(0%s) Server: %s\n", msg.payload, text);
        }
        else if (msg.type == MESSAGE_AUTH && (!strcmp(msg.payload, user_authenticated_code_str) || !strcmp(msg.payload, user_created_code_str)))
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
            printf("(0%d) Server: %s%s\n", MESSAGE_CODE_UID, message_code_to_string(MESSAGE_CODE_UID), msg.payload);
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
                    log_message(LOG_WARN, CLIENT_LOG, __FILE__, "Received quit signal that is not from server");
                else
                {
                    log_message(LOG_INFO, CLIENT_LOG, __FILE__, "Received quit signal from server");
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
                printf("(0%d) Server: %s\n", MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_THREE, message_code_to_string(MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_THREE));
                break;
            case 2:
                printf("(0%d) Server: %s\n", MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_TWO, message_code_to_string(MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_TWO));
                break;
            case 1:
                printf("(0%d) Server: %s\n", MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_ONE, message_code_to_string(MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_ONE));
                break;
            default:
                printf("(00001) Server: %s\n", msg.payload); // unknown code
                break;
            }
        }
        else if (msg.type == MESSAGE_USER_JOIN)
        {
            printf("(0%d) Server: %s%s\n", MESSAGE_CODE_USER_JOIN, msg.payload, message_code_to_string(MESSAGE_CODE_USER_JOIN));
        }
        else if (msg.type == MESSAGE_USER_LEAVE)
        {
            printf("(0%d) Server: %s%s\n", MESSAGE_CODE_USER_LEAVE, msg.payload, message_code_to_string(MESSAGE_CODE_USER_LEAVE));
        }
        else
        {
            int code = atoi(msg.payload);
            if (!code)
                printf("(00000) Server: %s\n", msg.payload); // unknown code
            else
            {
                const char* text = message_code_to_string(code);
                printf("(0%s) Server: %s\n", msg.payload, text);
            }
        }
    }

    pthread_exit(NULL);
}

int connect_to_server(struct sockaddr_in* server_addr)
{
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
    char input[MAX_PAYLOAD_SIZE];
    fd_set read_fds;
    int max_fd;
    init_logging(CLIENT_LOG);
    log_message(LOG_INFO, CLIENT_LOG, __FILE__, LOG_CLIENT_STARTED);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address/Address not supported");
        // log_message(LOG_ERROR, CLIENT_LOG, __FILE__, "Invalid address/Address not supported");
        exit(EXIT_FAILURE);
    }

    pthread_t recv_thread;
    while (!quit_flag)
    {
        if (connect_to_server(&server_addr))
        {
            printf("Failed to connect to server after %d attempts. Exiting.\n", SERVER_RECONNECTION_ATTEMPTS);
            exit(EXIT_FAILURE);
        }

        reconnect_flag = 0;
        client.uid = (char*)malloc(HASH_HEX_OUTPUT_LENGTH);
        if (client.uid == NULL)
        {
            perror("Memory allocation failed");
            close(client.socket);
            exit(EXIT_FAILURE);
        }
        strcpy(client.uid, CLIENT_DEFAULT_NAME);
        sleep(1);

        if (pthread_create(&recv_thread, NULL, receive_messages, (void*)&client.socket) != 0)
        {
            perror("Thread creation failed");
            close(client.socket);
            exit(EXIT_FAILURE);
        }

        while (!quit_flag && !reconnect_flag)
        {
            FD_ZERO(&read_fds);
            FD_SET(STDIN_FILENO, &read_fds);
            FD_SET(client.socket, &read_fds);

            max_fd = client.socket;

            int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

            if (activity < 0 && !quit_flag)
            {
                reconnect_flag = 1;
                break;
            }

            if (FD_ISSET(STDIN_FILENO, &read_fds))
            {
                memset(input, 0, sizeof(input));
                fgets(input, sizeof(input), stdin);
                input[strcspn(input, "\n")] = '\0';

                if (!strlen(input))
                {
                    continue;
                }

                if (!strcmp(input, "!exit") || !strcmp(input, "!quit"))
                {
                    reconnect_flag = 0;
                    quit_flag = 1;
                    exit(EXIT_SUCCESS);
                }

                message_t msg;
                if (client_state.is_entering_username)
                {
                    client_state.is_entering_username = 0;
                    client.username[0] = '\0';
                    strncpy(client.username, input, MAX_USERNAME_LENGTH);
                    client.username[MAX_USERNAME_LENGTH] = '\0';
                    create_message(&msg, MESSAGE_AUTH, client.uid, "server", input);
                }
                else if (client_state.is_entering_password)
                {
                    client_state.is_entering_password = 0;
                    create_message(&msg, MESSAGE_AUTH, client.uid, "server", input);
                }
                else if (client_state.is_confirming_password)
                {
                    client_state.is_confirming_password = 0;
                    create_message(&msg, MESSAGE_AUTH, client.uid, "server", input);
                }
                else
                    create_message(&msg, MESSAGE_TEXT, client.uid, "server", input);
#ifdef _DEBUG
                printf("(debug) %s: %s\n", client.username, input);
#endif
                if (send_message(client.ssl, &msg) != MESSAGE_SEND_SUCCESS)
                {
                    perror("Send failed. Server might be disconnected.");
                    reconnect_flag = 1;
                    break;
                }
            }

            if (FD_ISSET(client.socket, &read_fds))
            {
                continue;
            }
        }

        if (reconnect_flag)
        {
            printf("Attempting to reconnect...\n");
            client_state.is_authenticated = 0;
            client.username[0] = '\0';
            if (client.uid)
            {
                free(client.uid);
                client.uid = NULL;
            }
            close(client.socket);
            pthread_cancel(recv_thread);
            pthread_join(recv_thread, NULL);
        }
    }

    printf("Client is shutting down.\n");
    if (client.uid)
        free(client.uid);
    if (close(client.socket) == -1)
        perror("Error closing socket");
    if (pthread_cancel(recv_thread) != 0)
        perror("Error cancelling thread");
    if (pthread_join(recv_thread, NULL) != 0)
        perror("Error joining thread");
    exit(EXIT_SUCCESS);
}
