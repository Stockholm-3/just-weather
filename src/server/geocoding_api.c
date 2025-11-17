/**
 * geocoding_api.c - Geocoding API implementation
 *
 * Uses the Open-Meteo Geocoding API to search for city coordinates
 * API documentation: https://open-meteo.com/en/docs/geocoding-api
 */

#include "geocoding_api.h"

#include "hash_md5.h"

#include <curl/curl.h>
#include <errno.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/* ============= Configuration ============= */

#define GEOCODING_API_URL "https://geocoding-api.open-meteo.com/v1/search"
#define DEFAULT_CACHE_DIR "./cache/geo_cache"
#define DEFAULT_CACHE_TTL 604800 /* 7 days */
#define DEFAULT_MAX_RESULTS 10
#define DEFAULT_LANGUAGE "eng"

/* ============= Global State ============= */

static GeocodingConfig g_config = {.cache_dir   = DEFAULT_CACHE_DIR,
                                   .cache_ttl   = DEFAULT_CACHE_TTL,
                                   .use_cache   = true,
                                   .max_results = DEFAULT_MAX_RESULTS,
                                   .language    = DEFAULT_LANGUAGE};

/* ============= Internal Structures ============= */

typedef struct {
    char*  data;
    size_t size;
} MemoryChunk;

/* ============= Internal Functions ============= */

static size_t write_callback(void* contents, size_t size, size_t nmemb,
                             void* userp);
static char*  generate_cache_filepath(const char* search_key);
static int    is_cache_valid(const char* filepath, int ttl_seconds);
static int load_from_cache(const char* filepath, GeocodingResponse** response);
static int save_to_cache(const char* filepath, const char* json_str);
static int fetch_from_api(const char* city_name, const char* country,
                          GeocodingResponse** response);
static char* build_api_url(const char* city_name, const char* country,
                           int max_results, const char* language);
static int   parse_geocoding_json(const char*         json_str,
                                  GeocodingResponse** response);
static char* url_encode(const char* str);

/* ============= Public API Implementation ============= */
/**
 * Create directory and all parent directories (like mkdir -p)
 */
static int mkdir_recursive(const char* path, mode_t mode) {
    char   tmp[512];
    char*  p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);

    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;

            if (mkdir(tmp, mode) != 0) {
                if (errno != EEXIST) {
                    fprintf(stderr, "Failed to create dir: %s (errno: %d)", tmp,
                            errno);
                    return -1;
                }
            }

            *p = '/';
        }
    }

    if (mkdir(tmp, mode) != 0) {
        if (errno != EEXIST) {
            fprintf(stderr, "Failed to create dir: %s (errno: %d)", tmp, errno);
            return -1;
        }
    }

    return 0;
}

int geocoding_api_init(GeocodingConfig* config) {
    if (config) {
        /* Copy user-provided configuration */
        g_config = *config;
    }

    /* Create cache directory if it doesn't exist */
    if (mkdir_recursive(g_config.cache_dir, 0755) != 0) {
        fprintf(stderr,
                "[GEOCODING] Warning: Failed to create cache directory\n");
    }

    /* Initialize curl globally (if not already initialized) */
    curl_global_init(CURL_GLOBAL_DEFAULT);

    printf("[GEOCODING] API initialized\n");
    printf("[GEOCODING] Cache dir: %s\n", g_config.cache_dir);
    printf("[GEOCODING] Cache TTL: %d seconds (%d days)\n", g_config.cache_ttl,
           g_config.cache_ttl / 86400);
    printf("[GEOCODING] Cache enabled: %s\n",
           g_config.use_cache ? "yes" : "no");
    printf("[GEOCODING] Language: %s\n", g_config.language);

    return 0;
}

