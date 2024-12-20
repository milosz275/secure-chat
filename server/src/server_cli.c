#include "server_cli.h"

#include <readline/readline.h>
#include <readline/history.h>

#include "protocol.h"
#include "log.h"

// Server command array
static server_command srv_commands[] =
{
    {.srv_command = &srv_exit, .srv_command_name = "!exit", .srv_command_description = "Stops the server." },
    {.srv_command = &srv_exit, .srv_command_name = "!quit", .srv_command_description = "Exit command alias." },
    {.srv_command = &srv_exit, .srv_command_name = "!stop", .srv_command_description = "Exit command alias." },
    {.srv_command = &srv_exit, .srv_command_name = "!shutdown", .srv_command_description = "Exit command alias." },
    {.srv_command = &srv_history, .srv_command_name = "!history", .srv_command_description = "Prints command history." },
    {.srv_command = &srv_clear, .srv_command_name = "!clear", .srv_command_description = "Clears CLI screen." },
    {.srv_command = &srv_list, .srv_command_name = "!list", .srv_command_description = "Lists authenticated users." },
    {.srv_command = &srv_ban, .srv_command_name = "!ban", .srv_command_description = "Bans given user by UID." },
    {.srv_command = &srv_mute, .srv_command_name = "!mute", .srv_command_description = "Mutes given user by UID." },
    {.srv_command = &srv_kick, .srv_command_name = "!kick", .srv_command_description = "Kicks given user by UID." },
    {.srv_command = &srv_kick_all, .srv_command_name = "!kickall", .srv_command_description = "Kicks all authenticated users." },
    {.srv_command = &srv_broadcast, .srv_command_name = "!broadcast", .srv_command_description = "Broadcast given message to all authenticated users." },
    {.srv_command = &srv_help, .srv_command_name = "!help", .srv_command_description = "Prints command descriptions." },
};

int srv_history(char** args)
{
    if (args[0] != NULL)
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Arguments provided for history command ignored");
    HIST_ENTRY** history_entries = history_list();
    if (history_entries)
    {
        for (int i = 0; history_entries[i] != NULL; ++i)
            printf("%d: %s\n", i + 1, history_entries[i]->line);
    }
    return 1;
}

int srv_help(char** args)
{
    if (args[0] != NULL)
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Arguments provided for help command ignored"); // [ ] Add help for each command (e.g. !help !ban)
    for (int i = 0; i < SRV_COMMANDS_NUM; ++i)
        printf("%-11s- %s\n", srv_commands[i].srv_command_name, srv_commands[i].srv_command_description);
    return 1;
}

int srv_clear(char** args)
{
    if (args[0] != NULL)
        log_message(T_LOG_WARN, SERVER_LOG, __FILE__, "Arguments provided for clear command ignored");
    clear_cli();
    return 1;
}

int getline(char** lineptr, size_t* n, FILE* stream)
{
    static char line[MAX_LINE_LENGTH];
    char* ptr;
    unsigned int len;

    if (lineptr == NULL || n == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (ferror(stream))
        return -1;

    if (feof(stream))
        return -1;

    (void)fgets(line, 256, stream);

    ptr = strchr(line, '\n');
    if (ptr)
        *ptr = '\0';

    len = strlen(line);

    if ((len + 1) < 256)
    {
        ptr = realloc(*lineptr, 256);
        if (ptr == NULL)
            return(-1);
        *lineptr = ptr;
        *n = 256;
    }

    strcpy(*lineptr, line);
    return(len);
}

char* srv_read_line(void)
{
    char* line = readline("server> ");
    if (line && *line)
        add_history(line);
    return line;
}

int srv_exec_line(char* line)
{
    int i = 0;
    int buffer_size = SRV_BUFSIZE;
    char** tokens = malloc(buffer_size * sizeof(char*));
    char* token;

    char* command;
    char** args;

    if (!tokens)
    {
        fprintf(stderr, "srv_exec_line: malloc error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, SRV_DELIM);
    while (token != NULL)
    {
        tokens[i] = token;
        i++;

        if (i >= buffer_size)
        {
            buffer_size += SRV_BUFSIZE;
            tokens = realloc(tokens, buffer_size * sizeof(char*));
            if (!tokens)
            {
                fprintf(stderr, "srv_exec_line: malloc error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, SRV_DELIM);
    }
    tokens[i] = NULL;

    command = tokens[0];
    args = tokens + 1;

    if (command == NULL)
    {
        free(tokens);
        return 1;
    }

    for (i = 0; i < SRV_COMMANDS_NUM; ++i)
    {
        if (!strcmp(command, srv_commands[i].srv_command_name))
        {
            int result = (srv_commands[i].srv_command)(args);
            free(tokens);
            return result;
        }
    }
    printf("Command not found! Type !help to see available commands.\n");
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Command not found: %s", command);
    free(tokens);
    return 1;
}
