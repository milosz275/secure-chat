#include "sts_queue.h"

#include <stdlib.h>
#include <pthread.h>

#include "protocol.h"

static sts_header* create();
static void destroy(sts_header* header);
static void push(sts_header* header, message* elem);
static message* pop(sts_header* header);

static sts_header* create()
{
    sts_header* handle = malloc(sizeof(*handle));
    handle->head = NULL;
    handle->tail = NULL;

    pthread_mutex_t* mutex = malloc(sizeof(*mutex));
    handle->mutex = mutex;

    return handle;
}

static void destroy(sts_header* header)
{
    free(header->mutex);
    free(header);
}

static void push(sts_header* header, message* elem)
{
    // Create new element
    sts_element* element = malloc(sizeof(*element));
    element->value = elem;
    element->next = NULL;

    pthread_mutex_lock(header->mutex);
    // Is list empty
    if (header->head == NULL)
    {
        header->head = element;
        header->tail = element;
    }
    else
    {
        // Rewire
        sts_element* oldTail = header->tail;
        oldTail->next = element;
        header->tail = element;
    }
    pthread_mutex_unlock(header->mutex);
}

static message* pop(sts_header* header)
{
    pthread_mutex_lock(header->mutex);
    sts_element* head = header->head;

    // Is empty?
    if (head == NULL)
    {
        pthread_mutex_unlock(header->mutex);
        return NULL;
    }
    else
    {
        // Rewire
        header->head = head->next;

        // Get head and free element memory
        message* value = head->value;
        free(head);

        pthread_mutex_unlock(header->mutex);
        return value;
    }
}

_sts_queue const sts_queue =
{
  create,
  destroy,
  push,
  pop
};
