#include "client_msg_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <raylib.h>

#include "protocol.h"
#include "client.h"
#include "client_gui.h"
#include "log.h"

void handle_message(message_t* msg, client_t* client, client_state_t* state, volatile sig_atomic_t* reconnect_flag, volatile sig_atomic_t* quit_flag, volatile sig_atomic_t* server_answer)
{
    int msg_from_srv = !strcmp(msg->sender_uid, "server");
    if (msg->type == MESSAGE_PING && msg_from_srv)
    {
        create_message(msg, MESSAGE_ACK, client->uid, "server", "ACK");
        if (send_message(client->ssl, msg) != MESSAGE_SEND_SUCCESS)
        {
            perror("Send failed");
            *reconnect_flag = 1;
            pthread_exit(NULL);
        }
        log_message(T_LOG_INFO, CLIENT_LOG, __FILE__, "Received PING from server and sent ACK");
        return;
    }
    else if (msg->type == MESSAGE_ACK && msg_from_srv)
    {
        log_message(T_LOG_INFO, CLIENT_LOG, __FILE__, "Received ACK from server");
        *server_answer = 1;
        return;
    }

    // dropping messages addressed to other users
    if (!state->is_authenticated && strcmp(client->uid, CLIENT_DEFAULT_NAME) && msg->type != MESSAGE_PING && msg->type != MESSAGE_ACK)
    {
        // unauthenticated user should be addressed as CLIENT_DEFAULT_NAME
        printf("(11111) Server: %s\n", msg->payload);
        return;
    }
    else if (state->is_authenticated && strncmp((char*)msg->recipient_uid, client->uid, HASH_HEX_OUTPUT_LENGTH - 1) && msg->type != MESSAGE_UID && msg->type != MESSAGE_PING && msg->type != MESSAGE_ACK)
    {
        // authenticated user should be addressed by their UID
        printf("(11112) Server: %s\n", msg->payload);
        return;
    }

    if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_ENTER_USERNAME)) && msg_from_srv)
    {
        state->just_joined = 0;
        state->is_entering_username = 1;
        int code = atoi(msg->payload);
        const char* text = message_code_to_text(code);
        printf("(0%s) Server: %s\n", msg->payload, text);
    }
    else if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_ENTER_PASSWORD)) && msg_from_srv)
    {
        state->is_entering_username = 0;
        state->is_entering_password = 1;
        int code = atoi(msg->payload);
        const char* text = message_code_to_text(code);
        printf("(0%s) Server: %s\n", msg->payload, text);
    }
    else if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_ENTER_PASSWORD_CONFIRMATION)) && msg_from_srv)
    {
        state->is_confirming_password = 1;
        int code = atoi(msg->payload);
        const char* text = message_code_to_text(code);
        printf("(0%s) Server: %s\n", msg->payload, text);
    }
    else if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_USER_REGISTER_INFO)) && msg_from_srv)
    {
        state->can_register = 1;
        int code = atoi(msg->payload);
        const char* text = message_code_to_text(code);
        printf("(0%s) Server: %s\n", msg->payload, text);
    }
    else if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_TRY_AGAIN)) && msg_from_srv)
    {
        state->can_register = 0;
        int code = atoi(msg->payload);
        const char* text = message_code_to_text(code);
        printf("(0%s) Server: %s\n", msg->payload, text);
    }
    else if (msg->type == MESSAGE_CHOICE && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_USER_REGISTER_CHOICE)) && msg_from_srv)
    {
        state->is_entering_username = 0;
        state->is_choosing_register = 1;
        int code = atoi(msg->payload);
        const char* text = message_code_to_text(code);
        printf("(0%s) Server: %s\n", msg->payload, text);
    }
    else if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_USER_ALREADY_ONLINE)) && msg_from_srv)
    {
        reset_state(state);
        int code = atoi(msg->payload);
        const char* text = message_code_to_text(code);
        printf("(0%s) Server: %s\n", msg->payload, text);
    }
    else if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_USER_AUTHENTICATED)) && msg_from_srv)
    {
        state->is_entering_password = 0;
        state->is_authenticated = 1;
    }
    else if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_USER_CREATED)) && msg_from_srv)
    {
        state->is_confirming_password = 0;
        state->is_authenticated = 1;
    }
    else if (msg->type == MESSAGE_UID && msg_from_srv)
    {
        if (strlen(msg->payload) != HASH_HEX_OUTPUT_LENGTH - 1)
        {
            printf("Invalid UID received from server\n");
            return;
        }
        printf("(0%d) Server: %s%s\n", MESSAGE_CODE_UID, message_code_to_text(MESSAGE_CODE_UID), msg->payload);
        if (client->uid != NULL)
            strcpy(client->uid, msg->payload);
        else
        {
            client->uid = malloc(strlen(msg->payload) + 1);
            strcpy(client->uid, msg->payload);
        }
    }
    else if (msg->type == MESSAGE_SIGNAL)
    {
        if ((!strcmp(msg->payload, MESSAGE_SIGNAL_QUIT)) || (!strcmp(msg->payload, MESSAGE_SIGNAL_EXIT)))
        {
            if (msg_from_srv)
                log_message(T_LOG_WARN, CLIENT_LOG, __FILE__, "Received quit signal that is not from server");
            else
            {
                log_message(T_LOG_INFO, CLIENT_LOG, __FILE__, "Received quit signal from server");
                *reconnect_flag = 0;
                *quit_flag = 1;
                exit(EXIT_SUCCESS);
            }
        }
    }
    else if (msg->type == MESSAGE_AUTH_ATTEMPS && msg_from_srv)
    {
        switch (atoi(msg->payload))
        {
        case 3:
            printf("(0%d) Server: %s\n", MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_THREE, message_code_to_text(MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_THREE));
            state->auth_attempts = 3;
            break;
        case 2:
            printf("(0%d) Server: %s\n", MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_TWO, message_code_to_text(MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_TWO));
            state->auth_attempts = 2;
            break;
        case 1:
            printf("(0%d) Server: %s\n", MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_ONE, message_code_to_text(MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_ONE));
            state->auth_attempts = 1;
            break;
        default:
            printf("(00001) Server: %s\n", msg->payload); // unknown code
            break;
        }
    }
    else if (msg->type == MESSAGE_USER_JOIN && msg_from_srv)
    {
        printf("(0%d) Server: %s%s\n", MESSAGE_CODE_USER_JOIN, msg->payload, message_code_to_text(MESSAGE_CODE_USER_JOIN));
    }
    else if (msg->type == MESSAGE_USER_LEAVE && msg_from_srv)
    {
        printf("(0%d) Server: %s%s\n", MESSAGE_CODE_USER_LEAVE, msg->payload, message_code_to_text(MESSAGE_CODE_USER_LEAVE));
    }
    else if (msg->type == MESSAGE_BROADCAST && msg_from_srv)
    {
        printf("(0%d) Server: %s\n", MESSAGE_CODE_BROADCAST, msg->payload);
        add_message("Server", msg->payload);
    }
    else
    {
        if (msg_from_srv)
        {
            int code = atoi(msg->payload);
            if (!code)
            {
                printf("(00000) Server: %s\n", msg->payload); // unknown code
                char message[BUFFER_SIZE];
                snprintf(message, BUFFER_SIZE, "%s: %s", msg->sender_uid, msg->payload);
            }
            else
            {
                const char* text = message_code_to_text(code);
                printf("(0%s) Server: %s\n", msg->payload, text);
            }
        }
        else
        {
            printf("%s: %s\n", msg->sender_uid, msg->payload);
            add_message(msg->sender_uid, msg->payload);
        }
    }
}
