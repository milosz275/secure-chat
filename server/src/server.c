#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ctype.h>
#include <sys/sysinfo.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "protocol.h"
#include "server_cli.h"
#include "server_db.h"
#include "server_auth.h"
#include "server_openssl.h"
#include "log.h"
#include "sts_queue.h"

volatile sig_atomic_t quit_flag = 0;
extern _sts_queue const sts_queue;
extern sts_header* create();
static struct server_t server = { 0, {0}, 0, 0, 0, PTHREAD_MUTEX_INITIALIZER, {0}, NULL, NULL, NULL, NULL, 0 };

void usleep(unsigned int usec);
char* strdup(const char* str1);

int srv_exit(char** args)
{
    if (args[0] != NULL)
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Arguments provided for exit command ignored");
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Server shutdown requested");
    if (setsockopt(server.socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1)
        perror("setsockopt failed");
    if (close(server.socket) == -1)
        perror("close failed");
    hash_map_destroy(server.client_map);
    destroy_ssl(&server);
    sts_queue.destroy(server.message_queue);
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Server shutdown complete");
    server.socket = -1;
    quit_flag = 1;
    exit(0);
    return 1;
}

int srv_list(char** args)
{
    if (args[0] != NULL)
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Arguments provided for list command ignored");
    int user_count = server.client_map->current_elements;
    printf("Users online: %d\n", user_count);
    if (user_count > 0)
    {
        hash_map_iterate(server.client_map, print_client);
    }
    return 1;
}

int srv_ban(char** args)
{
    if (args[0] == NULL)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "No argument provided for ban command");
        return -1;
    }
    else if (args[1] != NULL)
    {
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Multiple arguments provided for ban command. Only first argument will be used");
    }
    // [ ] Implement ban command with database support
    return 1;
}

int srv_kick(char** args)
{
    if (args[0] == NULL)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "No argument provided for kick command");
        return -1;
    }
    else if (args[1] != NULL)
    {
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Multiple arguments provided for kick command. Only first argument will be used");
    }
    client_connection_t* cl = (client_connection_t*)malloc(sizeof(client_connection_t));
    int recipient_found = hash_map_find(server.client_map, args[0], &cl);
    if (recipient_found)
        send_quit_signal(cl);
    else
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Recipient not found in client map");
    return 1;
}

int srv_kick_all(char** args)
{
    if (args[0] != NULL)
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Arguments provided for kickall command ignored");
    hash_map_iterate(server.client_map, send_quit_signal);
    return 1;
}

int srv_mute(char** args)
{
    if (args[0] == NULL)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "No argument provided for mute command");
        return -1;
    }
    else if (args[1] != NULL)
    {
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Multiple arguments provided for mute command. Only first argument will be used");
    }
    // [ ] Implement mute command with database support
    return 1;
}

int srv_broadcast(char** args)
{
    if (args[0] == NULL)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "No message provided for broadcast command");
        return -1;
    }
    int total_length = 1;
    for (int i = 0; args[i] != NULL; ++i)
        total_length += strlen(args[i]) + 1;

    char* concatenated_args = (char*)malloc(total_length);
    if (concatenated_args == NULL)
        return -1;

    concatenated_args[0] = '\0';
    for (int i = 0; args[i] != NULL; ++i)
    {
        strcat(concatenated_args, args[i]);
        if (args[i + 1] != NULL)
            strcat(concatenated_args, " ");
    }

    hash_map_iterate2(server.client_map, send_broadcast, concatenated_args);

    free(concatenated_args);
    return 1;
}

void print_client(client_connection_t* cl)
{
    printf("ID: %d, Username: %s, Address: %s:%d, UID: %s\n", cl->id, cl->username, inet_ntoa(cl->request->address.sin_addr), ntohs(cl->request->address.sin_port), cl->uid);
}

void send_ping(client_connection_t* cl)
{
    if (cl->is_ready)
    {
        log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Sending PING to client %d", cl->id);
        message_t* msg = (message_t*)malloc(sizeof(message_t));
        create_message(msg, MESSAGE_PING, "server", cl->uid, "PING");
        sts_queue.push(server.message_queue, msg);
    }
}

void kick_unresponsive(client_connection_t* cl)
{
    if (cl->is_ready && cl->ping_sent)
    {
        log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Kicking unresponsive client %d", cl->id);
        send_quit_signal(cl);
        sleep(5);
        hash_map_erase(server.client_map, cl->uid);
    }
}

