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

#include "protocol.h"
#include "server_cli.h"
#include "server_db.h"
#include "server_auth.h"
#include "log.h"
#include "sts_queue.h"

volatile sig_atomic_t quit_flag = 0;
extern _sts_queue const sts_queue;
extern sts_header* create();
static struct client_connections_t clients = { PTHREAD_MUTEX_INITIALIZER, {NULL} };
static struct server_t server = { 0, {0}, 0, 0, 0, PTHREAD_MUTEX_INITIALIZER, {0}, NULL };
void usleep(unsigned int usec);

int srv_exit(char** args)
{
    if (setsockopt(server.socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1)
        perror("setsockopt failed");
    if (close(server.socket) == -1)
        perror("close failed");
    server.socket = -1;
    quit_flag = 1;
    exit(0);
    if (args[0] != NULL) {}
    return 1;
}

int srv_ban(char** args)
{
    // [ ] Implement ban command
    if (args[0] != NULL) {}
    return 1;
}

int srv_kick(char** args)
{
    // [ ] Implement kick command
    if (args[0] != NULL) {}
    return 1;
}

int srv_mute(char** args)
{
    // [ ] Implement mute command
    if (args[0] != NULL) {}
    return 1;
}

void* handle_client(void* arg)
{
    // before authentication log to requests.log
    char log_msg[MAX_LOG_LENGTH];
    char buffer[BUFFER_SIZE];
    int nbytes;
    request_t* req_ptr = ((request_t*)arg);
    request_t req = *req_ptr;
    message_t msg;
    client_connection_t cl;
    server.requests_handled++;
    cl.is_ready = 0;

    sprintf(log_msg, "Handing request %s:%d", inet_ntoa(req.address.sin_addr), ntohs(req.address.sin_port));
    log_message(LOG_INFO, REQUESTS_LOG, __FILE__, log_msg);

    if (user_auth(&req, &cl) != USER_AUTHENTICATION_SUCCESS)
    {
        close(req.socket);
        pthread_exit(NULL);
    }

    pthread_mutex_lock(&clients.mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (!clients.array[i])
        {
            cl.id = i;
            log_msg[0] = '\0';
            sprintf(log_msg, "Client %d added to client array", cl.id);
            log_message(LOG_INFO, CLIENTS_LOG, __FILE__, log_msg);
            clients.array[i] = &cl;
            break;
        }
    }
    pthread_mutex_unlock(&clients.mutex);

    // from this point log to clients.log
    server.client_logins_handled++;

    log_msg[0] = '\0';
    sprintf(log_msg, "Successful auth of client - id: %d - username: %s - address: %s:%d - uid: %s", cl.id, cl.username, inet_ntoa(req.address.sin_addr), ntohs(req.address.sin_port), cl.uid);
    log_message(LOG_INFO, CLIENTS_LOG, __FILE__, log_msg);

    // simplified message router for join messages
    pthread_mutex_lock(&clients.mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients.array[i] && clients.array[i] != &cl)
        {
            create_message(&msg, MESSAGE_USER_JOIN, "server", "client", cl.username);
            send_message(clients.array[i]->request->socket, &msg);
        }
    }
    pthread_mutex_unlock(&clients.mutex);

    cl.is_ready = 1;
    create_message(&msg, MESSAGE_PING, "server", "client", "PING");
    send_message(cl.request->socket, &msg);
    while ((nbytes = recv(cl.request->socket, buffer, sizeof(buffer), 0)) > 0)
    {
        if (quit_flag)
            break;

        if (nbytes > 0)
        {
            parse_message(&msg, buffer);
            char log_buf[BUFFER_SIZE];
            sprintf(log_buf, "Received message from client %d, %s: %s", cl.id, msg.sender_uid, msg.payload);
            log_message(LOG_INFO, CLIENTS_LOG, __FILE__, log_buf);

            if (msg.type == MESSAGE_PING)
            {
                create_message(&msg, MESSAGE_ACK, "server", msg.sender_uid, "ACK");
                send_message(cl.request->socket, &msg);
            }
            else if (msg.type == MESSAGE_ACK)
            {
                // do nothing
            }
            else
            {
                // simplified broadcast message router for text messages
                char payload[MAX_PAYLOAD_SIZE];
                strncpy(payload, msg.payload, MAX_PAYLOAD_SIZE);
                payload[MAX_PAYLOAD_SIZE - 1] = '\0';

                pthread_mutex_lock(&clients.mutex);
                for (int i = 0; i < MAX_CLIENTS; ++i)
                {
                    if (clients.array[i] && clients.array[i] != &cl)
                    {
                        char id_str[20];
                        snprintf(id_str, sizeof(id_str), "%d", cl.id);
                        create_message(&msg, MESSAGE_TEXT, id_str, "client", payload);
                        send_message(clients.array[i]->request->socket, &msg);
                    }
                }
                pthread_mutex_unlock(&clients.mutex);
            }
        }
        else
        {
            log_msg[0] = '\0';
            sprintf(log_msg, "Received malformed message from client %d", cl.id);
            log_message(LOG_INFO, CLIENTS_LOG, __FILE__, log_msg);
        }
    }

    close(cl.request->socket);
    pthread_exit(NULL);
}

