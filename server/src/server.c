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
static struct server srv = { 0, {0}, 0, 0, 0, PTHREAD_MUTEX_INITIALIZER, {0}, NULL, NULL, NULL, NULL, 0 };

void usleep(unsigned int usec);
char* strdup(const char* str1);

int srv_exit(char** args)
{
    if (args[0] != NULL)
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Arguments provided for exit command ignored");
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Server shutdown requested");
    quit_flag = 1;
    return 1;
}

int srv_list(char** args)
{
    if (args[0] != NULL)
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Arguments provided for list command ignored");
    int user_count = srv.client_map->current_elements;
    printf("Users online: %d\n", user_count);
    if (user_count > 0)
    {
        hash_map_iterate(srv.client_map, print_client);
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
    client_connection* cl = (client_connection*)malloc(sizeof(client_connection));
    if (!cl)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Srv kick: client memory allocation failed");
        return -1;
    }
    int recipient_found = hash_map_find(srv.client_map, args[0], &cl);
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
    hash_map_iterate(srv.client_map, send_quit_signal);
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
    if (!concatenated_args)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Srv broadcast: string memory allocation failed");
        return -1;
    }

    concatenated_args[0] = '\0';
    for (int i = 0; args[i] != NULL; ++i)
    {
        strcat(concatenated_args, args[i]);
        if (args[i + 1] != NULL)
            strcat(concatenated_args, " ");
    }

    hash_map_iterate2(srv.client_map, send_broadcast, concatenated_args);

    free(concatenated_args);
    return 1;
}

void print_client(client_connection* cl)
{
    printf("ID: %d, Username: %s, Address: %s:%d, UID: %s\n", cl->id, cl->username, inet_ntoa(cl->req->addr.sin_addr), ntohs(cl->req->addr.sin_port), cl->uid);
}

void send_ping(client_connection* cl)
{
    if (cl->is_ready)
    {
        log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Sending PING to client %d", cl->id);
        message* msg = (message*)malloc(sizeof(message));
        if (!msg)
        {
            log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Send ping: message memory allocation failed");
            return;
        }
        create_message(msg, MESSAGE_PING, "server", cl->uid, "PING");
        sts_queue.push(srv.message_queue, msg);
    }
}

void kick_unresponsive(client_connection* cl)
{
    if (cl->is_ready && cl->ping_sent)
    {
        log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Kicking unresponsive client %d", cl->id);
        send_quit_signal(cl);
        sleep(5);
        hash_map_erase(srv.client_map, cl->uid);
    }
}

void send_quit_signal(client_connection* cl)
{
    if (cl->is_ready)
    {
        log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Sending quit signal to client %d", cl->id);
        message* msg = (message*)malloc(sizeof(message));
        if (!msg)
        {
            log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Send quit signal: message memory allocation failed");
            return;
        }
        create_message(msg, MESSAGE_SIGNAL, "server", cl->uid, MESSAGE_SIGNAL_QUIT);
        sts_queue.push(srv.message_queue, msg);
    }
}

void send_broadcast(client_connection* cl, void* arg)
{
    char* msg_payload = (char*)arg;
    if (cl->is_ready)
    {
        log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Broadcasting message to client %d: %s", cl->id, msg_payload);
        message* msg = (message*)malloc(sizeof(message));
        if (!msg)
        {
            log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Send broadcast: message memory allocation failed");
            return;
        }
        create_message(msg, MESSAGE_TEXT, "server", cl->uid, msg_payload);
        sts_queue.push(srv.message_queue, msg);
    }
}

void send_join_message(client_connection* cl, void* arg)
{
    client_connection* new_cl = (client_connection*)arg;
    if (cl->is_ready && cl != new_cl)
    {
        log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Sending join message to client %d", cl->id);
        message* msg = (message*)malloc(sizeof(message));
        if (!msg)
        {
            log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Send join message: message memory allocation failed");
            return;
        }
        create_message(msg, MESSAGE_USER_JOIN, "server", cl->uid, new_cl->username);
        sts_queue.push(srv.message_queue, msg);
    }
}