void send_quit_signal(client_connection_t* cl)
{
    if (cl->is_ready)
    {
        log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Sending quit signal to client %d", cl->id);
        message_t* msg = (message_t*)malloc(sizeof(message_t));
        create_message(msg, MESSAGE_SIGNAL, "server", cl->uid, MESSAGE_SIGNAL_QUIT);
        sts_queue.push(server.message_queue, msg);
    }
}

void send_broadcast(client_connection_t* cl, void* arg)
{
    char* message = (char*)arg;
    if (cl->is_ready)
    {
        log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Broadcasting message to client %d: %s", cl->id, message);
        message_t* msg = (message_t*)malloc(sizeof(message_t));
        create_message(msg, MESSAGE_TEXT, "server", cl->uid, message);
        sts_queue.push(server.message_queue, msg);
    }
}

void send_join_message(client_connection_t* cl, void* arg)
{
    client_connection_t* new_cl = (client_connection_t*)arg;
    if (cl->is_ready && cl != new_cl)
    {
        log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Sending join message to client %d", cl->id);
        message_t* msg = (message_t*)malloc(sizeof(message_t));
        create_message(msg, MESSAGE_USER_JOIN, "server", cl->uid, new_cl->username);
        sts_queue.push(server.message_queue, msg);
    }
}

void* handle_client(void* arg)
{
    // before authentication log to requests.log
    char buffer[BUFFER_SIZE];
    int nbytes;
    request_t* req_ptr = ((request_t*)arg);
    request_t req = *req_ptr;
    message_t msg;
    client_connection_t cl;
    server.requests_handled++;
    cl.is_ready = 0;
    cl.is_inserted = 0;
    log_message(T_LOG_INFO, REQUESTS_LOG, __FILE__, "Handing request %s:%d", inet_ntoa(req.address.sin_addr), ntohs(req.address.sin_port));

    if (user_auth(&req, &cl, server.client_map) != USER_AUTHENTICATION_SUCCESS)
    {
        close(req.socket);
        pthread_exit(NULL);
    }

    if (!hash_map_insert(server.client_map, &cl))
    {
        log_message(T_LOG_ERROR, CLIENTS_LOG, __FILE__, "Failed to insert client into client map");
        close(req.socket);
        pthread_exit(NULL);
    }
    cl.is_inserted = 1;
    cl.id = server.client_map->current_elements - 1;
    log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "%s added to client array", cl.username);

    // from this point log to client_connections.log
    server.client_logins_handled++;
    log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Successful auth of client - id: %d - username: %s - address: %s:%d - uid: %s", cl.id, cl.username, inet_ntoa(req.address.sin_addr), ntohs(req.address.sin_port), cl.uid);

    // send join message to all clients except the new one
    hash_map_iterate2(server.client_map, send_join_message, &cl);

    cl.is_ready = 1;
    while ((nbytes = SSL_read(cl.request->ssl, buffer, sizeof(buffer))) > 0)
    {
        if (quit_flag)
            break;

        if (nbytes > 0)
        {
            parse_message(&msg, buffer);
            char log_buf[BUFFER_SIZE];
            sprintf(log_buf, "Received message from client %d, %s: %s", cl.id, msg.sender_uid, msg.payload);
            log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, log_buf);

            if (msg.type == MESSAGE_PING)
            {
                create_message(&msg, MESSAGE_ACK, "server", msg.sender_uid, "ACK");
                send_message(cl.request->ssl, &msg);
            }
            else if (msg.type == MESSAGE_ACK)
            {
                log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Received ACK from client %d", cl.id);
                cl.ping_sent = 0;
            }
            else
            {
                message_t* new_msg = (message_t*)malloc(sizeof(message_t));
                memcpy(new_msg, &msg, sizeof(message_t));
                sts_queue.push(server.message_queue, new_msg);
            }
        }
        else
            log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Received malformed message from client %d", cl.id);
    }
    // client disconnected
    log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Client %d disconnected", cl.id);
    if (cl.is_inserted)
        hash_map_erase(server.client_map, cl.uid);
    close(cl.request->socket);
    pthread_exit(NULL);
}

void* handle_client_ping(void* arg)
{
    while (!quit_flag)
    {
        hash_map_iterate(server.client_map, send_ping);
        sleep(15);
        hash_map_iterate(server.client_map, kick_unresponsive);
    }
    if (arg) {}
    pthread_exit(NULL);
}

void* handle_cli(void* arg)
{
    char* line = NULL;
    read_history(SERVER_CLI_HISTORY);
    usleep(100000); // 100 ms
    while (!quit_flag)
    {
        line = srv_read_line();
        if (line && *line)
        {
            srv_exec_line(line);
            write_history(SERVER_CLI_HISTORY);
        }
        if (line)
        {
            free(line);
            line = NULL;
        }
    }
    if (arg) {}
    pthread_exit(NULL);
}

