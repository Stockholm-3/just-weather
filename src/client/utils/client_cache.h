#ifndef CLIENT_CACHE_H
#define CLIENT_CACHE_H

#include <stddef.h>
#include <time.h>

#define CACHE_MAX_ENTRIES 50
#define CACHE_DEFAULT_TTL 300

typedef struct ClientCache ClientCache;

ClientCache* client_cache_create(size_t max_entries, time_t default_ttl);
void         client_cache_destroy(ClientCache* cache);
int          client_cache_set(ClientCache* cache, const char* key,
                              const char* json_data);
char*        client_cache_get(ClientCache* cache, const char* key);
void         client_cache_clear(ClientCache* cache);

#endif