int geocoding_api_search(const char* city_name, const char* country,
                         GeocodingResponse** response) {
    if (!city_name || !response) {
        fprintf(stderr, "[GEOCODING] Invalid parameters\n");
        return -1;
    }

    /* Generate cache key */
    char search_key[512];
    if (country) {
        snprintf(search_key, sizeof(search_key), "%s_%s_%s", city_name, country,
                 g_config.language);
    } else {
        snprintf(search_key, sizeof(search_key), "%s_%s", city_name,
                 g_config.language);
    }

    /* Generate cache file path */
    char* cache_file = generate_cache_filepath(search_key);
    if (!cache_file) {
        fprintf(stderr, "[GEOCODING] Failed to generate cache filepath\n");
        return -2;
    }

    printf("[GEOCODING] Searching for: %s%s%s\n", city_name,
           country ? " in " : "", country ? country : "");
    printf("[GEOCODING] Cache file: %s\n", cache_file);

    /* Check cache */
    if (g_config.use_cache && is_cache_valid(cache_file, g_config.cache_ttl)) {
        printf("[GEOCODING] Cache HIT - loading from file\n");

        int result = load_from_cache(cache_file, response);
        free(cache_file);

        if (result == 0) {
            return 0; /* Successfully loaded from cache */
        }

        fprintf(stderr, "[GEOCODING] Cache load failed, fetching from API\n");
    } else {
        if (g_config.use_cache) {
            printf("[GEOCODING] Cache MISS - fetching from API\n");
        } else {
            printf("[GEOCODING] Cache disabled - fetching from API\n");
        }
    }

    /* Fetch from API */
    int result = fetch_from_api(city_name, country, response);

    if (result != 0) {
        fprintf(stderr, "[GEOCODING] API fetch failed\n");
        free(cache_file);
        return -3;
    }

    /* Save to cache */
    if (g_config.use_cache && *response) {
        /* Convert response back to JSON for saving */
        json_t* root          = json_object();
        json_t* results_array = json_array();

        for (int i = 0; i < (*response)->count; i++) {
            GeocodingResult* r    = &(*response)->results[i];
            json_t*          item = json_object();

            json_object_set_new(item, "id", json_integer(r->id));
            json_object_set_new(item, "name", json_string(r->name));
            json_object_set_new(item, "latitude", json_real(r->latitude));
            json_object_set_new(item, "longitude", json_real(r->longitude));
            json_object_set_new(item, "country", json_string(r->country));
            json_object_set_new(item, "country_code",
                                json_string(r->country_code));
            if (r->admin1[0])
                json_object_set_new(item, "admin1", json_string(r->admin1));
            if (r->admin2[0])
                json_object_set_new(item, "admin2", json_string(r->admin2));
            if (r->population > 0)
                json_object_set_new(item, "population",
                                    json_integer(r->population));
            if (r->timezone[0])
                json_object_set_new(item, "timezone", json_string(r->timezone));

            json_array_append_new(results_array, item);
        }

        json_object_set_new(root, "results", results_array);

        char* json_str = json_dumps(root, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
        if (json_str) {
            if (save_to_cache(cache_file, json_str) == 0) {
                printf("[GEOCODING] Saved to cache\n");
            } else {
                fprintf(stderr, "[GEOCODING] Failed to save cache\n");
            }
            free(json_str);
        }
        json_decref(root);
    }

    free(cache_file);
    return 0;
}

int geocoding_api_search_detailed(const char* city_name, const char* region,
                                  const char*         country,
                                  GeocodingResponse** response) {
    /* First, perform a normal search */
    int result = geocoding_api_search(city_name, country, response);

    if (result != 0 || !response || !*response) {
        return result;
    }

    /* If a region is specified, filter the results */
    if (region && region[0] != '\0') {
        GeocodingResponse* filtered = malloc(sizeof(GeocodingResponse));
        if (!filtered) {
            return -1;
        }

        filtered->results =
            malloc(sizeof(GeocodingResult) * (*response)->count);
        if (!filtered->results) {
            free(filtered);
            return -1;
        }

        filtered->count = 0;

        /* Filter by region */
        for (int i = 0; i < (*response)->count; i++) {
            if (strstr((*response)->results[i].admin1, region) != NULL ||
                strstr((*response)->results[i].admin2, region) != NULL) {
                filtered->results[filtered->count] = (*response)->results[i];
                filtered->count++;
            }
        }

        /* If filtered results are found, replace the original response */
        if (filtered->count > 0) {
            /* Reallocate memory to the exact count */
            GeocodingResult* temp = realloc(
                filtered->results, sizeof(GeocodingResult) * filtered->count);
            if (temp) {
                filtered->results = temp;
            }

            geocoding_api_free_response(*response);
            *response = filtered;
        } else {
            /* If nothing is found after filtering, keep the original results */
            free(filtered->results);
            free(filtered);
            printf("[GEOCODING] No results match region '%s', returning all "
                   "results\n",
                   region);
        }
    }

    return 0;
}

GeocodingResult* geocoding_api_get_best_result(GeocodingResponse* response) {
    if (!response || response->count == 0) {
        return NULL;
    }

    /* Return the first result (API sorts by relevance) */
    return &response->results[0];
}

void geocoding_api_free_response(GeocodingResponse* response) {
    if (response) {
        if (response->results) {
            free(response->results);
        }
        free(response);
    }
}

int geocoding_api_clear_cache(void) {
    /* Remove all files from the cache directory */
    char command[512];
    snprintf(command, sizeof(command), "rm -f %s/*.json", g_config.cache_dir);

    int result = system(command);

    if (result == 0) {
        printf("[GEOCODING] Cache cleared\n");
        return 0;
    } else {
        fprintf(stderr, "[GEOCODING] Failed to clear cache\n");
        return -1;
    }
}

void geocoding_api_cleanup(void) {
    /* curl_global_cleanup is called in open_meteo_api_cleanup */
    printf("[GEOCODING] API cleaned up\n");
}

int geocoding_api_format_result(GeocodingResult* result, char* buffer,
                                size_t buffer_size) {
    if (!result || !buffer || buffer_size == 0) {
        return -1;
    }

    /* Format: "Name, Region, Country (lat, lon)" */
    int written = 0;

    written +=
        snprintf(buffer + written, buffer_size - written, "%s", result->name);

    if (result->admin1[0]) {
        written += snprintf(buffer + written, buffer_size - written, ", %s",
                            result->admin1);
    }

    written += snprintf(buffer + written, buffer_size - written, ", %s",
                        result->country);

    written += snprintf(buffer + written, buffer_size - written,
                        " (%.4f, %.4f)", result->latitude, result->longitude);

    return 0;
}

/* ============= Internal Functions Implementation ============= */

/**
 * CURL write callback for receiving data
 */
static size_t write_callback(void* contents, size_t size, size_t nmemb,
                             void* userp) {
    size_t       realsize = size * nmemb;
    MemoryChunk* mem      = (MemoryChunk*)userp;

    char* ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "[GEOCODING] Out of memory\n");
        return 0;
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    return realsize;
}

