#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Compare two strings (for string keys) */
int cache_string_compare(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

/* Create a new cache */
Cache *cache_create(int (*compare_func)(const void *, const void *),
                    void (*free_key)(void *),
                    void (*free_value)(void *),
                    int ttl) {
    Cache *cache = malloc(sizeof(Cache));
    if (!cache) return NULL;

    cache->head = NULL;
    cache->size = 0;
    cache->ttl = ttl;
    cache->compare_func = compare_func;
    cache->free_key = free_key;
    cache->free_value = free_value;

    return cache;
}

/* Add or update a key-value pair */
void cache_put(Cache *cache, void *key, void *value) {
    if (!cache || !key) return;

    CacheEntry *entry = cache->head;

    // Check if key exists → update value
    while (entry) {
        if (cache->compare_func(entry->key, key) == 0) {
            if (cache->free_value) cache->free_value(entry->value);
            entry->value = value;
            entry->timestamp = time(NULL);
            return;
        }
        entry = entry->next;
    }

    // Otherwise, insert new entry at head
    CacheEntry *new_entry = malloc(sizeof(CacheEntry));
    new_entry->key = key;
    new_entry->value = value;
    new_entry->timestamp = time(NULL);
    new_entry->next = cache->head;
    cache->head = new_entry;
    cache->size++;
}

/* Retrieve a value by key */
void *cache_get(Cache *cache, const void *key) {
    if (!cache || !key) return NULL;

    CacheEntry *entry = cache->head;
    time_t now = time(NULL);

    while (entry) {
        if (cache->compare_func(entry->key, key) == 0) {
            // Check TTL expiration
            if (cache->ttl > 0 && (now - entry->timestamp) > cache->ttl)
                return NULL;
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

/* Free all memory used by cache */
void cache_destroy(Cache *cache) {
    if (!cache) return;

    CacheEntry *entry = cache->head;
    while (entry) {
        CacheEntry *next = entry->next;
        if (cache->free_key) cache->free_key(entry->key);
        if (cache->free_value) cache->free_value(entry->value);
        free(entry);
        entry = next;
    }
    free(cache);
}

/* Global cache */
Cache *global_cache = NULL;

void cache_global_init(void) {
    global_cache = cache_create(cache_string_compare, free, free, 60);
    if (global_cache)
        printf("[Cache] Global cache initialized.\n");
}

void cache_global_destroy(void) {
    cache_destroy(global_cache);
    global_cache = NULL;
    printf("[Cache] Global cache destroyed.\n");
}