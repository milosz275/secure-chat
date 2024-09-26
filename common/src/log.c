#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/file.h>

static struct loggers_t loggers = { PTHREAD_MUTEX_INITIALIZER, {NULL} };

void usleep(unsigned int usec);

void init_logging(const char* filename)
{
    pthread_mutex_lock(&loggers.log_mutex);
    for (int i = 0; i < MAX_LOG_FILES; i++)
    {
        if (loggers.array[i] == NULL) {
            loggers.array[i] = (struct logger_t*)malloc(sizeof(logger_t));
            loggers.array[i]->filename = filename;
            loggers.array[i]->file = fopen(filename, "a"); 
            if (loggers.array[i]->file == NULL)
            {
                perror("Failed to open log file");
                free(loggers.array[i]);
                loggers.array[i] = NULL;
            }
            break;
        }
    }
    pthread_mutex_unlock(&loggers.log_mutex); 
}

void log_message(log_level_t level, const char* filename, const char* message)
{
    pthread_mutex_lock(&loggers.log_mutex);

    FILE* log = NULL;
    for (int i = 0; i < MAX_LOG_FILES; i++)
    {
        if (loggers.array[i] != NULL && strcmp(loggers.array[i]->filename, filename) == 0)
        {
            log = loggers.array[i]->file;
            break;
        }
    }

    if (log == NULL)
    {
        pthread_mutex_unlock(&loggers.log_mutex);
        fprintf(stderr, "Log file not found: %s\n", filename);
        return;
    }

    int timeout = 0;
    int error = 0;

    rewind(log);
    error = lockf(fileno(log), F_TLOCK, 0);
    while (error == EACCES || error == EAGAIN)
    {
        usleep(LOCKED_FILE_RETRY_TIME);
        timeout += LOCKED_FILE_RETRY_TIME;

        if (timeout > LOCKED_FILE_TIMEOUT)
        {
            fprintf(stderr, "Log file locking timed out: %s\n", filename);
            pthread_mutex_unlock(&loggers.log_mutex);
            return;
        }
        error = lockf(fileno(log), F_TLOCK, 0);
    }

    fseek(log, 0, SEEK_END);

    const char* level_str;
    switch (level)
    {
    case LOG_DEBUG: level_str = "DEBUG"; break;
    case LOG_INFO: level_str = "INFO"; break;
    case LOG_WARN: level_str = "WARN"; break;
    case LOG_ERROR: level_str = "ERROR"; break;
    case LOG_FATAL: level_str = "FATAL"; break;
    default: level_str = "UNKNOWN"; break;
    }

    fprintf(log, "[%s] %s\n", level_str, message);

    fflush(log);

    rewind(log);
    lockf(fileno(log), F_ULOCK, 0);

    pthread_mutex_unlock(&loggers.log_mutex); 
}

void finish_logging()
{
    pthread_mutex_lock(&loggers.log_mutex);
    for (int i = 0; i < MAX_LOG_FILES; i++)
    {
        if (loggers.array[i] != NULL)
        {
            fclose(loggers.array[i]->file);
            free(loggers.array[i]);
            loggers.array[i] = NULL;
        }
    }
    pthread_mutex_unlock(&loggers.log_mutex); 
}
