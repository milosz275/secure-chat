#include "server_auth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "server_db.h"
#include "log.h"
#include "hash_map.h"

void usleep(unsigned int usec);

int user_auth(request_t* req, client_connection_t* cl, hash_map* user_map)
{
    char log_msg[MAX_LOG_LENGTH];
    message_t msg;
    char buffer[BUFFER_SIZE];
    int nbytes;

    sqlite3* db;
    sqlite3_stmt* stmt = NULL;
    int db_result = connect_db(&db, DB_NAME);
    if (db_result != DATABASE_CONNECTION_SUCCESS)
    {
        close(req->socket);
        return USER_AUTHENTICATION_DATABASE_CONNECTION_FAILURE;
    }

    int attempts = 0;
    while (attempts < USER_LOGIN_ATTEMPTS)
    {
        char send_msg[MAX_PAYLOAD_SIZE];
        send_msg[0] = '\0';
        if (!attempts)
        {
            sprintf(send_msg, "%d", MESSAGE_CODE_WELCOME);
            create_message(&msg, MESSAGE_TOAST, "server", CLIENT_DEFAULT_NAME, send_msg);
            send_message(req->ssl, &msg);
            send_msg[0] = '\0';
            sprintf(send_msg, "%d", USER_LOGIN_ATTEMPTS - attempts);
            create_message(&msg, MESSAGE_AUTH_ATTEMPS, "server", CLIENT_DEFAULT_NAME, send_msg);
            sleep(1);
            send_message(req->ssl, &msg);
            send_msg[0] = '\0';
            sprintf(send_msg, "%d", MESSAGE_CODE_USER_REGISTER_INFO);
            create_message(&msg, MESSAGE_SYSTEM, "server", CLIENT_DEFAULT_NAME, send_msg);
            sleep(1);
            send_message(req->ssl, &msg);
        }
        else
        {
            sprintf(send_msg, "%d", USER_LOGIN_ATTEMPTS - attempts);
            create_message(&msg, MESSAGE_AUTH_ATTEMPS, "server", CLIENT_DEFAULT_NAME, send_msg);
            send_message(req->ssl, &msg);
        }
        send_msg[0] = '\0';
        sprintf(send_msg, "%d", MESSAGE_CODE_ENTER_USERNAME);
        create_message(&msg, MESSAGE_AUTH, "server", CLIENT_DEFAULT_NAME, send_msg);
        usleep(100000); // 100 ms
        send_message(req->ssl, &msg);

        nbytes = SSL_read(req->ssl, buffer, sizeof(buffer));
        if (nbytes <= 0)
        {
            close(req->socket);
            return USER_AUTHENTICATION_USERNAME_RECEIVE_FAILURE;
        }

        parse_message(&msg, buffer);

        char username[MAX_USERNAME_LENGTH + 1];
        snprintf(username, MAX_USERNAME_LENGTH, "%.*s", MAX_USERNAME_LENGTH - 1, msg.payload);
        username[MAX_USERNAME_LENGTH] = '\0';

        log_msg[0] = '\0';
        sprintf(log_msg, "Request from %s:%d for username %s", inet_ntoa(req->address.sin_addr), ntohs(req->address.sin_port), username);
        log_message(LOG_INFO, REQUESTS_LOG, __FILE__, log_msg);

        char* sql = "SELECT uid FROM users WHERE username = ?;";

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
        {
            fprintf(stderr, "Can't prepare username query: %s\n", sqlite3_errmsg(db));
            goto cleanup;
        }

        if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK)
        {
            fprintf(stderr, "Can't bind username parameter: %s\n", sqlite3_errmsg(db));
            goto cleanup;
        }

        if (sqlite3_step(stmt) != SQLITE_ROW)
        {
            if (!attempts)
            {
                // register
                send_msg[0] = '\0';
                sprintf(send_msg, "%d", MESSAGE_CODE_USER_DOES_NOT_EXIST);
                create_message(&msg, MESSAGE_AUTH, "server", CLIENT_DEFAULT_NAME, send_msg);
                send_message(req->ssl, &msg);
                send_msg[0] = '\0';
                sprintf(send_msg, "%d", MESSAGE_CODE_USER_REGISTER_CHOICE);
                create_message(&msg, MESSAGE_CHOICE, "server", CLIENT_DEFAULT_NAME, send_msg);
                usleep(100000); // 100 ms
                send_message(req->ssl, &msg);

                nbytes = SSL_read(req->ssl, buffer, sizeof(buffer));
                if (nbytes <= 0)
                {
                    goto cleanup;
                }

                parse_message(&msg, buffer);

                if (!strcmp(msg.payload, "y") || !strcmp(msg.payload, "Y"))
                {
                    send_msg[0] = '\0';
                    sprintf(send_msg, "%d", MESSAGE_CODE_ENTER_PASSWORD);
                    create_message(&msg, MESSAGE_AUTH, "server", CLIENT_DEFAULT_NAME, send_msg);
                    send_message(req->ssl, &msg);

                    nbytes = SSL_read(req->ssl, buffer, sizeof(buffer));
                    if (nbytes <= 0)
                    {
                        goto cleanup;
                    }

                    parse_message(&msg, buffer);

                    char password[MAX_PASSWORD_LENGTH];
                    snprintf(password, MAX_PASSWORD_LENGTH, "%.*s", MAX_PASSWORD_LENGTH - 1, msg.payload);

                    send_msg[0] = '\0';
                    sprintf(send_msg, "%d", MESSAGE_CODE_ENTER_PASSWORD_CONFIRMATION);
                    create_message(&msg, MESSAGE_AUTH, "server", CLIENT_DEFAULT_NAME, send_msg);
                    send_message(req->ssl, &msg);

                    nbytes = SSL_read(req->ssl, buffer, sizeof(buffer));
                    if (nbytes <= 0)
                    {
                        goto cleanup;
                    }

                    parse_message(&msg, buffer);

                    char password_confirmation[MAX_PASSWORD_LENGTH];
                    snprintf(password_confirmation, MAX_PASSWORD_LENGTH, "%.*s", MAX_PASSWORD_LENGTH - 1, msg.payload);

                    if (!strcmp(password, password_confirmation))
                    {
                        snprintf(cl->username, MAX_USERNAME_LENGTH + 1, "%s", username);

                        cl->uid = (char*)malloc(HASH_HEX_OUTPUT_LENGTH);
                        if (!cl->uid)
                        {
                            fprintf(stderr, "Failed to allocate memory for UID\n");
                            log_msg[0] = '\0';
                            sprintf(log_msg, "Failed to allocate memory for UID - register request from %s:%d", inet_ntoa(req->address.sin_addr), ntohs(req->address.sin_port));
                            log_message(LOG_WARN, REQUESTS_LOG, __FILE__, log_msg);
                            log_message(LOG_WARN, SERVER_LOG, __FILE__, log_msg);
                            goto cleanup;
                        }
                        if (get_hash((unsigned char*)username, cl->uid) != 0)
                        {
                            fprintf(stderr, "Failed to hash username\n");
                            log_msg[0] = '\0';
                            sprintf(log_msg, "Failed to hash username - register request from %s:%d", inet_ntoa(req->address.sin_addr), ntohs(req->address.sin_port));
                            log_message(LOG_WARN, REQUESTS_LOG, __FILE__, log_msg);
                            log_message(LOG_WARN, SERVER_LOG, __FILE__, log_msg);
                            goto cleanup;
                        }

                        char password_hash[HASH_HEX_OUTPUT_LENGTH];
                        if (get_hash((unsigned char*)password, password_hash) != 0)
                        {
                            fprintf(stderr, "Failed to hash password\n");
                            log_msg[0] = '\0';
                            sprintf(log_msg, "Failed to hash password - register request from %s:%d", inet_ntoa(req->address.sin_addr), ntohs(req->address.sin_port));
                            log_message(LOG_WARN, REQUESTS_LOG, __FILE__, log_msg);
                            log_message(LOG_WARN, SERVER_LOG, __FILE__, log_msg);
                            goto cleanup;
                        }

                        sql = "INSERT INTO users (username, uid, password_hash, last_login) VALUES (?, ?, ?, CURRENT_TIMESTAMP);";

                        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
                        {
                            fprintf(stderr, "Can't prepare user creation query: %s\n", sqlite3_errmsg(db));
                            goto cleanup;
                        }

                        if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK ||
                            sqlite3_bind_text(stmt, 2, cl->uid, -1, SQLITE_STATIC) != SQLITE_OK ||
                            sqlite3_bind_text(stmt, 3, (char*)password_hash, HASH_HEX_OUTPUT_LENGTH, SQLITE_STATIC) != SQLITE_OK)
                        {
                            fprintf(stderr, "Can't bind query parameters: %s\n", sqlite3_errmsg(db));
                            goto cleanup;
                        }

                        if (sqlite3_step(stmt) != SQLITE_DONE)
                        {
                            fprintf(stderr, "Can't create user: %s\n", sqlite3_errmsg(db));
                            goto cleanup;
                        }

                        sqlite3_finalize(stmt);
                        stmt = NULL;
                        sqlite3_close(db);
                        cl->request = req;

                        log_msg[0] = '\0';
                        sprintf(log_msg, "Registered user %s with UID %s", username, cl->uid);
                        log_message(LOG_INFO, REQUESTS_LOG, __FILE__, log_msg);

                        // send auth success message without UID specified as recipient parameter. user should be reading UID from next message now on
                        send_msg[0] = '\0';
                        sprintf(send_msg, "%d", MESSAGE_CODE_USER_CREATED);
                        create_message(&msg, MESSAGE_AUTH, "server", CLIENT_DEFAULT_NAME, send_msg);
                        send_message(req->ssl, &msg);

                        char send_uid[HASH_HEX_OUTPUT_LENGTH];
                        snprintf(send_uid, HASH_HEX_OUTPUT_LENGTH, "%s", cl->uid);
                        create_message(&msg, MESSAGE_UID, "server", CLIENT_DEFAULT_NAME, send_uid);
                        usleep(100000); // 100 ms
                        send_message(req->ssl, &msg);

                        return USER_AUTHENTICATION_SUCCESS;
                    }
                    else
                    {
                        send_msg[0] = '\0';
                        sprintf(send_msg, "%d", MESSAGE_CODE_PASSWORDS_DO_NOT_MATCH);
                        create_message(&msg, MESSAGE_AUTH, "server", CLIENT_DEFAULT_NAME, send_msg);
                        send_message(req->ssl, &msg);
                        send_msg[0] = '\0';
                        sprintf(send_msg, "%d", MESSAGE_CODE_TRY_AGAIN);
                        create_message(&msg, MESSAGE_TEXT, "server", CLIENT_DEFAULT_NAME, send_msg);
                        usleep(100000); // 100 ms
                        send_message(req->ssl, &msg);
                        attempts++;
                    }
                }
                else if (!strcmp(msg.payload, "n") || !strcmp(msg.payload, "N"))
                {
                    attempts++;
                }
                else
                {
                    log_msg[0] = '\0';
                    char choice_truncated[4];
                    snprintf(choice_truncated, 4, "%.*s", 3, msg.payload);
                    sprintf(log_msg, "Request from %s:%d failed authentication - invalid choice: %s", inet_ntoa(req->address.sin_addr), ntohs(req->address.sin_port), choice_truncated);
                    log_message(LOG_INFO, REQUESTS_LOG, __FILE__, log_msg);
                    goto cleanup;
                }
            }
            else
            {
                attempts++;
                if (attempts == USER_LOGIN_ATTEMPTS)
                {
                    send_msg[0] = '\0';
                    sprintf(send_msg, "%d", MESSAGE_CODE_USER_DOES_NOT_EXIST);
                    create_message(&msg, MESSAGE_AUTH, "server", CLIENT_DEFAULT_NAME, send_msg);
                    send_message(req->ssl, &msg);
                    send_msg[0] = '\0';
                    sprintf(send_msg, "%d", MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_EXCEEDED);
                    create_message(&msg, MESSAGE_ERROR, "server", CLIENT_DEFAULT_NAME, send_msg);
                    usleep(100000); // 100 ms
                    send_message(req->ssl, &msg);

                    log_msg[0] = '\0';
                    sprintf(log_msg, "Request from %s:%d failed authentication - out of login attempts", inet_ntoa(req->address.sin_addr), ntohs(req->address.sin_port));
                    log_message(LOG_INFO, REQUESTS_LOG, __FILE__, log_msg);
                    goto cleanup;
                }
                else
                {
                    send_msg[0] = '\0';
                    sprintf(send_msg, "%d", MESSAGE_CODE_USER_DOES_NOT_EXIST);
                    create_message(&msg, MESSAGE_AUTH, "server", CLIENT_DEFAULT_NAME, send_msg);
                    send_message(req->ssl, &msg);
                    send_msg[0] = '\0';
                    sprintf(send_msg, "%d", MESSAGE_CODE_TRY_AGAIN);
                    create_message(&msg, MESSAGE_TEXT, "server", CLIENT_DEFAULT_NAME, send_msg);
                    usleep(100000); // 100 ms
                    send_message(req->ssl, &msg);
                }
            }
        }
        else
        {
            // username exists, check if online
            const char* uid = (const char*)sqlite3_column_text(stmt, 0); // this becomes inaccessible after first sqlite3_finalize

            if (hash_map_find(user_map, uid, &cl))
            {
                log_msg[0] = '\0';
                sprintf(log_msg, "Request from %s:%d failed authentication - user already online", inet_ntoa(req->address.sin_addr), ntohs(req->address.sin_port));
                log_message(LOG_INFO, REQUESTS_LOG, __FILE__, log_msg);
                send_msg[0] = '\0';
                sprintf(send_msg, "%d", MESSAGE_CODE_USER_ALREADY_ONLINE);
                create_message(&msg, MESSAGE_AUTH, "server", CLIENT_DEFAULT_NAME, send_msg);
                send_message(req->ssl, &msg);
                sqlite3_finalize(stmt);
                stmt = NULL;
                attempts++;
                break;
            }

            // authenticate credentials
            send_msg[0] = '\0';
            sprintf(send_msg, "%d", MESSAGE_CODE_ENTER_PASSWORD);
            create_message(&msg, MESSAGE_AUTH, "server", CLIENT_DEFAULT_NAME, send_msg);
            send_message(req->ssl, &msg);

            nbytes = SSL_read(req->ssl, buffer, sizeof(buffer));
            if (nbytes <= 0)
            {
                goto cleanup;
            }

            parse_message(&msg, buffer);

            char password[MAX_PASSWORD_LENGTH];
            snprintf(password, MAX_PASSWORD_LENGTH, "%.*s", MAX_PASSWORD_LENGTH - 1, msg.payload);

            char password_hash[HASH_HEX_OUTPUT_LENGTH];
            if (get_hash((unsigned char*)password, password_hash) != 0)
            {
                fprintf(stderr, "Failed to hash password\n");
                log_msg[0] = '\0';
                sprintf(log_msg, "Failed to hash password - login request from %s:%d", inet_ntoa(req->address.sin_addr), ntohs(req->address.sin_port));
                log_message(LOG_WARN, REQUESTS_LOG, __FILE__, log_msg);
                log_message(LOG_WARN, SERVER_LOG, __FILE__, log_msg);
                goto cleanup;
            }

            sqlite3_finalize(stmt);
            stmt = NULL;
            sql = "SELECT uid FROM users WHERE username = ? AND password_hash = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
            {
                fprintf(stderr, "Can't prepare user authentication query: %s\n", sqlite3_errmsg(db));
                goto cleanup;
            }

            if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK ||
                sqlite3_bind_text(stmt, 2, (char*)password_hash, HASH_HEX_OUTPUT_LENGTH, SQLITE_STATIC) != SQLITE_OK)
            {
                fprintf(stderr, "Can't bind query parameters: %s\n", sqlite3_errmsg(db));
                goto cleanup;
            }

            if (sqlite3_step(stmt) != SQLITE_ROW)
            {
                log_msg[0] = '\0';
                sprintf(log_msg, "Request from %s:%d failed authentication - invalid password", inet_ntoa(req->address.sin_addr), ntohs(req->address.sin_port));
                log_message(LOG_INFO, REQUESTS_LOG, __FILE__, log_msg);
                attempts++;
                if (attempts == USER_LOGIN_ATTEMPTS)
                {
                    send_msg[0] = '\0';
                    sprintf(send_msg, "%d", MESSAGE_CODE_INVALID_PASSWORD);
                    create_message(&msg, MESSAGE_AUTH, "server", CLIENT_DEFAULT_NAME, send_msg);
                    send_message(req->ssl, &msg);
                    send_msg[0] = '\0';
                    sprintf(send_msg, "%d", MESSAGE_CODE_USER_AUTHENTICATION_ATTEMPTS_EXCEEDED);
                    create_message(&msg, MESSAGE_ERROR, "server", CLIENT_DEFAULT_NAME, send_msg);
                    usleep(100000); // 100 ms
                    send_message(req->ssl, &msg);
                }
                else
                {
                    send_msg[0] = '\0';
                    sprintf(send_msg, "%d", MESSAGE_CODE_ENTER_PASSWORD);
                    create_message(&msg, MESSAGE_AUTH, "server", CLIENT_DEFAULT_NAME, send_msg);
                    send_message(req->ssl, &msg);
                    send_msg[0] = '\0';
                    sprintf(send_msg, "%d", MESSAGE_CODE_TRY_AGAIN);
                    create_message(&msg, MESSAGE_TEXT, "server", CLIENT_DEFAULT_NAME, send_msg);
                    usleep(100000); // 100 ms
                    send_message(req->ssl, &msg);
                }

                if (attempts >= USER_LOGIN_ATTEMPTS)
                {
                    log_msg[0] = '\0';
                    sprintf(log_msg, "Request from %s:%d failed authentication - out of login attempts", inet_ntoa(req->address.sin_addr), ntohs(req->address.sin_port));
                    log_message(LOG_INFO, REQUESTS_LOG, __FILE__, log_msg);
                    goto cleanup;
                }
            }
            else
            {
                // successful auth, set username, get UID
                snprintf(cl->username, MAX_USERNAME_LENGTH + 1, "%s", username);

                const char* uid = (const char*)sqlite3_column_text(stmt, 0);
                cl->uid = (char*)malloc(strlen(uid) + 1);
                if (!cl->uid)
                {
                    fprintf(stderr, "Memory allocation failed for cl->uid\n");
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    close(req->socket);
                    return USER_AUTHENTICATION_MEMORY_ALLOCATION_FAILURE;
                }
                strcpy(cl->uid, uid);

                sqlite3_finalize(stmt);
                stmt = NULL;

                // last login update
                sql = "UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE uid = ?;";
                if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK ||
                    sqlite3_bind_text(stmt, 1, cl->uid, -1, SQLITE_STATIC) != SQLITE_OK)
                {
                    fprintf(stderr, "Can't update last login: %s\n", sqlite3_errmsg(db));
                    goto cleanup;
                }

                if (sqlite3_step(stmt) != SQLITE_DONE)
                {
                    fprintf(stderr, "Failed to update last login: %s\n", sqlite3_errmsg(db));
                    goto cleanup;
                }

                sqlite3_finalize(stmt);
                stmt = NULL;
                sqlite3_close(db);
                cl->request = req;

                log_msg[0] = '\0';
                sprintf(log_msg, "Authenticated user %s with UID %s", username, cl->uid);
                log_message(LOG_INFO, REQUESTS_LOG, __FILE__, log_msg);

                // send auth success message without UID specified as recipient parameter. user should be reading UID from next message now on
                send_msg[0] = '\0';
                sprintf(send_msg, "%d", MESSAGE_CODE_USER_AUTHENTICATED);
                create_message(&msg, MESSAGE_AUTH, "server", CLIENT_DEFAULT_NAME, send_msg);
                send_message(req->ssl, &msg);

                char send_uid[HASH_HEX_OUTPUT_LENGTH];
                snprintf(send_uid, HASH_HEX_OUTPUT_LENGTH, "%s", cl->uid);
                create_message(&msg, MESSAGE_UID, "server", CLIENT_DEFAULT_NAME, send_uid);
                usleep(100000); // 100 ms
                send_message(req->ssl, &msg);

                return USER_AUTHENTICATION_SUCCESS;
            }
        }
    }
    if (db)
    {
        sqlite3_close(db);
    }

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    if (cl->uid)
        free(cl->uid);
    if (db)
        sqlite3_close(db);
    close(req->socket);
    return USER_AUTHENTICATION_FAILURE;
}
