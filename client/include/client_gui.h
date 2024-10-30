#ifndef __CLIENT_GUI_H
#define __CLIENT_GUI_H

#include <stdio.h>
#include <signal.h>
#include <raylib.h>

#include "client.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define TARGET_FPS 60

#define BACKSPACE_DELAY 50
#define FONT_SIZE 20
#define FONT_SPACING 2

#define BUTTON_TEXT_LENGTH 16
#define FAVICON_SIZE 32

/**
 * The client state structure. This structure is used to define the client state structure.
 *
 * @param just_joined The client just joined.
 * @param is_entering_username The client is entering the username.
 * @param is_entering_password The client is entering the password.
 * @param is_confirming_password The client is confirming the password.
 * @param is_choosing_register The client is choosing to register (y/n).
 * @param is_connected The client is connected.
 * @param is_authenticated The client is authenticated.
 * @param auth_attempts The client authentication attempts.
 * @param can_register The client can register.
 */
typedef struct client_state
{
    int just_joined;
    int is_entering_username;
    int is_entering_password;
    int is_confirming_password;
    int is_choosing_register;
    int is_connected;
    int is_authenticated;
    int auth_attempts;
    int can_register;
} client_state;

/**
 * The button structure. This structure is used to define the button structure.
 *
 * @param rect The rectangle.
 * @param color The color.
 * @param text The text.
 */
typedef struct button
{
    Rectangle rect;
    Color color;
    char text[BUTTON_TEXT_LENGTH];
} button;

/**
 * Initialize button. This function is used to initialize the button.
 *
 * @param bt The button structure.
 * @param rect The rectangle.
 * @param col The color.
 */
void init_button(button* bt, Rectangle rect, Color col);

/**
 * Button hovered state. This function is used to check if the button is hovered.
 *
 * @param bt The button structure.
 * @return 1 if the button is hovered, 0 otherwise.
 */
int is_button_hovered(button* bt);

/**
 * Button clicked state. This function is used to check if the button is clicked.
 *
 * @param bt The button structure.
 * @return 1 if the button is clicked, 0 otherwise.
 */
int is_button_clicked(button* bt);

/**
 * Initialize UI. This function is used to initialize the user interface using Raylib library.
 */
void init_ui();

/**
 * Cycle UI. This function is used to update, draw and handle the user interface using Raylib library.
 *
 * @param cl The client structure.
 * @param cl_state The client state structure.
 * @param reconnect_flag The reconnect flag.
 * @param quit_flag The quit flag.
 * @param log_filename The log filename.
 */
void ui_cycle(client* cl, client_state* cl_state, volatile sig_atomic_t* reconnect_flag, volatile sig_atomic_t* quit_flag, const char* log_filename);

/**
 * Draw UI. This function is used to draw singular frame of the user interface using Raylib library.
 *
 * @param cl The client structure.
 * @param cl_state The client state structure.
 * @param reconnect_flag The reconnect flag.
 * @param quit_flag The quit flag.
 */
void draw_ui(client* cl, client_state* cl_state, volatile sig_atomic_t reconnect_flag, volatile sig_atomic_t quit_flag);

/**
 * Add message. This function is used to add a message to the client state's message array.
 * NOTE: Replaced in the future by client-sided message handling using database.
 *
 * @param sender The message sender.
 * @param payload The message payload.
 */
void add_message(const char* sender, const char* payload);

/**
 * Reset state. This function is used to reset the client state.
 *
 * @param cl_state The client state.
 */
void reset_state(client_state* cl_state);

/**
 * Enable input. This function is used to enable input from the command line interface.
 */
void enable_cli_input();

/**
 * Disable input. This function is used to disable input from the command line interface.
 */
void disable_cli_input();

#endif
