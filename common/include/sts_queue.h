#ifndef __STS_QUEUE_H
#define __STS_QUEUE_H

#include <pthread.h>

#include "protocol.h"

/**
 * The STS queue element structure. This structure is used to define the STS queue element.
 */
typedef struct sts_element
{
    void* next;
    message_t* value;
} sts_element;

/**
 * The STS queue header structure. This structure is used to define the STS queue header.
 */
typedef struct sts_header
{
    sts_element* head;
    sts_element* tail;
    pthread_mutex_t* mutex;
} sts_header;

/**
 * The STS queue structure. This structure is used to define the STS queue and its operations.
 *
 * This simple thread-safe queue is adopted from https://github.com/petercrona/StsQueue.
 *
 * @param create Create a new STS queue.
 * @param destroy Destroy the STS queue.
 * @param push Push a message to the STS queue.
 * @param pop Pop a message from the STS queue.
 */
typedef struct
{
    sts_header* (* const create)();
    void (* const destroy)(sts_header* handle);
    void (* const push)(sts_header* handle, message_t* elem);
    message_t* (* const pop)(sts_header* handle);
} _sts_queue;

extern _sts_queue const sts_queue;

#endif