/**
 * Generate cache file path using MD5 hash
 */
static char* generate_cache_filepath(const char* search_key) {
    if (!search_key) {
        return NULL;
    }

    /* Compute MD5 hash */
    char hash[HASH_MD5_STRING_LENGTH];
    if (hash_md5_string(search_key, strlen(search_key), hash, sizeof(hash)) !=
        0) {
        fprintf(stderr, "[GEOCODING] Failed to calculate MD5 hash\n");
        return NULL;
    }

    /* Build full path: cache_dir/hash.json */
    char* filepath = malloc(512);
    if (!filepath) {
        fprintf(stderr, "[GEOCODING] Failed to allocate memory for filepath\n");
        return NULL;
    }

    snprintf(filepath, 512, "%s/%s.json", g_config.cache_dir, hash);

    return filepath;
}

/**
 * Check whether cache file exists and is not expired
 */
static int is_cache_valid(const char* filepath, int ttl_seconds) {
    struct stat file_stat;

    /* Check if the file exists */
    if (stat(filepath, &file_stat) != 0) {
        return 0; /* File does not exist */
    }

    /* Check if the file is recent enough */
    time_t now = time(NULL);
    double age = difftime(now, file_stat.st_mtime);

    if (age > ttl_seconds) {
        return 0; /* Cache expired */
    }

    return 1; /* Cache valid */
}

/**
 * Load data from cache
 */
static int load_from_cache(const char* filepath, GeocodingResponse** response) {
    json_error_t error;
    json_t*      root = json_load_file(filepath, 0, &error);

    if (!root) {
        fprintf(stderr, "[GEOCODING] Failed to load cache: %s\n", error.text);
        return -1;
    }

    json_t* results_array = json_object_get(root, "results");
    if (!results_array || !json_is_array(results_array)) {
        fprintf(stderr, "[GEOCODING] Cache file has invalid format\n");
        json_decref(root);
        return -2;
    }

    /* Parse results */
    return parse_geocoding_json(json_dumps(root, 0), response);
}

/**
 * Save data to cache
 */
static int save_to_cache(const char* filepath, const char* json_str) {
    if (!filepath || !json_str) {
        return -1;
    }

    /* Parse JSON for validation */
    json_error_t error;
    json_t*      json = json_loadb(json_str, strlen(json_str), 0, &error);
    if (json == NULL) {
        fprintf(stderr, "[GEOCODING] Invalid JSON to cache: %s\n", error.text);
        return -2;
    }

    /* Save with formatting */
    if (json_dump_file(json, filepath, JSON_INDENT(2) | JSON_PRESERVE_ORDER) !=
        0) {
        fprintf(stderr, "[GEOCODING] Failed to save JSON to file: %s\n",
                filepath);
        json_decref(json);
        return -3;
    }

    json_decref(json);
    return 0;
}

/**
 * URL encoding for parameters
 */
static char* url_encode(const char* str) {
    if (!str) {
        return NULL;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return NULL;
    }

    char* encoded = curl_easy_escape(curl, str, strlen(str));
    if (!encoded) {
        curl_easy_cleanup(curl);
        return NULL;
    }

    /* Copy the result */
    char* result = strdup(encoded);
    curl_free(encoded);
    curl_easy_cleanup(curl);

    return result;
}

/**
 * Build URL for API request
 */
static char* build_api_url(const char* city_name, const char* country,
                           int max_results, const char* language) {
    char* url = malloc(2048);
    if (!url) {
        return NULL;
    }

    char* encoded_city = url_encode(city_name);
    if (!encoded_city) {
        free(url);
        return NULL;
    }

    int written =
        snprintf(url, 2048, "%s?name=%s&count=%d&language=%s&format=json",
                 GEOCODING_API_URL, encoded_city, max_results, language);

    free(encoded_city);

    if (country) {
        written +=
            snprintf(url + written, 2048 - written, "&country=%s", country);
    }

    return url;
}

