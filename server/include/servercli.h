#ifndef __SERVERCLI_H
#define __SERVERCLI_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define SRV_BUFSIZE 64
#define SRV_DELIM " "

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

int srv_exit(char**);
int srv_ban(char**);
int srv_kick(char**);
int srv_mute(char**);
int srv_help(char**);

//getline() implementation
int getline(char** lineptr, size_t* n, FILE* stream);

/**
 * Function reading input from server
 * !!!WARNING!!! char* returned from this function must be freed
 */
char* srv_read_line(void);

/**
 * Functione executing the input line.
 *
 * @param line The string containing input from user.
 */
int srv_exec_line(char* line);

#endif