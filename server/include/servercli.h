#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//Server commands
int srv_exit(char**);
int srv_ban(char**);
int srv_kick(char**);
int srv_mute(char**);
int srv_help(char**);

//Server command array
char* builtin_str[] = {
  "!exit",
  "!ban",
  "!kick",
  "!mute",
  "!help"
};

int (*server_commands[]) (char**) = {
    &srv_exit,
    &srv_ban,
    &srv_kick,
    &srv_mute,
    &srv_help
};

//Takes dynamically allocated buffer as input
int srv_read_line(char* line) {
    size_t n;
    if (!(n = (line == NULL)))
        n = strlen(line);
    if (getline(&line, &n, stdin) == -1) {
        perror("getline");
        exit(EXIT_FAILURE);
    }
    return (int)n;
}

//Divide line into tokens - command and args, checks if command exists, if it does executes it and passes args
int srv_exec_line(char* line) {

}