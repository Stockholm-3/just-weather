#ifndef CACHE_H
#define CACHE_H
#include <time.h>
#include <stddef.h>

typedef struct CacheEntry{
    void *key;
    void *value;
    time_t timestamp;
    struct CacheEntry *next;
}CacheEntry;

typedef struct{
    CacheEntry *head;
    size_t size;
    int ttl;
    int(*compare_func)(const void *a, const void *b);
    void(*free_key)(void *key);
    void(*free_value)(void *value);
}Cache;

Cache *cache_create(int (*compare_func)(const void *, const void *),
    void (*free_key)(void *),
    void (*free_value)(void *),
    int ttl);

    void cache_put(Cache *cache, void *key, void *value);
    void *cache_get(Cache *cache, const void *key);
    void cache_destroy(Cache *cache);

    extern Cache *global_cache;

    void cache_global_init(void);
    void cache_global_destroy(void);

    int cache_string_compare(const void *a, const void *b);



#endif