void* handle_msg_queue(void* arg)
{
    // approach: pop one message at the time and sleep briefly
    // NOTE: use only for authenticated users with UID. router will not handle messages with "client" sender or recipient
    while (!quit_flag)
    {
        message_t* msg = sts_queue.pop(server.message_queue);
        if (msg)
        {
            const char* payload_copy = strdup(msg->payload);
            log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Message from %s to %s: %s", msg->sender_uid, msg->recipient_uid, (char*)payload_copy);
            client_connection_t* cl = (client_connection_t*)malloc(sizeof(client_connection_t));
            int recipient_found = hash_map_find(server.client_map, msg->recipient_uid, &cl);
            if (recipient_found)
            {
                send_message(cl->request->ssl, msg);
                if (msg->type == MESSAGE_PING && !strcmp(msg->sender_uid, "server"))
                {
                    log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Sent PING to client %d", cl->id);
                    cl->ping_sent = 1;
                }
            }
            else
                log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Recipient not found in the client map: %s (msg type: %d; msg payload: %s)", msg->recipient_uid, msg->type, msg->payload);
            free(msg);
            free((void*)payload_copy);
        }
        usleep(100000); // 100 ms
    }
    if (arg) {}
    pthread_exit(NULL);
}

void* handle_info_update(void* arg)
{
    struct sysinfo sys_info;
    char formatted_srv_uptime[9];
    char formatted_sys_uptime[9];
    while (!quit_flag)
    {
        int user_count = server.client_map->current_elements;

        if (!sysinfo(&sys_info))
        {
            time_t current_time = time(NULL);
            long uptime_seconds = (long)difftime(current_time, server.start_time);
            format_uptime(uptime_seconds, formatted_srv_uptime, sizeof(formatted_srv_uptime));
            format_uptime(sys_info.uptime, formatted_sys_uptime, sizeof(formatted_sys_uptime));
            log_message(T_LOG_INFO, SYSTEM_LOG, __FILE__, "Online: %d, Req: %d, Auths: %d, Uptime: %s, Sys-uptime: %s, Load avg: %.2f, RAM: %lu/%lu MB",
                user_count,
                server.requests_handled,
                server.client_logins_handled,
                formatted_srv_uptime,
                formatted_sys_uptime,
                sys_info.loads[0] / 65536.0,
                (sys_info.totalram - sys_info.freeram) / 1024 / 1024,
                sys_info.totalram / 1024 / 1024);
        }
        else
            log_message(T_LOG_ERROR, SYSTEM_LOG, __FILE__, "Failed to get system info");
        sleep(10);
    }
    if (arg) {}
    pthread_exit(NULL);
}