/**
 * Parse JSON response from the API
 */
static int parse_geocoding_json(const char*         json_str,
                                GeocodingResponse** response) {
    json_error_t error;
    json_t*      root = json_loadb(json_str, strlen(json_str), 0, &error);

    if (!root) {
        fprintf(stderr, "[GEOCODING] JSON parse error: %s\n", error.text);
        return -1;
    }

    json_t* results_array = json_object_get(root, "results");
    if (!results_array) {
        /* No results */
        *response            = calloc(1, sizeof(GeocodingResponse));
        (*response)->count   = 0;
        (*response)->results = NULL;
        json_decref(root);
        return 0;
    }

    if (!json_is_array(results_array)) {
        fprintf(stderr, "[GEOCODING] Invalid results format\n");
        json_decref(root);
        return -2;
    }

    size_t count = json_array_size(results_array);
    if (count == 0) {
        *response            = calloc(1, sizeof(GeocodingResponse));
        (*response)->count   = 0;
        (*response)->results = NULL;
        json_decref(root);
        return 0;
    }

    /* Allocate memory */
    *response = calloc(1, sizeof(GeocodingResponse));
    if (!*response) {
        json_decref(root);
        return -3;
    }

    (*response)->results = calloc(count, sizeof(GeocodingResult));
    if (!(*response)->results) {
        free(*response);
        json_decref(root);
        return -4;
    }

    (*response)->count = count;

    /* Parse each result */
    for (size_t i = 0; i < count; i++) {
        json_t*          item   = json_array_get(results_array, i);
        GeocodingResult* result = &(*response)->results[i];

        /* Required fields */
        json_t* id           = json_object_get(item, "id");
        json_t* name         = json_object_get(item, "name");
        json_t* lat          = json_object_get(item, "latitude");
        json_t* lon          = json_object_get(item, "longitude");
        json_t* country_name = json_object_get(item, "country");
        json_t* country_code = json_object_get(item, "country_code");

        if (id)
            result->id = json_integer_value(id);
        if (name)
            strncpy(result->name, json_string_value(name),
                    sizeof(result->name) - 1);
        if (lat)
            result->latitude = json_real_value(lat);
        if (lon)
            result->longitude = json_real_value(lon);
        if (country_name)
            strncpy(result->country, json_string_value(country_name),
                    sizeof(result->country) - 1);
        if (country_code)
            strncpy(result->country_code, json_string_value(country_code),
                    sizeof(result->country_code) - 1);

        /* Optional fields */
        json_t* admin1     = json_object_get(item, "admin1");
        json_t* admin2     = json_object_get(item, "admin2");
        json_t* population = json_object_get(item, "population");
        json_t* timezone   = json_object_get(item, "timezone");

        if (admin1)
            strncpy(result->admin1, json_string_value(admin1),
                    sizeof(result->admin1) - 1);
        if (admin2)
            strncpy(result->admin2, json_string_value(admin2),
                    sizeof(result->admin2) - 1);
        if (population)
            result->population = json_integer_value(population);
        if (timezone)
            strncpy(result->timezone, json_string_value(timezone),
                    sizeof(result->timezone) - 1);
    }

    json_decref(root);
    return 0;
}

/**
 * Fetch data from API
 */
static int fetch_from_api(const char* city_name, const char* country,
                          GeocodingResponse** response) {
    CURL*       curl;
    CURLcode    res;
    MemoryChunk chunk = {0};

    /* Build URL */
    char* url = build_api_url(city_name, country, g_config.max_results,
                              g_config.language);
    if (!url) {
        return -1;
    }

    printf("[GEOCODING] Fetching: %s\n", url);

    /* Initialize curl */
    curl = curl_easy_init();
    if (!curl) {
        free(url);
        return -2;
    }

    /* Configure curl */
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "just-weather-geocoding/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    /* Perform request */
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "[GEOCODING] CURL error: %s\n",
                curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(url);
        if (chunk.data)
            free(chunk.data);
        return -3;
    }

    /* Check HTTP status */
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
        fprintf(stderr, "[GEOCODING] HTTP error: %ld\n", http_code);
        curl_easy_cleanup(curl);
        free(url);
        if (chunk.data)
            free(chunk.data);
        return -4;
    }

    curl_easy_cleanup(curl);
    free(url);

    /* Parse JSON */
    int result = parse_geocoding_json(chunk.data, response);

    if (chunk.data)
        free(chunk.data);

    if (result != 0) {
        return -5;
    }

    printf("[GEOCODING] Found %d result(s)\n", (*response)->count);
    return 0;
}