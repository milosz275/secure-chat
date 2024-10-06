#ifndef __LOG_H
#define __LOG_H

#include <stdio.h>
#include <pthread.h>

#define MAX_LOG_FILES 10
#define MAX_FILENAME_LENGTH 256
#define MAX_LOG_LENGTH 4096
#define LOGS_DIR "logs"
#define SERVER_LOG "server.log"
#define CLIENTS_LOG "clients.log"
#define REQUESTS_LOG "requests.log"
#define SYSTEM_LOG "system.log"

#define LOCKED_FILE_RETRY_TIME 1000 // in microseconds
#define LOCKED_FILE_TIMEOUT 5000000 // in microseconds (5 seconds)

/**
 * The log level enumeration. This enumeration is used to define the level of logging that is being used.
 *
 * @param LOG_DEBUG Debug level logging
 * @param LOG_INFO Information level logging
 * @param LOG_WARN Warning level logging
 * @param LOG_ERROR Error level logging
 * @param LOG_FATAL Fatal level logging
 */
typedef enum
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
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
 * @param array The array of loggers.
 */
typedef struct loggers_t
{
    pthread_mutex_t log_mutex;
    logger_t* array[MAX_LOG_FILES];
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
 * @param message The log message.
 */
void log_message(log_level_t level, const char* filename, const char* source_file, const char* message);

/**
 * Finish logging. This function is used to finish logging and close the log file.
 */
void finish_logging();

#endif