void* handle_cli(void* arg)
{
    char* line = NULL;

    usleep(100000); // 100 ms
    while (!quit_flag)
    {
        printf("server> ");
        line = srv_read_line();
        srv_exec_line(line);
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
    // approach: use condition variable to signal new message in queue
    while (!quit_flag)
    {
        // messages from queue must be malloced, e.g. message_t* msg = (message_t*)malloc(sizeof(message_t));
        message_t* msg = sts_queue.pop(server.message_queue);
        if (msg)
        {
            // [ ] Implement message handling
            free(msg);
        }
        usleep(100000); // 100 ms
    }
    if (arg) {}
    pthread_exit(NULL);
}

void* handle_info_update(void* arg)
{
    struct sysinfo sys_info;
    char log_msg[MAX_LOG_LENGTH];
    char formatted_uptime[9];
    while (!quit_flag)
    {
        int user_count = 0;
        pthread_mutex_lock(&clients.mutex);
        for (int i = 0; i < MAX_CLIENTS; ++i)
            if (clients.array[i])
                user_count++;
        pthread_mutex_unlock(&clients.mutex);

        if (!sysinfo(&sys_info))
        {
            format_uptime(sys_info.uptime, formatted_uptime, sizeof(formatted_uptime));
            sprintf(log_msg, "Online: %d, Req: %d, Auths: %d, Uptime: %s, Load avg: %.2f, RAM: %lu/%lu MB",
                user_count,
                server.requests_handled,
                server.client_logins_handled,
                formatted_uptime,
                sys_info.loads[0] / 65536.0,
                (sys_info.totalram - sys_info.freeram) / 1024 / 1024,
                sys_info.totalram / 1024 / 1024);
            log_message(LOG_INFO, SERVER_LOG, __FILE__, log_msg);
        }
        else
        {
            log_message(LOG_ERROR, SERVER_LOG, __FILE__, "Failed to get system info");
        }
        sleep(10);
    }
    if (arg) {}
    pthread_exit(NULL);
}

int run_server()
{
    if (get_nprocs() == 2)
    {
        log_message(LOG_WARN, SERVER_LOG, __FILE__, "Server running on a dual core system");
    }
    else if (get_nprocs() == 1)
    {
        log_message(LOG_ERROR, SERVER_LOG, __FILE__, "Tried to run server on a single core system");
        return SINGLE_CORE_SYSTEM;
    }

    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    request_t req;
    init_logging(SERVER_LOG);
    log_message(LOG_INFO, SERVER_LOG, __FILE__, "Server started");

    sqlite3* db;
    if (setup_db(&db, DB_NAME) != DATABASE_CREATE_SUCCESS)
    {
        return DATABASE_SETUP_FAILURE;
    }
    log_message(LOG_INFO, SERVER_LOG, __FILE__, "Database setup successful");

    server.socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server.socket < 0)
    {
        perror("Socket creation failed");
        log_message(LOG_ERROR, SERVER_LOG, __FILE__, "Socket creation failed. Server shutting down");
        finish_logging();
        exit(EXIT_FAILURE);
    }
    server.address.sin_family = AF_INET;
    server.address.sin_addr.s_addr = INADDR_ANY;
    server.address.sin_port = htons(PORT);
    server.message_queue = sts_queue.create();

    char log_msg[MAX_LOG_LENGTH];
    if (server.address.sin_family == AF_INET)
        sprintf(log_msg, "IPv4 socket created with address %s and port %d", inet_ntoa(server.address.sin_addr), PORT);
    else if (server.address.sin_family == AF_INET6)
        sprintf(log_msg, "IPv6 socket created with address %s and port %d", inet_ntoa(server.address.sin_addr), PORT);
    else
        sprintf(log_msg, "Socket created with unknown address family and port %d", PORT);
    log_message(LOG_INFO, SERVER_LOG, __FILE__, log_msg);

    int attempts = 0;
    int bind_success = 0;
    while (attempts < PORT_BIND_ATTEMPTS)
    {
        if (bind(server.socket, (struct sockaddr*)&server.address, sizeof(server.address)) < 0)
        {
            system("clear");
            perror("Bind failed");
            printf("Retrying in %d seconds... (Attempt %d/%d)\n",
                PORT_BIND_INTERVAL, attempts + 1, PORT_BIND_ATTEMPTS);
            attempts++;
            if (attempts < PORT_BIND_ATTEMPTS)
            {
                if (quit_flag)
                {
                    close(server.socket);
                    log_message(LOG_INFO, SERVER_LOG, __FILE__, "Server shutting down");
                    finish_logging();
                    exit(EXIT_SUCCESS);
                }
                sleep(PORT_BIND_INTERVAL);
            }
            else
            {
                close(server.socket);
                log_message(LOG_ERROR, SERVER_LOG, __FILE__, "Could not bind to the server address. Server shutting down");
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
        log_message(LOG_ERROR, SERVER_LOG, __FILE__, "Could not bind to the server address");
        finish_logging();
        exit(EXIT_FAILURE);
    }

    if (listen(server.socket, 10) < 0)
    {
        perror("Listen failed");
        close(server.socket);
        log_message(LOG_INFO, SERVER_LOG, __FILE__, "Listen failed. Server shutting down");
        finish_logging();
        exit(EXIT_FAILURE);
    }
    log_msg[0] = '\0';
    sprintf(log_msg, "Server listening on port %d", PORT);
    log_message(LOG_INFO, SERVER_LOG, __FILE__, log_msg);

    pthread_t cli_thread;
    if (pthread_create(&cli_thread, NULL, handle_cli, (void*)NULL) != 0)
    {
        log_msg[0] = '\0';
        snprintf(log_msg, sizeof(log_msg), "CLI thread creation failed: %s", strerror(errno));
        log_message(LOG_WARN, SERVER_LOG, __FILE__, log_msg);
    }
    log_message(LOG_INFO, SERVER_LOG, __FILE__, "CLI thread started");
    server.thread_count++;

    pthread_t info_update_thread;
    if (pthread_create(&info_update_thread, NULL, handle_info_update, (void*)NULL) != 0)
    {
        log_msg[0] = '\0';
        snprintf(log_msg, sizeof(log_msg), "Info update thread creation failed: %s", strerror(errno));
        log_message(LOG_WARN, SERVER_LOG, __FILE__, log_msg);
    }
    log_message(LOG_INFO, SERVER_LOG, __FILE__, "Info update thread started");
    server.thread_count++;

    pthread_t msg_queue_thread;
    if (pthread_create(&msg_queue_thread, NULL, handle_msg_queue, (void*)NULL) != 0)
    {
        log_msg[0] = '\0';
        snprintf(log_msg, sizeof(log_msg), "Message queue handler thread creation failed: %s", strerror(errno));
        log_message(LOG_WARN, SERVER_LOG, __FILE__, log_msg);
    }
    log_message(LOG_INFO, SERVER_LOG, __FILE__, "Message queue handler thread started");
    server.thread_count++;

    init_logging(REQUESTS_LOG);
    init_logging(CLIENTS_LOG);
    log_message(LOG_INFO, REQUESTS_LOG, __FILE__, "Server started");
    log_message(LOG_INFO, CLIENTS_LOG, __FILE__, "Server started");

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
                log_message(LOG_INFO, SERVER_LOG, __FILE__, "Server shutting down");
                finish_logging();
                exit(EXIT_SUCCESS);
            }
            perror("Accept failed");
            continue;
        }

        req.socket = client_socket;
        req.address = client_addr;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, (void*)&req) != 0)
        {
            char error_msg[128];
            snprintf(error_msg, sizeof(error_msg), "Thread creation failed: %s", strerror(errno));

            log_msg[0] = '\0';
            snprintf(log_msg, sizeof(log_msg), "Thread creation failed for client %s: %s", inet_ntoa(client_addr.sin_addr), error_msg);
            log_message(LOG_WARN, SERVER_LOG, __FILE__, log_msg);
            close(client_socket);
            continue;
        }
        pthread_mutex_lock(&server.thread_count_mutex);
        if (server.thread_count < MAX_CLIENTS)
        {
            server.threads[server.thread_count++] = tid;
            server.thread_count++;
        }
        pthread_mutex_unlock(&server.thread_count_mutex);

        usleep(200000); // 200 ms
    }

    for (int i = 0; i < server.thread_count; ++i)
    {
        pthread_join(server.threads[i], NULL);
    }

    sts_queue.destroy(server.message_queue);
    close(server.socket);
    pthread_join(cli_thread, NULL);
    pthread_join(info_update_thread, NULL);
    pthread_join(msg_queue_thread, NULL);
    log_message(LOG_INFO, SERVER_LOG, __FILE__, "Server shutting down");
    finish_logging();
    exit(EXIT_SUCCESS);
}