void* handle_client(void* arg)
{
    // before authentication log to requests.log
    char buffer[BUFFER_SIZE];
    int nbytes;
    request* req = (request*)arg;
    message msg;
    client_connection cl;
    srv.requests_handled++;
    cl.is_ready = 0;
    cl.is_inserted = 0;
    log_message(T_LOG_INFO, REQUESTS_LOG, __FILE__, "Handing request %s:%d", inet_ntoa(req->addr.sin_addr), ntohs(req->addr.sin_port));

    if (user_auth(req, &cl, srv.client_map) != USER_AUTHENTICATION_SUCCESS)
    {
        close(req->sock);
        free(req);
        pthread_exit(NULL);
    }

    if (!hash_map_insert(srv.client_map, &cl))
    {
        log_message(T_LOG_ERROR, CLIENTS_LOG, __FILE__, "Failed to insert client into client map");
        close(req->sock);
        free(req);
        pthread_exit(NULL);
    }
    cl.is_inserted = 1;
    cl.id = srv.client_map->current_elements - 1;
    log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "%s added to client array", cl.username);

    // from this point log to client_connections.log
    srv.client_logins_handled++;
    log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Successful auth of client - id: %d - username: %s - address: %s:%d - uid: %s", cl.id, cl.username, inet_ntoa(req->addr.sin_addr), ntohs(req->addr.sin_port), cl.uid);

    // send join message to all clients except the new one
    hash_map_iterate2(srv.client_map, send_join_message, &cl);

    cl.is_ready = 1;
    while ((nbytes = SSL_read(cl.req->ssl, buffer, sizeof(buffer))) > 0)
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
                send_message(cl.req->ssl, &msg);
            }
            else if (msg.type == MESSAGE_ACK)
            {
                log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Received ACK from client %d", cl.id);
                cl.ping_sent = 0;
            }
            else
            {
                message* new_msg = (message*)malloc(sizeof(message));
                if (!new_msg)
                {
                    log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Failed to allocate memory for message that should be enqueued for handling");
                    continue;
                }
                memcpy(new_msg, &msg, sizeof(message));
                sts_queue.push(srv.message_queue, new_msg);
            }
        }
        else
            log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Received malformed message from client %d", cl.id);
    }
    // client disconnected
    log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, "Client %d disconnected", cl.id);
    if (cl.is_inserted)
        hash_map_erase(srv.client_map, cl.uid);
    close(cl.req->sock);
    free(req);
    pthread_exit(NULL);
}

