#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "protocol.h"

#define NK_IMPLEMENTATION
#include "nuklear.h"

#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define MAX_MEMORY 4096 * 1024 // 4MB

void* receive_messages(void* socket_desc)
{
    int sock = *(int*)socket_desc;
    char buffer[BUFFER_SIZE];
    int nbytes;

    while ((nbytes = recv(sock, buffer, sizeof(buffer), 0)) > 0)
    {
        buffer[nbytes] = '\0';
        printf("%s\n", buffer);
    }

    if (!nbytes)
    {
        printf("Server disconnected\n");
    }
    else
    {
        perror("Recv failed");
    }

    pthread_exit(NULL);
}

void run_client()
{
    // // Initialize Nuklear - template from https://immediate-mode-ui.github.io/Nuklear/doc/index.html#nuklear/example
    // enum {EASY, HARD};
    // static int op = EASY;
    // static float value = 0.6f;

    // struct nk_context ctx;
    // nk_init_fixed(&ctx, calloc(1, MAX_MEMORY), MAX_MEMORY, NULL);
    // if (nk_begin(&ctx, "Show", nk_rect(50, 50, 220, 220), NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_CLOSABLE))
    //     {

    //     nk_layout_row_static(&ctx, 30, 80, 1);
    //     if (nk_button_label(&ctx, "button"))
    //     {

    //     }

    //     nk_layout_row_dynamic(&ctx, 30, 2);
    //     if (nk_option_label(&ctx, "easy", op == EASY))
    //         op = EASY;
    //     if (nk_option_label(&ctx, "hard", op == HARD))
    //         op = HARD;

    //     nk_layout_row_begin(&ctx, NK_STATIC, 30, 2);
    //     {
    //         nk_layout_row_push(&ctx, 50);
    //         nk_label(&ctx, "Volume:", NK_TEXT_LEFT);
    //         nk_layout_row_push(&ctx, 110);
    //         nk_slider_float(&ctx, 0, &value, 1.0f, 0.1f);
    //     }
    //     nk_layout_row_end(&ctx);
    // }
    // nk_end(&ctx);

    // client
    int sock;
    struct sockaddr_in server_addr;
    char input[BUFFER_SIZE];
    message_t msg;
    pthread_t recv_thread;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address/Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
    printf("Connected to server\n");

    if (pthread_create(&recv_thread, NULL, receive_messages, (void*)&sock) != 0)
    {
        perror("Thread creation failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        while (1)
        {
            // auth
            fgets(input, BUFFER_SIZE, stdin);
            input[strcspn(input, "\n")] = 0;
            if (send(sock, input, strlen(input), 0) < 0)
            {
                perror("Send failed");
                break;
            }
        }

        // authed
        printf("Enter recipient ID and message (e.g., recipient_id:message): ");
        fgets(input, BUFFER_SIZE, stdin);
        sscanf(input, "%[^:]:%[^\n]", msg.recipient_uid, msg.message);

        if (send(sock, (void*)&msg, sizeof(message_t), 0) < 0)
        {
            perror("Send failed");
            break;
        }
    }

    close(sock);
}
