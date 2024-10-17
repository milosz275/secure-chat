#include "client_gui.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <time.h>
#include <raylib.h>

#include "protocol.h"
#include "client.h"
#include "log.h"

Font font;
Texture2D app_logo;
Texture2D app_favicon;

char messages[10][MAX_MESSAGE_LENGTH] = { 0 };
int message_count = 0;
time_t last_backspace_press = 0;

int dark_mode = 0;
button_t button_dark_mode = { 0 };

int window_width = WINDOW_WIDTH;
int window_height = WINDOW_HEIGHT;

void init_button(button_t* button, Rectangle rect, Color color)
{
    button->rect = rect;
    button->color = color;
}

int is_button_hovered(button_t* button)
{
    return CheckCollisionPointRec(GetMousePosition(), button->rect);
}

int is_button_clicked(button_t* button)
{
    return is_button_hovered(button) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

void init_ui()
{
    InitWindow(window_width, window_height, "Secure Chat Client");
    SetTargetFPS(TARGET_FPS);
    SetExitKey(0);
    SetTraceLogLevel(LOG_NONE);
    SetTraceLogCallback(NULL);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetWindowFocused();
    font = LoadFontEx("assets/JetBrainsMono.ttf", FONT_SIZE, NULL, 0);
    init_button(&button_dark_mode, (Rectangle) { 10, 10, FAVICON_SIZE, FAVICON_SIZE }, RED);

    Image logo_image = LoadImage("assets/logo.png");
    app_logo = LoadTextureFromImage(logo_image);
    UnloadImage(logo_image);

    logo_image = LoadImage("assets/android-chrome-512x512.png");
    SetWindowIcon(logo_image);
    app_favicon = LoadTextureFromImage(logo_image);
    app_favicon.width = app_favicon.height = FAVICON_SIZE;
    UnloadImage(logo_image);
}

void ui_cycle(client_t* client, client_state_t* client_state, volatile sig_atomic_t* reconnect_flag, volatile sig_atomic_t* quit_flag, const char* log_filename)
{
    // exit conditions
    if (*quit_flag)
    {
        CloseWindow();
        return;
    }

    // update
    window_width = GetScreenWidth();
    window_height = GetScreenHeight();

    if (is_button_hovered(&button_dark_mode))
    {
        button_dark_mode.color = GRAY;
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    }
    else
    {
        SetMouseCursor(MOUSE_CURSOR_ARROW);
        if (dark_mode)
            button_dark_mode.color = WHITE;
        else
            button_dark_mode.color = BLACK;
    }
    if (is_button_clicked(&button_dark_mode))
        dark_mode = !dark_mode;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
    {
        message_t msg;
        if (client_state->is_entering_username)
        {
            client->username[0] = '\0';
            char truncated_username[MAX_USERNAME_LENGTH];
            snprintf(truncated_username, sizeof(truncated_username), "%.*s", MAX_USERNAME_LENGTH - 1, client->input);
            strncpy(client->username, truncated_username, MAX_USERNAME_LENGTH);
            client->username[MAX_USERNAME_LENGTH] = '\0';
            create_message(&msg, MESSAGE_AUTH, client->uid, "server", client->input);
        }
        else if (client_state->is_entering_password || client_state->is_confirming_password)
            create_message(&msg, MESSAGE_AUTH, client->uid, "server", client->input);
        else if (client_state->is_choosing_register)
            create_message(&msg, MESSAGE_CHOICE, client->uid, "server", client->input);
        else
            create_message(&msg, MESSAGE_TEXT, client->uid, "server", client->input);

        if (send_message(client->ssl, &msg) != MESSAGE_SEND_SUCCESS)
        {
            log_message(T_LOG_ERROR, log_filename, __FILE__, "Send failed. Server might be disconnected.");
            *reconnect_flag = 1;
            client->input[0] = '\0';
            return;
        }
        if (client_state->is_authenticated)
        {
            log_message(T_LOG_INFO, log_filename, __FILE__, "Message sent to %s: %s", msg.recipient_uid, client->input);
            add_message(client->username, client->input);
        }
        client->input[0] = '\0';
    }
    else if (IsKeyPressed(KEY_BACKSPACE) || IsKeyDown(KEY_BACKSPACE))
    {
        time_t current_time = clock();
        int elapsed_time = (current_time - last_backspace_press) * 1000 / CLOCKS_PER_SEC;

        if (elapsed_time > BACKSPACE_DELAY)
        {
            int len = strlen(client->input);
            if (len > 0)
            {
                client->input[len - 1] = '\0';
                last_backspace_press = current_time;
            }
        }
    }
    else if (IsKeyPressed(KEY_ESCAPE))
    {
        *reconnect_flag = 0;
        *quit_flag = 1;
        client->input[0] = '\0';
        for (int i = 0; i < 10; ++i)
            messages[i][0] = '\0';
        message_count = 0;
        CloseWindow();
        return;
    }
    else
    {
        int len = strlen(client->input);
        if (len < MAX_INPUT_LENGTH - 1)
        {
            char c = GetCharPressed();
            if (c != 0)
            {
                if (len < MAX_INPUT_LENGTH - 1)
                {
                    client->input[len] = c;
                    client->input[len + 1] = '\0';
                }
                else
                    log_message(T_LOG_WARN, log_filename, __FILE__, "Input buffer overflow");
            }
        }
    }
    // draw
    BeginDrawing();
    draw_ui(client, client_state, *reconnect_flag, *quit_flag);
    EndDrawing();
}

void draw_ui(client_t* client, client_state_t* state, volatile sig_atomic_t reconnect_flag, volatile sig_atomic_t quit_flag)
{
    Color text_color_light = DARKGRAY;
    Color text_color = BLACK;
    Color bg_color = RAYWHITE;
    if (dark_mode)
    {
        text_color_light = LIGHTGRAY;
        text_color = WHITE;
        bg_color = (Color){ 13, 17, 23, 255 };
    }
    ClearBackground(bg_color);

    DrawTexture(app_favicon, 10, 10, WHITE);
    DrawTextEx(font, "Secure Chat", (Vector2) { 10.0 + button_dark_mode.rect.width + 10.0, 10.0 + button_dark_mode.rect.height / 2 - FONT_SIZE / 2 }, FONT_SIZE, FONT_SPACING, text_color);

    if (reconnect_flag)
    {
        DrawTextEx(font, "Reconnecting...", (Vector2) { 100.0, 100.0 }, FONT_SIZE, FONT_SPACING, text_color_light);
        return;
    }
    else if (quit_flag)
    {
        DrawTextEx(font, "Exiting...", (Vector2) { 100.0, 100.0 }, FONT_SIZE, FONT_SPACING, text_color_light);
        return;
    }
    else if (!state->is_authenticated)
    {
        if (state->can_register && !state->just_joined && !reconnect_flag)
            DrawTextEx(font, "You can register.", (Vector2) { window_width - 370.0, window_height - 30.0 }, FONT_SIZE, FONT_SPACING, text_color_light);
        if (state->auth_attempts >= 0 && !state->just_joined && !reconnect_flag && state->is_connected)
        {
            char auth_attempts_str[12];
            sprintf(auth_attempts_str, "%d", state->auth_attempts);
            DrawTextEx(font, "Auth attempts: ", (Vector2) { window_width - 175.0, window_height - 30.0 }, FONT_SIZE, FONT_SPACING, text_color_light);
            DrawTextEx(font, auth_attempts_str, (Vector2) { window_width - 20.0, window_height - 30.0 }, FONT_SIZE, FONT_SPACING, text_color);
        }

        if (state->is_entering_username)
        {
            DrawTextEx(font, message_code_to_text(MESSAGE_CODE_ENTER_USERNAME), (Vector2) { 100.0, 100.0 }, FONT_SIZE, FONT_SPACING, text_color_light);
            DrawTextEx(font, client->input, (Vector2) { 320.0, 100.0 }, FONT_SIZE, FONT_SPACING, text_color);
        }
        else if (state->is_entering_password)
        {
            DrawTextEx(font, message_code_to_text(MESSAGE_CODE_ENTER_USERNAME), (Vector2) { 100.0, 100.0 }, FONT_SIZE, FONT_SPACING, text_color_light);
            DrawTextEx(font, client->username, (Vector2) { 320.0, 100.0 }, FONT_SIZE, FONT_SPACING, text_color);

            DrawTextEx(font, message_code_to_text(MESSAGE_CODE_ENTER_PASSWORD), (Vector2) { 100.0, 150.0 }, FONT_SIZE, FONT_SPACING, text_color_light);
            DrawTextEx(font, client->input, (Vector2) { 320.0, 150.0 }, FONT_SIZE, FONT_SPACING, text_color);
        }
        else if (state->is_confirming_password)
        {
            DrawTextEx(font, message_code_to_text(MESSAGE_CODE_ENTER_USERNAME), (Vector2) { 100.0, 100.0 }, FONT_SIZE, FONT_SPACING, text_color_light);
            DrawTextEx(font, client->username, (Vector2) { 320.0, 100.0 }, FONT_SIZE, FONT_SPACING, text_color);

            DrawTextEx(font, message_code_to_text(MESSAGE_CODE_ENTER_PASSWORD_CONFIRMATION), (Vector2) { 100.0, 150.0 }, FONT_SIZE, FONT_SPACING, text_color_light);
            DrawTextEx(font, client->input, (Vector2) { 345.0, 150.0 }, FONT_SIZE, FONT_SPACING, text_color);
        }
        else if (state->is_choosing_register)
        {
            DrawTextEx(font, message_code_to_text(MESSAGE_CODE_USER_DOES_NOT_EXIST), (Vector2) { 100.0, 100.0 }, FONT_SIZE, FONT_SPACING, text_color_light);
            DrawTextEx(font, client->username, (Vector2) { 320.0, 100.0 }, FONT_SIZE, FONT_SPACING, text_color);

            DrawTextEx(font, message_code_to_text(MESSAGE_CODE_USER_REGISTER_CHOICE), (Vector2) { 100.0, 150.0 }, FONT_SIZE, FONT_SPACING, text_color_light);
            DrawTextEx(font, client->input, (Vector2) { 470.0, 150.0 }, FONT_SIZE, FONT_SPACING, text_color);
        }
        else
        {
            if (state->just_joined)
            {
                DrawTexture(app_logo, window_width / 2 - app_logo.width / 2, window_height / 2 - app_logo.height / 2, WHITE);
                DrawTextEx(font, message_code_to_text(MESSAGE_CODE_WELCOME), (Vector2) { (float)window_width / 2 - 25, (float)window_height - 40 }, FONT_SIZE, FONT_SPACING, text_color_light);
            }
            else
                DrawTextEx(font, "Loading...", (Vector2) { (float)window_width / 2 - 25, (float)window_height / 2 - 25 }, FONT_SIZE, FONT_SPACING, text_color_light);
        }
    }
    else
    {
        DrawTextEx(font, "Enter message:", (Vector2) { 100.0, 500.0 }, FONT_SIZE, FONT_SPACING, text_color_light);
        DrawTextEx(font, client->input, (Vector2) { 255.0, 500.0 }, FONT_SIZE, FONT_SPACING, text_color);

        for (int i = 0; i < message_count; ++i)
            DrawTextEx(font, messages[i], (Vector2) { 100.0, 100.0 + i * 30 }, FONT_SIZE, FONT_SPACING, text_color);
    }
    DrawTextEx(font, "Press ESC to exit", (Vector2) { (float)window_width - 200, 10.0 + button_dark_mode.rect.height / 2 - FONT_SIZE / 2 }, FONT_SIZE, FONT_SPACING, text_color_light);
}

void add_message(const char* sender, const char* payload)
{
    char temp[MAX_MESSAGE_LENGTH];
    snprintf(temp, MAX_MESSAGE_LENGTH, "%s: %s", sender, payload);
    strncpy(messages[message_count], temp, MAX_MESSAGE_LENGTH);
    messages[message_count][MAX_MESSAGE_LENGTH - 1] = '\0';
    message_count = (message_count + 1) % 10;
}

void reset_state(client_state_t* state)
{
    state->is_entering_username = 0;
    state->is_entering_password = 0;
    state->is_confirming_password = 0;
    state->is_choosing_register = 0;
    state->is_authenticated = 0;
    state->auth_attempts = 0;
    state->can_register = 0;
}

void disable_cli_input()
{
    struct termios newt;
    tcgetattr(STDIN_FILENO, &newt);
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}
