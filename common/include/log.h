#ifndef __LOG_H
#define __LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>

#define MAX_LOG_FILES 10
#define MAX_FILENAME_LENGTH 256
#define LOGS_DIR "logs"
#define SERVER_LOG "server.log"
#define CLIENTS_LOG "clients.log"
#define REQUESTS_LOG "requests.log"
#define SYSTEM_LOG "system.log"
#define CLIENT_LOG "client.log"
#define LOG_SERVER_STARTED "Server started ----------------------------------------------------------------------------------------"
#define LOG_CLIENT_STARTED "Client started ----------------------------------------------------------------------------------------"

#define LOCKED_FILE_RETRY_TIME 1000 // in microseconds
#define LOCKED_FILE_TIMEOUT 5000000 // in microseconds (5 seconds)

/**
 * The log level enumeration. This enumeration is used to define the level of logging that is being used. Regular log enums are used by Raylib.
 *
 * @param T_LOG_DEBUG Debug level logging
 * @param T_LOG_INFO Information level logging
 * @param T_LOG_WARN Warning level logging
 * @param T_LOG_ERROR Error level logging
 * @param T_LOG_FATAL Fatal level logging
 */
typedef enum
{
    T_LOG_DEBUG,
    T_LOG_INFO,
    T_LOG_WARN,
    T_LOG_ERROR,
    T_LOG_FATAL
} log_level_t;

/**
 * The logger structure. This structure is used to store the logger for the logging system.
 *
 * @param filename The filename of the log file.
 * @param file The file pointer for the log file.
 */
typedef struct logger_t
{
    char* filename;
    FILE* file;
} logger_t;

/**
 * The loggers structure. This structure is used to store the loggers for the logging system.
 *
 * @param log_mutex The mutex for the loggers.
 * @param array The array of loggers.
 * @param is_initializing The flag to indicate if the loggers are initializing.
 */
typedef struct loggers_t
{
    pthread_mutex_t log_mutex;
    logger_t* array[MAX_LOG_FILES];
    int is_initializing;
} loggers_t;

/**
 * Initialize logging. This function is used to initialize the logging system, open and preserve the log file.
 *
 * @param log_file The log file to write to.
 */
void init_logging(const char* filename);

/**
 * Log a message. This function is used to log a message to the console or a file.
 *
 * @param level The log level.
 * @param log_file The file that the log message is destined for.
 * @param source_file The source file that the log message is from.
 * @param format The format of the log message.
 * @param ... The arguments for the format.
 */
void log_message(log_level_t level, const char* filename, const char* source_file, const char* format, ...);

/**
 * Finish logging. This function is used to finish logging and close the log file.
 */
void finish_logging();

#endif
