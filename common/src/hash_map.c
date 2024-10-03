#include "hash_map.h"

#include <pthread.h>

hash_map* hash_map_create(size_t hash_size, size_t(*hash_fn)(const void*))
{
    hash_map* map = (hash_map*)malloc(sizeof(hash_map));
    if (!map)
        return NULL;

    map->hash_size = hash_size;
    map->hash_table = (hash_bucket*)calloc(hash_size, sizeof(hash_bucket));
    if (!map->hash_table)
    {
        free(map);
        return NULL;
    }

    map->hash_fn = hash_fn;

    // Initialize mutexes for each bucket
    for (size_t i = 0; i < hash_size; ++i)
    {
        if (pthread_mutex_init(&map->hash_table[i].mutex, NULL) != 0)
        {
            // Cleanup on failure
            for (size_t j = 0; j < i; ++j)
                pthread_mutex_destroy(&map->hash_table[j].mutex);
            free(map->hash_table);
            free(map);
            return NULL;
        }
        map->hash_table[i].head = NULL;
    }

    return map;
}

// Function to destroy a hash map and free allocated memory
void hash_map_destroy(hash_map* map)
{
    if (!map) return;

    // Free each bucket's linked list
    for (size_t i = 0; i < map->hash_size; ++i)
    {
        hash_node* node = map->hash_table[i].head;
        while (node)
        {
            hash_node* next = node->next;
            free(node->key);
            free(node->value);
            free(node);
            node = next;
        }
        pthread_mutex_destroy(&map->hash_table[i].mutex);
    }

    free(map->hash_table);
    free(map);
}

bool hash_map_find(hash_map* map, const void* key, void** value)
{
    size_t hash_value = map->hash_fn(key) % map->hash_size;

    pthread_mutex_lock(&map->hash_table[hash_value].mutex);
    hash_node* node = map->hash_table[hash_value].head;

    while (node)
    {
        if (memcmp(node->key, key, strlen(key)) == 0)
        {
            *value = node->value;
            pthread_mutex_unlock(&map->hash_table[hash_value].mutex);
            return true;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&map->hash_table[hash_value].mutex);
    return false;
}

void hash_map_insert(hash_map* map, const void* key, const void* value)
{
    size_t hash_value = map->hash_fn(key) % map->hash_size;

    pthread_mutex_lock(&map->hash_table[hash_value].mutex);
    hash_node* prev = NULL;
    hash_node* node = map->hash_table[hash_value].head;

    while (node && memcmp(node->key, key, strlen(key)) != 0)
    {
        prev = node;
        node = node->next;
    }

    if (!node)
    {  // New entry
        hash_node* new_node = (hash_node*)malloc(sizeof(hash_node));
        new_node->key = malloc(strlen(key) + 1);  // Allocate memory based on the actual size of key
        memcpy(new_node->key, key, strlen(key) + 1);  // Copy key (include null terminator for strings)

        new_node->value = malloc(strlen(value) + 1);  // Allocate memory for the value
        memcpy(new_node->value, value, strlen(value) + 1);  // Copy value

        new_node->next = NULL;

        if (!map->hash_table[hash_value].head)
            map->hash_table[hash_value].head = new_node;
        else
            prev->next = new_node;
    }
    else // Update existing entry
    {
        free(node->value);  // Free the old value
        node->value = malloc(strlen(value) + 1);
        memcpy(node->value, value, strlen(value) + 1);  // Update value
    }

    pthread_mutex_unlock(&map->hash_table[hash_value].mutex);
}

void hash_map_erase(hash_map* map, const void* key)
{
    size_t hash_value = map->hash_fn(key) % map->hash_size;

    pthread_mutex_lock(&map->hash_table[hash_value].mutex);
    hash_node* prev = NULL;
    hash_node* node = map->hash_table[hash_value].head;

    while (node && memcmp(node->key, key, strlen(key)) != 0)
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

        free(node->key);
        free(node->value);
        free(node);
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
            free(node->key);
            free(node->value);
            free(node);
            node = next;
        }
        map->hash_table[i].head = NULL;
        pthread_mutex_unlock(&map->hash_table[i].mutex);
    }
}
