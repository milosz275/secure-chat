#ifndef __CLIENT_GUI_H
#define __CLIENT_GUI_H

#include <stdio.h>
#include <signal.h>
#include <raylib.h>

#include "client.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
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
 */
typedef struct client_state_t
{
    int just_joined;
    int is_entering_username;
    int is_entering_password;
    int is_confirming_password;
    int is_choosing_register;
    int is_connected;
    int is_authenticated;
    int auth_attempts;
} client_state_t;

/**
 * The button structure. This structure is used to define the button structure.
 *
 * @param rect The rectangle.
 * @param color The color.
 * @param text The text.
 */
typedef struct button_t
{
    Rectangle rect;
    Color color;
    char text[BUTTON_TEXT_LENGTH];
} button_t;

/**
 * Initialize button. This function is used to initialize the button.
 *
 * @param button The button structure.
 * @param rect The rectangle.
 * @param color The color.
 */
void init_button(button_t* button, Rectangle rect, Color color);

/**
 * Button hovered state. This function is used to check if the button is hovered.
 *
 * @param button The button structure.
 * @return 1 if the button is hovered, 0 otherwise.
 */
int is_button_hovered(button_t* button);

/**
 * Button clicked state. This function is used to check if the button is clicked.
 *
 * @param button The button structure.
 * @return 1 if the button is clicked, 0 otherwise.
 */
int is_button_clicked(button_t* button);

/**
 * Initialize UI. This function is used to initialize the user interface using Raylib library.
 */
void init_ui();

/**
 * Draw UI. This function is used to draw the user interface using Raylib library.
 *
 * @param client The client structure.
 * @param state The client state structure.
 * @param reconnect_flag The reconnect flag.
 * @param quit_flag The quit flag.
 */
void ui_cycle(client_t* client, client_state_t* client_state, volatile sig_atomic_t* reconnect_flag, volatile sig_atomic_t* quit_flag);

/**
 * Draw UI. This function is used to draw the user interface using Raylib library.
 *
 * @param client The client structure.
 * @param state The client state structure.
 * @param reconnect_flag The reconnect flag.
 * @param quit_flag The quit flag.
 */
void draw_ui(client_t* client, client_state_t* state, volatile sig_atomic_t reconnect_flag, volatile sig_atomic_t quit_flag);

void add_message(const char* formatted_message);

/**
 * Reset state. This function is used to reset the client state.
 *
 * @param state The client state.
 */
void reset_state(client_state_t* state);

/**
 * Disable input. This function is used to disable input from the command line interface.
 */
void disable_input();

#endif
