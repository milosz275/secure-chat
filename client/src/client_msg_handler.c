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

void handle_message(message* msg, client* cl, client_state* cl_state, volatile sig_atomic_t* reconnect_flag, volatile sig_atomic_t* quit_flag, volatile sig_atomic_t* server_answer, const char* log_filename)
{
    int msg_from_srv = !strcmp(msg->sender_uid, "server");
    if (msg->type == MESSAGE_PING && msg_from_srv)
    {
        create_message(msg, MESSAGE_ACK, cl->uid, "server", "ACK");
        if (send_message(cl->ssl, msg) != MESSAGE_SEND_SUCCESS)
        {
            perror("Send failed");
            *reconnect_flag = 1;
            pthread_exit(NULL);
        }
        log_message(T_LOG_INFO, log_filename, __FILE__, "Received PING from server and sent ACK");
        return;
    }
    else if (msg->type == MESSAGE_ACK && msg_from_srv)
    {
        log_message(T_LOG_INFO, log_filename, __FILE__, "Received ACK from server");
        *server_answer = 1;
        return;
    }
    if (msg->type == MESSAGE_PING && !msg_from_srv)
        return;
    else if (msg->type == MESSAGE_ACK && !msg_from_srv)
        return;

    // dropping messages addressed to other users
    if (!cl_state->is_authenticated && strcmp(cl->uid, CLIENT_DEFAULT_NAME) && msg->type != MESSAGE_PING && msg->type != MESSAGE_ACK)
    {
        // unauthenticated user should be addressed as CLIENT_DEFAULT_NAME
        printf("(11111) Server: %s\n", msg->payload);
        return;
    }
    else if (cl_state->is_authenticated && strncmp((char*)msg->recipient_uid, cl->uid, HASH_HEX_OUTPUT_LENGTH - 1) && msg->type != MESSAGE_UID && msg->type != MESSAGE_PING && msg->type != MESSAGE_ACK)
    {
        // authenticated user should be addressed by their UID
        printf("(11112) Server: %s\n", msg->payload);
        return;
    }

    if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_ENTER_USERNAME)) && msg_from_srv)
    {
        cl_state->just_joined = 0;
        cl_state->is_entering_username = 1;
        int code = atoi(msg->payload);
        const char* text = message_code_to_text(code);
        printf("(0%s) Server: %s\n", msg->payload, text);
    }
    else if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_ENTER_PASSWORD)) && msg_from_srv)
    {
        cl_state->is_entering_username = 0;
        cl_state->is_entering_password = 1;
        int code = atoi(msg->payload);
        const char* text = message_code_to_text(code);
        printf("(0%s) Server: %s\n", msg->payload, text);
    }
    else if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_ENTER_PASSWORD_CONFIRMATION)) && msg_from_srv)
    {
        cl_state->is_entering_password = 0;
        cl_state->is_confirming_password = 1;
        int code = atoi(msg->payload);
        const char* text = message_code_to_text(code);
        printf("(0%s) Server: %s\n", msg->payload, text);
    }
    else if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_USER_REGISTER_INFO)) && msg_from_srv)
    {
        cl_state->can_register = 1;
        int code = atoi(msg->payload);
        const char* text = message_code_to_text(code);
        printf("(0%s) Server: %s\n", msg->payload, text);
    }
    else if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_TRY_AGAIN)) && msg_from_srv)
    {
        cl_state->can_register = 0;
        int code = atoi(msg->payload);
        const char* text = message_code_to_text(code);
        printf("(0%s) Server: %s\n", msg->payload, text);
    }
    else if (msg->type == MESSAGE_CHOICE && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_USER_REGISTER_CHOICE)) && msg_from_srv)
    {
        cl_state->is_entering_username = 0;
        cl_state->is_choosing_register = 1;
        int code = atoi(msg->payload);
        const char* text = message_code_to_text(code);
        printf("(0%s) Server: %s\n", msg->payload, text);
    }
    else if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_USER_ALREADY_ONLINE)) && msg_from_srv)
    {
        reset_state(cl_state);
        int code = atoi(msg->payload);
        const char* text = message_code_to_text(code);
        printf("(0%s) Server: %s\n", msg->payload, text);
    }
    else if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_USER_AUTHENTICATED)) && msg_from_srv)
    {
        cl_state->is_entering_password = 0;
        cl_state->is_authenticated = 1;
    }
    else if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_USER_CREATED)) && msg_from_srv)
    {
        cl_state->is_confirming_password = 0;
        cl_state->is_authenticated = 1;
    }
    else if (msg->type == MESSAGE_AUTH && !strcmp(msg->payload, message_code_to_string(MESSAGE_CODE_USER_DOES_NOT_EXIST)) && msg_from_srv)
    {
        int code = atoi(msg->payload);
        const char* text = message_code_to_text(code);
        printf("(0%s) Server: %s\n", msg->payload, text);
    }
    else if (msg->type == MESSAGE_UID && msg_from_srv)
    {
        char username[MAX_USERNAME_LENGTH + 1];
        char uid[HASH_HEX_OUTPUT_LENGTH];
        sscanf(msg->payload, "%[^" MESSAGE_DELIMITER "]%*c%s", username, uid);
        printf("(0%d) Server: %s%s%s\n", MESSAGE_CODE_UID, username, message_code_to_text(MESSAGE_CODE_UID), uid);

        if (!cl->uid && strcmp(cl->username, username) == 0)
        {
            // allocating new uid for the client from server after auth
            cl->uid = malloc(strlen(uid) + 1);
            strcpy(cl->uid, uid);
            return;
        }
        else if (cl->uid && strcmp(cl->username, username) == 0 && strcmp(cl->uid, CLIENT_DEFAULT_NAME) == 0)
        {
            // assigning new uid for the client from server after auth
            free(cl->uid);
            cl->uid = malloc(strlen(uid) + 1);
            strcpy(cl->uid, uid);
            return;
        }
        else if (cl->uid && strcmp(cl->username, username) == 0 && strcmp(cl->uid, CLIENT_DEFAULT_NAME) != 0)
        {
            // skip uid overwrite
            printf("Attempted to overwrite user UID\n");
            printf("Current UID: %s\n", cl->uid);
            return;
        }
        else if (strcmp(cl->uid, CLIENT_DEFAULT_NAME) != 0 && strcmp(cl->username, username) == 0)
        {
            // skip being addressed with default name while authed
            printf("Received UID from server while already authenticated\n");
            return;
        }
        else if (strcmp(cl->username, username) != 0)
        {
            // [ ] Add local entry: user - UID, for communication
            printf("Received UID from server of another user\n");
            return;
        }
        else
        {
            // unknown situation
            printf("Received UID not handled\n");
            return;
        }
    }
    else if (msg->type == MESSAGE_SIGNAL)
    {
        if ((!strcmp(msg->payload, MESSAGE_SIGNAL_QUIT)) || (!strcmp(msg->payload, MESSAGE_SIGNAL_EXIT)))
        {
            if (msg_from_srv)
                log_message(T_LOG_WARN, log_filename, __FILE__, "Received quit signal that is not from server");
            else
            {
                log_message(T_LOG_INFO, log_filename, __FILE__, "Received quit signal from server");
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
            cl_state->auth_attempts = 3;
            break;
        case 2:
            printf("(0%d) Server: %s\n", MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_TWO, message_code_to_text(MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_TWO));
            cl_state->auth_attempts = 2;
            break;
        case 1:
            printf("(0%d) Server: %s\n", MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_ONE, message_code_to_text(MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_ONE));
            cl_state->auth_attempts = 1;
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
    else if (msg->type == MESSAGE_TOAST && msg_from_srv)
    {
        int code = atoi(msg->payload);
        const char* text = message_code_to_text(code);
        printf("(0%s) Server: %s\n", msg->payload, text);
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
            add_message(msg->sender_uid, msg->payload);
        }
        else
            printf("%s: %s\n", msg->sender_uid, msg->payload);
    }
}
