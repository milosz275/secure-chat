#ifndef __HASH_MAP_H_
#define __HASH_MAP_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>

#include "protocol.h"

/**
 * The hash node structure. This structure is used to define the hash node and its operations.
 *
 * @param cl The client connection structure.
 * @param next The next node in the linked list.
 */
typedef struct hash_node
{
    client_connection_t* cl;  // Store client connection
    struct hash_node* next;
} hash_node;

/**
 * The hash bucket structure. This structure is used to define the hash bucket and its operations.
 *
 * @param head The head of the linked list.
 * @param mutex The mutex for synchronization.
 */
typedef struct hash_bucket
{
    hash_node* head;
    pthread_mutex_t mutex;
} hash_bucket;

/**
 * The hash map structure. This structure is used to define the hash map and its operations.
 *
 * @param hash_size The size of the hash map.
 * @param hash_table The hash table.
 * @param current_elements The current number of elements in the hash map.
 */
typedef struct hash_map
{
    size_t hash_size;
    hash_bucket* hash_table;
    size_t current_elements;
} hash_map;

/**
 * Function to create a hash map. This function will create a hash map with the specified hash size.
 *
 * @param hash_size The size of the hash map.
 * @return The hash map.
 */
hash_map* hash_map_create(size_t hash_size);

/**
 * Function to destroy a hash map and free allocated memory.
 *
 * @param map The hash map to destroy.
 */
void hash_map_destroy(hash_map* map);

/**
 * Find an entry in the hash map. This function will find the entry in the hash map if it exists.
 *
 * @param map The hash map to search.
 * @param uid The uid to search for (already hashed).
 * @param cl The client connection pointer to store the result in.
 * @return True if the entry was found, false otherwise.
 */
bool hash_map_find(hash_map* map, const char* uid, client_connection_t** cl);

/**
 * Insert an entry into the hash map. This function will insert the entry into the hash map if it does not already exist.
 *
 * @param map The hash map to insert into.
 * @param cl The client connection to insert (uses its `uid` as the key).
 * @return True if the entry was inserted, false otherwise.
 */
int hash_map_insert(hash_map* map, client_connection_t* cl);

/**
 * Erase an entry from the hash map. This function will remove the entry from the hash map if it exists.
 *
 * @param map The hash map to erase from.
 * @param uid The uid to erase (already hashed).
 */
void hash_map_erase(hash_map* map, const char* uid);

/**
 * Clear the hash map. This function will remove all entries from the hash map.
 *
 * @param map The hash map to clear.
 */
void hash_map_clear(hash_map* map);

/**
 * Iterate over the hash map. This function will iterate over all entries in the hash map and call the specified callback function.
 *
 * @param map The hash map to iterate over.
 * @param callback The callback function to call for each entry.
 */
void hash_map_iterate(hash_map* map, void (*callback)(client_connection_t*));

/**
 * Iterate over the hash map with a parameter. This function will iterate over all entries in the hash map and call the specified callback function with a parameter.
 *
 * @param map The hash map to iterate over.
 * @param callback The callback function to call for each entry.
 * @param param The parameter to pass to the callback function.
 */
void hash_map_iterate2(hash_map* map, void (*callback)(client_connection_t*, void*), void* param);

#endif