int run_server()
{
    if (get_nprocs() == 2)
    {
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Server running on a dual core system");
    }
    else if (get_nprocs() == 1)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Tried to run server on a single core system");
        return SINGLE_CORE_SYSTEM;
    }

    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    init_logging(SERVER_LOG);
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, LOG_SERVER_STARTED);

    sqlite3* db;
    if (setup_db(&db, DB_NAME) != DATABASE_CREATE_SUCCESS)
    {
        finish_logging();
        return DATABASE_SETUP_FAILURE;
    }
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Database setup successful");

    server.socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server.socket < 0)
    {
        perror("Socket creation failed");
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Socket creation failed. Server shutting down");
        finish_logging();
        exit(EXIT_FAILURE);
    }
    server.address.sin_family = AF_INET;
    server.address.sin_addr.s_addr = INADDR_ANY;
    server.address.sin_port = htons(PORT);
    if (init_ssl(&server) != OPENSSL_INIT_SUCCESS)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "SSL initialization failed. Server shutting down");
        finish_logging();
        exit(EXIT_FAILURE);
    }
    server.message_queue = sts_queue.create();
    server.client_map = hash_map_create(MAX_CLIENTS);
    server.start_time = time(NULL);

    if (server.address.sin_family == AF_INET)
        log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "IPv4 socket created with address %s and port %d", inet_ntoa(server.address.sin_addr), PORT);
    else if (server.address.sin_family == AF_INET6)
        log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "IPv6 socket created with address %s and port %d", inet_ntoa(server.address.sin_addr), PORT);
    else
        log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Socket created with unknown address family and port %d", PORT);

    int attempts = 0;
    int bind_success = 0;
    while (attempts < PORT_BIND_ATTEMPTS)
    {
        if (bind(server.socket, (struct sockaddr*)&server.address, sizeof(server.address)) < 0)
        {
            clear_cli();
            perror("Bind failed");
            printf("Retrying in %d seconds... (Attempt %d/%d)\n",
                PORT_BIND_INTERVAL, attempts + 1, PORT_BIND_ATTEMPTS);
            attempts++;
            if (attempts < PORT_BIND_ATTEMPTS)
            {
                if (quit_flag)
                {
                    close(server.socket);
                    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Server shutting down");
                    finish_logging();
                    exit(EXIT_SUCCESS);
                }
                sleep(PORT_BIND_INTERVAL);
            }
            else
            {
                close(server.socket);
                log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Could not bind to the server address. Server shutting down");
                finish_logging();
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
        close(server.socket);
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Could not bind to the server address");
        finish_logging();
        exit(EXIT_FAILURE);
    }

    if (listen(server.socket, 10) < 0)
    {
        perror("Listen failed");
        close(server.socket);
        log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Listen failed. Server shutting down");
        finish_logging();
        exit(EXIT_FAILURE);
    }
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Server listening on port %d", PORT);

    pthread_t cli_thread;
    if (pthread_create(&cli_thread, NULL, handle_cli, (void*)NULL) != 0)
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "CLI thread creation failed: %s", strerror(errno));
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "CLI thread started");
    server.thread_count++;

    init_logging(SYSTEM_LOG);
    log_message(T_LOG_INFO, SYSTEM_LOG, __FILE__, LOG_SERVER_STARTED);
    pthread_t info_update_thread;
    if (pthread_create(&info_update_thread, NULL, handle_info_update, (void*)NULL) != 0)
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Info update thread creation failed: %s", strerror(errno));
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Info update thread started");
    server.thread_count++;

    pthread_t msg_queue_thread;
    if (pthread_create(&msg_queue_thread, NULL, handle_msg_queue, (void*)NULL) != 0)
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Message queue handler thread creation failed: %s", strerror(errno));
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Message queue handler thread started");
    server.thread_count++;

    pthread_t client_ping_thread;
    if (pthread_create(&client_ping_thread, NULL, handle_client_ping, (void*)NULL) != 0)
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Client ping thread creation failed: %s", strerror(errno));
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Client ping thread started");
    server.thread_count++;

    init_logging(REQUESTS_LOG);
    init_logging(CLIENTS_LOG);
    log_message(T_LOG_INFO, REQUESTS_LOG, __FILE__, LOG_SERVER_STARTED);
    log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, LOG_SERVER_STARTED);

    while (!quit_flag)
    {
        if (server.thread_count >= MAX_CLIENTS || server.thread_count >= MAX_THREADS)
        {
            usleep(200000); // 200 ms
            continue;
        }

        client_socket = accept(server.socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0)
        {
            if (quit_flag)
            {
                close(server.socket);
                log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Server shutting down");
                finish_logging();
                exit(EXIT_SUCCESS);
            }
            if (errno == EINTR)
                continue;
            perror("Accept failed");
            continue;
        }

        SSL* client_ssl = SSL_new(server.ssl_ctx);
        if (!client_ssl)
        {
            log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Failed to create SSL object for client");
            close(client_socket);
            continue;
        }
        SSL_set_fd(client_ssl, client_socket);
        if (SSL_accept(client_ssl) <= 0)
        {
            log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "SSL handshake failed");
            SSL_free(client_ssl);
            close(client_socket);
            continue;
        }
        request_t req;
        req.socket = client_socket;
        req.address = client_addr;
        req.ssl = client_ssl;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, (void*)&req) != 0)
        {
            char error_msg[128];
            snprintf(error_msg, sizeof(error_msg), "Thread creation failed: %s", strerror(errno));
            log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Thread creation failed for client %s: %s", inet_ntoa(client_addr.sin_addr), error_msg);
            close(client_socket);
            continue;
        }
        pthread_mutex_lock(&server.thread_count_mutex);
        if (server.thread_count < MAX_CLIENTS)
        {
            server.threads[server.thread_count] = tid;
            server.thread_count++;
        }
        pthread_mutex_unlock(&server.thread_count_mutex);

        usleep(200000); // 200 ms
    }

    for (int i = 0; i < server.thread_count; ++i)
        pthread_join(server.threads[i], NULL);

    sts_queue.destroy(server.message_queue);
    hash_map_destroy(server.client_map);
    destroy_ssl(&server);
    close(server.socket);
    pthread_join(cli_thread, NULL);
    pthread_join(info_update_thread, NULL);
    pthread_join(msg_queue_thread, NULL);
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Server shutting down");
    finish_logging();
    exit(EXIT_SUCCESS);
}
