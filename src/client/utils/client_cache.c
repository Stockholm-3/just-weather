#include "client_cache.h"

#include "client_list.h"
#include "hash_md5.h"

#include <dirent.h>
#include <errno.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CACHE_DIR "src/client/cache"

typedef struct {
    char*  key;
    char*  json_data;
    time_t created_at;
    time_t ttl;
} CacheEntry;

struct ClientCache {
    LinkedList* entries;
    size_t      max_entries;
    time_t      default_ttl;
};

static void free_cache_entry(CacheEntry* entry) {
    if (entry) {
        free(entry->key);
        free(entry->json_data);
        free(entry);
    }
}

static void ensure_cache_dir() {
    struct stat st;
    if (stat(CACHE_DIR, &st) == -1) {
        mkdir(CACHE_DIR, 0755);
    }
}

static char* get_cache_filepath(const char* key) {
    char hash[HASH_MD5_STRING_LENGTH];
    if (hash_md5_string(key, strlen(key), hash, sizeof(hash)) != 0) {
        return NULL;
    }

    char* filepath = malloc(512);
    if (!filepath) {
        return NULL;
    }

    snprintf(filepath, 512, "%s/%s.json", CACHE_DIR, hash);
    return filepath;
}

static int is_cache_file_valid(const char* filepath, time_t ttl) {
    struct stat file_stat;

    if (stat(filepath, &file_stat) != 0) {
        return 0;
    }

    time_t now = time(NULL);
    double age = difftime(now, file_stat.st_mtime);

    if (age > (double)ttl) {
        return 0;
    }

    return 1;
}

static int save_to_file(const char* key, const char* json_data) {
    ensure_cache_dir();

    char* filepath = get_cache_filepath(key);
    if (!filepath) {
        return -1;
    }

    json_error_t error;
    json_t*      json = json_loads(json_data, 0, &error);
    if (!json) {
        free(filepath);
        return -1;
    }

    int result =
        json_dump_file(json, filepath, JSON_INDENT(2) | JSON_PRESERVE_ORDER);

    json_decref(json);
    free(filepath);

    return result;
}

static char* load_from_file(const char* key, time_t ttl) {
    char* filepath = get_cache_filepath(key);
    if (!filepath) {
        return NULL;
    }

    if (!is_cache_file_valid(filepath, ttl)) {
        unlink(filepath);
        free(filepath);
        return NULL;
    }

    json_error_t error;
    json_t*      json = json_load_file(filepath, 0, &error);
    free(filepath);

    if (!json) {
        return NULL;
    }

    char* json_str = json_dumps(json, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
    json_decref(json);

    return json_str;
}

static void delete_file(const char* key) {
    char* filepath = get_cache_filepath(key);
    if (filepath) {
        unlink(filepath);
        free(filepath);
    }
}

ClientCache* client_cache_create(size_t max_entries, time_t default_ttl) {
    ClientCache* cache = malloc(sizeof(ClientCache));
    if (!cache) {
        return NULL;
    }

    cache->entries = linked_list_create();
    if (!cache->entries) {
        free(cache);
        return NULL;
    }

    cache->max_entries = max_entries > 0 ? max_entries : CACHE_MAX_ENTRIES;
    cache->default_ttl = default_ttl > 0 ? default_ttl : CACHE_DEFAULT_TTL;

    return cache;
}

void client_cache_destroy(ClientCache* cache) {
    if (!cache) {
        return;
    }

    linked_list_clear(cache->entries, (void (*)(void*))free_cache_entry);
    linked_list_dispose(&cache->entries, NULL);
    free(cache);
}

int client_cache_set(ClientCache* cache, const char* key,
                     const char* json_data) {
    if (!cache || !key || !json_data) {
        return -1;
    }

    LinkedList_foreach(cache->entries, node) {
        CacheEntry* entry = (CacheEntry*)node->item;
        if (strcmp(entry->key, key) == 0) {
            linked_list_remove(cache->entries, node,
                               (void (*)(void*))free_cache_entry);
            break;
        }
    }

    if (cache->entries->size >= cache->max_entries) {
        if (cache->entries->head) {
            CacheEntry* oldest = (CacheEntry*)cache->entries->head->item;
            LinkedList_foreach(cache->entries, node) {
                CacheEntry* entry = (CacheEntry*)node->item;
                if (entry->created_at < oldest->created_at) {
                    oldest = entry;
                }
            }

            LinkedList_foreach(cache->entries, node) {
                if (node->item == oldest) {
                    delete_file(oldest->key);
                    linked_list_remove(cache->entries, node,
                                       (void (*)(void*))free_cache_entry);
                    break;
                }
            }
        }
    }

    CacheEntry* entry = malloc(sizeof(CacheEntry));
    if (!entry) {
        return -1;
    }

    entry->key       = strdup(key);
    entry->json_data = strdup(json_data);

    if (!entry->key || !entry->json_data) {
        free_cache_entry(entry);
        return -1;
    }

    entry->created_at = time(NULL);
    entry->ttl        = cache->default_ttl;

    if (linked_list_append(cache->entries, entry) != 0) {
        free_cache_entry(entry);
        return -1;
    }

    save_to_file(key, json_data);

    return 0;
}

char* client_cache_get(ClientCache* cache, const char* key) {
    if (!cache || !key) {
        return NULL;
    }

    LinkedList_foreach(cache->entries, node) {
        CacheEntry* entry = (CacheEntry*)node->item;
        if (strcmp(entry->key, key) == 0) {
            time_t now = time(NULL);
            double age = difftime(now, entry->created_at);

            if (age > (double)entry->ttl) {
                linked_list_remove(cache->entries, node,
                                   (void (*)(void*))free_cache_entry);
                delete_file(key);
                return NULL;
            }

            char* filepath = get_cache_filepath(key);
            if (filepath) {
                struct stat file_stat;
                if (stat(filepath, &file_stat) != 0) {
                    free(filepath);
                    linked_list_remove(cache->entries, node,
                                       (void (*)(void*))free_cache_entry);
                    return NULL;
                }
                free(filepath);
            }

            return strdup(entry->json_data);
        }
    }

    char* json_data = load_from_file(key, cache->default_ttl);
    if (json_data) {
        CacheEntry* entry = malloc(sizeof(CacheEntry));
        if (entry) {
            entry->key        = strdup(key);
            entry->json_data  = strdup(json_data);
            entry->created_at = time(NULL);
            entry->ttl        = cache->default_ttl;

            if (entry->key && entry->json_data) {
                linked_list_append(cache->entries, entry);
            } else {
                free_cache_entry(entry);
            }
        }
        return json_data;
    }

    return NULL;
}

void client_cache_clear(ClientCache* cache) {
    if (!cache) {
        return;
    }

    LinkedList_foreach(cache->entries, node) {
        CacheEntry* entry = (CacheEntry*)node->item;
        delete_file(entry->key);
    }

    linked_list_clear(cache->entries, (void (*)(void*))free_cache_entry);

    DIR* dir = opendir(CACHE_DIR);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            if (strcmp(entry->d_name, "README.md") == 0) {
                continue;
            }

            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", CACHE_DIR,
                     entry->d_name);

            struct stat file_stat;
            if (stat(filepath, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
                unlink(filepath);
            }
        }
        closedir(dir);
    }
}
