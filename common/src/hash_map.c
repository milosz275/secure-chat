#include "hash_map.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "protocol.h"
#include "log.h"

hash_map* hash_map_create(size_t hash_size)
{
    hash_map* map = (hash_map*)malloc(sizeof(hash_map));
    if (!map)
        return NULL;

    map->hash_size = hash_size;
    map->hash_table = (hash_bucket*)calloc(hash_size, sizeof(hash_bucket));
    if (!map->hash_table)
    {
        if (map)
        {
            free(map);
            map = NULL;
        }
        return NULL;
    }
    map->current_elements = 0;

    for (size_t i = 0; i < hash_size; ++i)
    {
        if (pthread_mutex_init(&map->hash_table[i].mutex, NULL) != 0)
        {
            for (size_t j = 0; j < i; ++j)
                pthread_mutex_destroy(&map->hash_table[j].mutex);
            if (map->hash_table)
            {
                free(map->hash_table);
                map->hash_table = NULL;
            }
            if (map)
            {
                free(map);
                map = NULL;
            }
            return NULL;
        }
        map->hash_table[i].head = NULL;
    }

    return map;
}

void hash_map_destroy(hash_map* map)
{
    if (!map) return;

    for (size_t i = 0; i < map->hash_size; ++i)
    {
        hash_node* node = map->hash_table[i].head;
        while (node)
        {
            hash_node* next = node->next;
            if (node->cl)
                node->cl = NULL;
            if (node)
            {
                free(node);
                node = NULL;
            }
            node = next;
        }
        pthread_mutex_destroy(&map->hash_table[i].mutex);
    }

    if (map->hash_table)
    {
        free(map->hash_table);
        map->hash_table = NULL;
    }
    if (map)
    {
        free(map);
        map = NULL;
    }
}

bool hash_map_find(hash_map* map, const char* uid, client_connection** cl)
{
    // Directly use the uid to compute the index (uid is already hashed)
    size_t hash_value = strtoul(uid, NULL, 16) % map->hash_size;

    pthread_mutex_lock(&map->hash_table[hash_value].mutex);
    hash_node* node = map->hash_table[hash_value].head;

    while (node)
    {
        if (node->cl && node->cl->uid && node->cl->req)
        {
            if (strcmp(node->cl->uid, uid) == 0)
            {
                *cl = node->cl;
                pthread_mutex_unlock(&map->hash_table[hash_value].mutex);
                return true;
            }
            node = node->next;
        }
        else
        {
            log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Found a client with NULL fields");
            pthread_mutex_unlock(&map->hash_table[hash_value].mutex);
            return false;
        }
    }

    pthread_mutex_unlock(&map->hash_table[hash_value].mutex);
    return false;
}

int hash_map_insert(hash_map* map, client_connection* cl)
{
    int insert_success = 0;
    const char* uid = cl->uid;

    // Directly use the uid to compute the index (uid is already hashed)
    if (!uid)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Client UID is NULL");
        return insert_success;
    }
    size_t hash_value = strtoul(uid, NULL, 16) % map->hash_size;

    pthread_mutex_lock(&map->hash_table[hash_value].mutex);
    hash_node* prev = NULL;
    hash_node* node = map->hash_table[hash_value].head;

    while (node && node->cl && node->cl->uid && strcmp(node->cl->uid, uid) != 0)
    {
        prev = node;
        node = node->next;
    }

    if (!node)
    {  // New entry
        hash_node* new_node = (hash_node*)malloc(sizeof(hash_node));
        new_node->cl = cl;  // Use client connection pointer directly
        new_node->next = NULL;

        if (!map->hash_table[hash_value].head)
            map->hash_table[hash_value].head = new_node;
        else
            prev->next = new_node;
        map->current_elements++;
        insert_success = 1;
    }
    else
    { // Existing entry - overwriting existing client connection needs to be handled by the caller
        node->cl = cl;
        insert_success = 2;
    }

    pthread_mutex_unlock(&map->hash_table[hash_value].mutex);
    return insert_success;
}

void hash_map_erase(hash_map* map, const char* uid)
{
    // Directly use the uid to compute the index (uid is already hashed)
    size_t hash_value = strtoul(uid, NULL, 16) % map->hash_size;

    pthread_mutex_lock(&map->hash_table[hash_value].mutex);
    hash_node* prev = NULL;
    hash_node* node = map->hash_table[hash_value].head;

    while (node && strcmp(node->cl->uid, uid) != 0)
    {
        prev = node;
        node = node->next;
    }

    if (node)
    {
        if (!prev)
            map->hash_table[hash_value].head = node->next;
        else
            prev->next = node->next;

        map->current_elements--;
        node->cl = NULL;
        if (node)
        {
            free(node);
            node = NULL;
        }
    }

    pthread_mutex_unlock(&map->hash_table[hash_value].mutex);
}

void hash_map_clear(hash_map* map)
{
    for (size_t i = 0; i < map->hash_size; ++i)
    {
        pthread_mutex_lock(&map->hash_table[i].mutex);
        hash_node* node = map->hash_table[i].head;
        while (node)
        {
            hash_node* next = node->next;
            if (node->cl)
            {
                free(node->cl);
                node->cl = NULL;
            }
            if (node)
            {
                free(node);
                node = NULL;
            }
            node = next;
        }
        map->hash_table[i].head = NULL;
        pthread_mutex_unlock(&map->hash_table[i].mutex);
    }
}

void hash_map_iterate(hash_map* map, void (*callback)(client_connection*))
{
    for (size_t i = 0; i < map->hash_size; ++i)
    {
        pthread_mutex_lock(&map->hash_table[i].mutex);  // Lock the bucket
        hash_node* node = map->hash_table[i].head;
        while (node)
        {
            callback(node->cl);  // Perform operation on the client connection
            node = node->next;
        }
        pthread_mutex_unlock(&map->hash_table[i].mutex);  // Unlock the bucket
    }
}

void hash_map_iterate2(hash_map* map, void (*callback)(client_connection*, void*), void* param)
{
    for (size_t i = 0; i < map->hash_size; ++i)
    {
        pthread_mutex_lock(&map->hash_table[i].mutex);  // Lock the bucket
        hash_node* node = map->hash_table[i].head;
        while (node)
        {
            callback(node->cl, param);  // Pass the client additional parameter
            node = node->next;
        }
        pthread_mutex_unlock(&map->hash_table[i].mutex);  // Unlock the bucket
    }
}
