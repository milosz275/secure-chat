#include "serverauth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "serverdb.h"

int user_auth(request_t* req, client_t* cl)
{
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
        char start_message[MAX_PAYLOAD_SIZE];
        if (attempts == 0)
            sprintf(start_message, "Welcome! You have %d login attempts and you can register on the first one.", USER_LOGIN_ATTEMPTS - attempts);
        else if (USER_LOGIN_ATTEMPTS - attempts == 1)
            sprintf(start_message, "You have 1 login attempt.");
        else
            sprintf(start_message, "You have %d login attempts.", USER_LOGIN_ATTEMPTS - attempts);

        create_message(&msg, MESSAGE_TEXT, "server", "client", start_message);
        send_message(req->socket, &msg);
        sleep(2);
        create_message(&msg, MESSAGE_TEXT, "server", "client", "Enter your username: ");
        send_message(req->socket, &msg);

        nbytes = recv(req->socket, buffer, sizeof(buffer), 0);
        if (nbytes <= 0)
        {
            close(req->socket);
            return USER_AUTHENTICATION_USERNAME_RECEIVE_FAILURE;
        }

        parse_message(&msg, buffer);
        printf("Received username: %s\n", msg.payload);

        char username[MAX_USERNAME_LENGTH + 1];
        snprintf(username, MAX_USERNAME_LENGTH, "%.*s", MAX_USERNAME_LENGTH - 1, msg.payload);
        username[MAX_USERNAME_LENGTH] = '\0';

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
                create_message(&msg, MESSAGE_TEXT, "server", "client", "Username does not exist. Would you like to register? [y/n]: ");
                send_message(req->socket, &msg);

                nbytes = recv(req->socket, buffer, sizeof(buffer), 0);
                if (nbytes <= 0)
                {
                    goto cleanup;
                }

                parse_message(&msg, buffer);
                printf("Received choice: %s\n", msg.payload);

                if (strcmp(msg.payload, "y") == 0 || strcmp(msg.payload, "Y") == 0)
                {
                    create_message(&msg, MESSAGE_TEXT, "server", "client", "Enter your password: ");
                    send_message(req->socket, &msg);

                    nbytes = recv(req->socket, buffer, sizeof(buffer), 0);
                    if (nbytes <= 0)
                    {
                        goto cleanup;
                    }

                    parse_message(&msg, buffer);
                    printf("Received password: %s\n", msg.payload);

                    char password[MAX_PASSWORD_LENGTH];
                    snprintf(password, MAX_PASSWORD_LENGTH, "%.*s", MAX_PASSWORD_LENGTH - 1, msg.payload);

                    create_message(&msg, MESSAGE_TEXT, "server", "client", "Confirm your password: ");
                    send_message(req->socket, &msg);

                    nbytes = recv(req->socket, buffer, sizeof(buffer), 0);
                    if (nbytes <= 0)
                    {
                        goto cleanup;
                    }

                    parse_message(&msg, buffer);
                    printf("Received password confirmation: %s\n", msg.payload);

                    char password_confirmation[MAX_PASSWORD_LENGTH];
                    snprintf(password_confirmation, MAX_PASSWORD_LENGTH, "%.*s", MAX_PASSWORD_LENGTH - 1, msg.payload);

                    if (strcmp(password, password_confirmation) == 0)
                    {
                        cl->uid = (char*)generate_unique_user_id(username);
                        printf("Generated UID for %s: %s\n", username, cl->uid);

                        char* password_hash = generate_password_hash(password);

                        sql = "INSERT INTO users (username, uid, password_hash, last_login) VALUES (?, ?, ?, CURRENT_TIMESTAMP);";

                        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
                        {
                            fprintf(stderr, "Can't prepare user creation query: %s\n", sqlite3_errmsg(db));
                            goto cleanup;
                        }

                        if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK ||
                            sqlite3_bind_text(stmt, 2, cl->uid, -1, SQLITE_STATIC) != SQLITE_OK ||
                            sqlite3_bind_text(stmt, 3, (char*)password_hash, PASSWORD_HASH_LENGTH, SQLITE_STATIC) != SQLITE_OK)
                        {
                            fprintf(stderr, "Can't bind query parameters: %s\n", sqlite3_errmsg(db));
                            free(password_hash);
                            password_hash = NULL;
                            goto cleanup;
                        }

                        if (sqlite3_step(stmt) != SQLITE_DONE)
                        {
                            fprintf(stderr, "Can't create user: %s\n", sqlite3_errmsg(db));
                            goto cleanup;
                        }
                        free(password_hash);
                        password_hash = NULL;

                        sqlite3_finalize(stmt);
                        stmt = NULL;
                        sqlite3_close(db);
                        cl->request = req;
                        // cl->timestamp = (char*)get_timestamp();
                        return USER_AUTHENTICATION_SUCCESS;
                    }
                    else
                    {
                        create_message(&msg, MESSAGE_TEXT, "server", "client", "Passwords do not match, please try again.");
                        attempts++;
                    }
                }
                else if (strcmp(msg.payload, "n") == 0 || strcmp(msg.payload, "N") == 0)
                {
                    attempts++;
                }
                else
                {
                    goto cleanup;
                }
            }
            else
            {
                attempts++;
                if (attempts == USER_LOGIN_ATTEMPTS)
                {
                    create_message(&msg, MESSAGE_TEXT, "server", "client", "Username does not exist. Out of login attempts.");
                    send_message(req->socket, &msg);
                    goto cleanup;
                }
                else
                {
                    create_message(&msg, MESSAGE_TEXT, "server", "client", "Username does not exist. Please try again.");
                    send_message(req->socket, &msg);
                }
            }
        }
        else
        {
            // username exists, authenticate credentials
            create_message(&msg, MESSAGE_TEXT, "server", "client", "Enter your password: ");
            send_message(req->socket, &msg);

            nbytes = recv(req->socket, buffer, sizeof(buffer), 0);
            if (nbytes <= 0)
            {
                goto cleanup;
            }

            parse_message(&msg, buffer);
            printf("Received password: %s\n", msg.payload);

            char password[MAX_PASSWORD_LENGTH];
            snprintf(password, MAX_PASSWORD_LENGTH, "%.*s", MAX_PASSWORD_LENGTH - 1, msg.payload);

            char* password_hash = generate_password_hash(password);

            sqlite3_finalize(stmt);
            stmt = NULL;
            sql = "SELECT uid FROM users WHERE username = ? AND password_hash = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
            {
                fprintf(stderr, "Can't prepare user authentication query: %s\n", sqlite3_errmsg(db));
                goto cleanup;
            }

            if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK ||
                sqlite3_bind_text(stmt, 2, (char*)password_hash, PASSWORD_HASH_LENGTH, SQLITE_STATIC) != SQLITE_OK)
            {
                fprintf(stderr, "Can't bind query parameters: %s\n", sqlite3_errmsg(db));
                free(password_hash);
                password_hash = NULL;
                goto cleanup;
            }

            if (sqlite3_step(stmt) != SQLITE_ROW)
            {
                free(password_hash);
                password_hash = NULL;

                fprintf(stderr, "Authentication failed for user %s\n", username);
                attempts++;
                if (attempts == USER_LOGIN_ATTEMPTS)
                    create_message(&msg, MESSAGE_TEXT, "server", "client", "Invalid password.");
                else
                    create_message(&msg, MESSAGE_TEXT, "server", "client", "Invalid password, try again.");
                send_message(req->socket, &msg);

                if (attempts >= USER_LOGIN_ATTEMPTS)
                {
                    goto cleanup;
                }
            }
            else
            {
                free(password_hash);
                password_hash = NULL;

                // successful auth, get UID
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
                printf("Authenticated user %s with UID %s\n", username, cl->uid);

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
                // cl->timestamp = (char*)get_timestamp();
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