void* handle_client_ping(void* arg)
{
    while (!quit_flag)
    {
        hash_map_iterate(srv.client_map, send_ping);
        sleep(15);
        hash_map_iterate(srv.client_map, kick_unresponsive);
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
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Exiting CLI thread");
    pthread_exit(NULL);
}

void* handle_msg_queue(void* arg)
{
    // approach: pop one message at the time and sleep briefly
    // NOTE: use only for authenticated users with UID. router will not handle messages with "client" sender or recipient
    while (!quit_flag)
    {
        message* msg = sts_queue.pop(srv.message_queue);
        if (msg)
        {
            const char* payload_copy = strdup(msg->payload);
            log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Message from %s to %s: %s", msg->sender_uid, msg->recipient_uid, (char*)payload_copy);
            client_connection* cl = (client_connection*)malloc(sizeof(client_connection));
            if (!cl)
            {
                log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Msg queue: client memory allocation failed");
                continue;
            }
            int recipient_found = hash_map_find(srv.client_map, msg->recipient_uid, &cl);
            if (recipient_found)
            {
                send_message(cl->req->ssl, msg);
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
        usleep(50000); // 50 ms
    }
    if (arg) {}
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Exiting msg queue thread");
    pthread_exit(NULL);
}

void* handle_info_update(void* arg)
{
    struct sysinfo sys_info;
    char formatted_srv_uptime[9];
    char formatted_sys_uptime[9];
    while (!quit_flag)
    {
        int user_count = srv.client_map->current_elements;

        if (!sysinfo(&sys_info))
        {
            time_t current_time = time(NULL);
            long uptime_seconds = (long)difftime(current_time, srv.start_time);
            format_uptime(uptime_seconds, formatted_srv_uptime, sizeof(formatted_srv_uptime));
            format_uptime(sys_info.uptime, formatted_sys_uptime, sizeof(formatted_sys_uptime));
            log_message(T_LOG_INFO, SYSTEM_LOG, __FILE__, "Online: %d, Req: %d, Auths: %d, Uptime: %s, Sys-uptime: %s, Load avg: %.2f, RAM: %lu/%lu MB",
                user_count,
                srv.requests_handled,
                srv.client_logins_handled,
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
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Exiting info update thread");
    pthread_exit(NULL);
}

void* handle_connection_add(void* arg)
{
    int cl_sock;
    struct sockaddr_in cl_addr;
    socklen_t cl_len = sizeof(cl_addr);

    while (!quit_flag)
    {
        if (srv.thread_count >= MAX_CLIENTS || srv.thread_count >= MAX_THREADS)
        {
            usleep(200000); // 200 ms
            continue;
        }

        cl_sock = accept(srv.sock, (struct sockaddr*)&cl_addr, &cl_len);
        if (cl_sock < 0)
        {
            if (quit_flag)
            {
                close(srv.sock);
                log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Server shutting down");
                finish_logging();
                exit(EXIT_SUCCESS);
            }
            if (errno == EINTR)
                continue;
            perror("Accept failed");
            continue;
        }

        SSL* client_ssl = SSL_new(srv.ssl_ctx);
        if (!client_ssl)
        {
            log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Failed to create SSL object for client");
            close(cl_sock);
            continue;
        }
        SSL_set_fd(client_ssl, cl_sock);
        if (SSL_accept(client_ssl) <= 0)
        {
            log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "SSL handshake failed");
            SSL_free(client_ssl);
            close(cl_sock);
            continue;
        }
        request* req = (request*)malloc(sizeof(request)); // this should be freed in handle_client thread
        if (!req)
        {
            log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Request memory allocation failed");
            SSL_free(client_ssl);
            close(cl_sock);
            continue;
        }
        req->sock = cl_sock;
        req->addr = cl_addr;
        req->ssl = client_ssl;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, (void*)req) != 0)
        {
            char error_msg[128];
            snprintf(error_msg, sizeof(error_msg), "Thread creation failed: %s", strerror(errno));
            log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Thread creation failed for client %s: %s", inet_ntoa(cl_addr.sin_addr), error_msg);
            close(cl_sock);
            continue;
        }
        pthread_mutex_lock(&srv.thread_count_mutex);
        if (srv.thread_count < MAX_CLIENTS)
        {
            srv.threads[srv.thread_count] = tid;
            srv.thread_count++;
        }
        pthread_mutex_unlock(&srv.thread_count_mutex);

        usleep(100000); // 100 ms
    }
    if (arg) {}
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Exiting connection add thread");
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

    init_logging(SERVER_LOG);
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, LOG_SERVER_STARTED);

    sqlite3* db;
    if (setup_db(&db, DB_NAME) != DATABASE_CREATE_SUCCESS)
    {
        finish_logging();
        return DATABASE_SETUP_FAILURE;
    }
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Database setup successful");

    srv.sock = socket(AF_INET, SOCK_STREAM, 0);
    if (srv.sock < 0)
    {
        perror("Socket creation failed");
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Socket creation failed. Server shutting down");
        finish_logging();
        exit(EXIT_FAILURE);
    }
    srv.addr.sin_family = AF_INET;
    srv.addr.sin_addr.s_addr = INADDR_ANY;
    srv.addr.sin_port = htons(PORT);
    if (init_ssl(&srv) != OPENSSL_INIT_SUCCESS)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "SSL initialization failed. Server shutting down");
        finish_logging();
        exit(EXIT_FAILURE);
    }
    srv.message_queue = sts_queue.create();
    srv.client_map = hash_map_create(MAX_CLIENTS);
    srv.start_time = time(NULL);

    if (srv.addr.sin_family == AF_INET)
        log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "IPv4 socket created with address %s and port %d", inet_ntoa(srv.addr.sin_addr), PORT);
    else if (srv.addr.sin_family == AF_INET6)
        log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "IPv6 socket created with address %s and port %d", inet_ntoa(srv.addr.sin_addr), PORT);
    else
        log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Socket created with unknown address family and port %d", PORT);

    int attempts = 0;
    int bind_success = 0;
    while (attempts < PORT_BIND_ATTEMPTS)
    {
        if (bind(srv.sock, (struct sockaddr*)&srv.addr, sizeof(srv.addr)) < 0)
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
                    close(srv.sock);
                    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Server shutting down");
                    finish_logging();
                    exit(EXIT_SUCCESS);
                }
                sleep(PORT_BIND_INTERVAL);
            }
            else
            {
                close(srv.sock);
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
        close(srv.sock);
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Could not bind to the server address");
        finish_logging();
        exit(EXIT_FAILURE);
    }

    if (listen(srv.sock, 10) < 0)
    {
        perror("Listen failed");
        close(srv.sock);
        log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Listen failed. Server shutting down");
        finish_logging();
        exit(EXIT_FAILURE);
    }
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Server listening on port %d", PORT);

    pthread_t cli_thread;
    if (pthread_create(&cli_thread, NULL, handle_cli, (void*)NULL) != 0)
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "CLI thread creation failed: %s", strerror(errno));
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "CLI thread started");
    srv.threads[srv.thread_count] = cli_thread;
    srv.thread_count++;

    init_logging(SYSTEM_LOG);
    log_message(T_LOG_INFO, SYSTEM_LOG, __FILE__, LOG_SERVER_STARTED);
    pthread_t info_update_thread;
    if (pthread_create(&info_update_thread, NULL, handle_info_update, (void*)NULL) != 0)
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Info update thread creation failed: %s", strerror(errno));
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Info update thread started");
    srv.threads[srv.thread_count] = info_update_thread;
    srv.thread_count++;

    pthread_t msg_queue_thread;
    if (pthread_create(&msg_queue_thread, NULL, handle_msg_queue, (void*)NULL) != 0)
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Message queue handler thread creation failed: %s", strerror(errno));
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Message queue handler thread started");
    srv.threads[srv.thread_count] = msg_queue_thread;
    srv.thread_count++;

    pthread_t client_ping_thread;
    if (pthread_create(&client_ping_thread, NULL, handle_client_ping, (void*)NULL) != 0)
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Client ping thread creation failed: %s", strerror(errno));
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Client ping thread started");
    srv.threads[srv.thread_count] = client_ping_thread;
    srv.thread_count++;

    init_logging(REQUESTS_LOG);
    init_logging(CLIENTS_LOG);
    log_message(T_LOG_INFO, REQUESTS_LOG, __FILE__, LOG_SERVER_STARTED);
    log_message(T_LOG_INFO, CLIENTS_LOG, __FILE__, LOG_SERVER_STARTED);

    pthread_t connection_add_thread;
    if (pthread_create(&connection_add_thread, NULL, handle_connection_add, (void*)NULL) != 0)
    {
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Connection add thread creation failed: %s", strerror(errno));
        close(srv.sock);
        finish_logging();
        exit(EXIT_FAILURE);
    }
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Connection add thread started");
    srv.threads[srv.thread_count] = connection_add_thread;
    srv.thread_count++;

    while (!quit_flag)
        usleep(100000); // 100 ms

    // exit
    for (int i = 0; i < srv.thread_count; ++i)
        pthread_join(srv.threads[i], NULL);

    sts_queue.destroy(srv.message_queue);
    hash_map_destroy(srv.client_map);
    destroy_ssl(&srv);
    close(srv.sock);

    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Server shutting down");
    finish_logging();
    exit(EXIT_SUCCESS);
}
