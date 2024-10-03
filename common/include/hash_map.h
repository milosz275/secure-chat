#ifndef __HASH_MAP_H_
#define __HASH_MAP_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>

/**
 * The hash node structure. This structure is used to define the hash node and its operations.
 *
 * @param key The key of the node.
 * @param value The value of the node.
 * @param next The next node in the linked list.
 */
typedef struct hash_node
{
    void* key;
    void* value;
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
 * @param hash_fn The hashing function pointer.
 */
typedef struct hash_map
{
    size_t hash_size;
    hash_bucket* hash_table;
    size_t(*hash_fn)(const void*);
} hash_map;

/**
 * Function to create a hash map. This function will create a hash map with the specified hash size and hash function.
 *
 * @param hash_size The size of the hash map.
 * @param hash_fn The hash function to use.
 * @return The hash map.
 */
hash_map* hash_map_create(size_t hash_size, size_t(*hash_fn)(const void*));

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
 * @param key The key to search for.
 * @param value The value to store the result in.
 */
bool hash_map_find(hash_map* map, const void* key, void** value);

/**
 * Insert an entry into the hash map. This function will insert the entry into the hash map if it does not already exist.
 *
 * @param map The hash map to insert into.
 * @param key The key to insert.
 * @param value The value to insert.
 */
void hash_map_insert(hash_map* map, const void* key, const void* value);

/**
 * Erase an entry from the hash map. This function will remove the entry from the hash map if it exists.
 *
 * @param map The hash map to erase from.
 * @param key The key to erase.
 */
void hash_map_erase(hash_map* map, const void* key);

/**
 * Clear the hash map. This function will remove all entries from the hash map.
 *
 * @param map The hash map to clear.
 */
void hash_map_clear(hash_map* map);

#endif 
