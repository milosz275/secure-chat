#ifndef __SERVER_CLI_H
#define __SERVER_CLI_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define SRV_BUFSIZE 64
#define SRV_DELIM " "
#define MAX_LINE_LENGTH 256

#define SRV_COMMANDS_NUM (int) (sizeof(srv_commands) / sizeof(srv_command_t))

/**
 * Server commands structure.
 *
 * @param srv_command The function pointer to executed function.
 * @param srv_command_name The name of command by which it is called.
 * @param srv_command_description The description of command printed by !help.
 */
typedef struct
{
    int (*srv_command)(char**);
    char* srv_command_name;
    char* srv_command_description;
} srv_command_t;

/**
 * Exit server. This function is used to exit the server.
 *
 * @param args The arguments passed to the function should be empty.
 * @return The exit code.
 */
extern int srv_exit(char** args);

/**
 * Clear CLI. This function is used to clear the CLI.
 *
 * @param args The arguments passed to the function should be empty.
 * @return The exit code.
 */
int srv_clear(char** args);

/**
 * List users. This function is used to list all authenticated users.
 *
 * @param args The arguments passed to the function should be empty.
 * @return The exit code.
 */
extern int srv_list(char** args);

/**
 * Ban user. This function is used to ban a specified user.
 *
 * @param args The arguments passed to this function should contain the username to ban.
 * @return The exit code.
 */
extern int srv_ban(char** args);

/**
 * Kick user. This function is used to kick a specified user.
 *
 * @param args The arguments passed to this function should contain the username to kick.
 * @return The exit code.
 */
extern int srv_kick(char** args);

/**
 * Kick all users. This function is used to kick all users.
 *
 * @param args The arguments passed to this function should be empty.
 * @return The exit code.
 */
extern int srv_kick_all(char** args);

/**
 * Mute user. This function is used to mute a specified user.
 *
 * @param args The arguments passed to this function should contain the username to mute.
 * @return The exit code.
 */
extern int srv_mute(char** args);

/**
 * Broadcast message. This function is used to broadcast a message to all users.
 *
 * @param args The arguments passed to this function should contain the message to broadcast.
 * @return The exit code.
 */
extern int srv_broadcast(char** args);

/**
 * Print help. This function prints all available commands.
 *
 * @param args The arguments passed to this function should be empty.
 * @return The exit code.
 */
int srv_help(char** args);

/**
 * Getline function. This function is used to read a line from a file stream.
 *
 * @param lineptr The pointer to the line.
 * @param n The size of the line.
 * @param stream The file stream.
 * @return The number of characters read.
 */
int getline(char** lineptr, size_t* n, FILE* stream);

/**
 * Function reading input from server
 * !!! WARNING !!! char* returned from this function must be freed
 *
 * @return The string containing input from user.
 */
char* srv_read_line(void);

/**
 * Execute command. This function executes a command from line.
 *
 * @param line The string containing input from user.
 * @return The exit code.
 */
int srv_exec_line(char* line);

#endif
