/* open_meteo_api.c - Open-Meteo API integration with MD5 caching */

#include "open_meteo_api.h"

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

#define API_BASE_URL "https://api.open-meteo.com/v1/forecast"
#define DEFAULT_CACHE_DIR "./cache/weather_cache"
#define DEFAULT_CACHE_TTL 900 /* 15 minutes */

/* ============= Global State ============= */

static WeatherConfig g_config = {.cache_dir = DEFAULT_CACHE_DIR,
                                 .cache_ttl = DEFAULT_CACHE_TTL,
                                 .use_cache = true};

/* ============= Internal Structures ============= */

typedef struct {
    char*  data;
    size_t size;
} MemoryChunk;

/* ============= Internal Functions ============= */

static size_t write_callback(void* contents, size_t size, size_t nmemb,
                             void* userp);
static char*  generate_cache_filepath(float lat, float lon);
static int    is_cache_valid(const char* filepath, int ttl_seconds);
static int    load_weather_from_cache(const char* filepath, WeatherData** data);
static int   save_raw_json_to_cache(const char* filepath, const char* json_str);
static int   fetch_weather_from_api(Location* location, WeatherData** data);
static char* build_api_url(float lat, float lon);
static int   parse_weather_json(const char* json_str, WeatherData* data,
                                float lat, float lon);
static const char* get_wind_direction_name(int degrees);

/* ============= Weather Code Descriptions ============= */

static const struct {
    int         code;
    const char* description;
} weather_descriptions[] = {{0, "Clear sky"},
                            {1, "Mainly clear"},
                            {2, "Partly cloudy"},
                            {3, "Overcast"},
                            {45, "Fog"},
                            {48, "Depositing rime fog"},
                            {51, "Light drizzle"},
                            {53, "Moderate drizzle"},
                            {55, "Dense drizzle"},
                            {61, "Slight rain"},
                            {63, "Moderate rain"},
                            {65, "Heavy rain"},
                            {71, "Slight snow"},
                            {73, "Moderate snow"},
                            {75, "Heavy snow"},
                            {77, "Snow grains"},
                            {80, "Slight rain showers"},
                            {81, "Moderate rain showers"},
                            {82, "Violent rain showers"},
                            {85, "Slight snow showers"},
                            {86, "Heavy snow showers"},
                            {95, "Thunderstorm"},
                            {96, "Thunderstorm with slight hail"},
                            {99, "Thunderstorm with heavy hail"},
                            {-1, "Unknown"}};

/* ============= Wind Direction Cardinal ============= */

static const char* get_wind_direction_name(int degrees) {
    /* Normalize to 0-360 range */
    degrees = degrees % 360;
    if (degrees < 0)
        degrees += 360;

    /* 16-point compass rose with 22.5° per direction */
    if (degrees >= 348.75 || degrees < 11.25)
        return "North";
    else if (degrees < 33.75)
        return "North-Northeast";
    else if (degrees < 56.25)
        return "Northeast";
    else if (degrees < 78.75)
        return "East-Northeast";
    else if (degrees < 101.25)
        return "East";
    else if (degrees < 123.75)
        return "East-Southeast";
    else if (degrees < 146.25)
        return "Southeast";
    else if (degrees < 168.75)
        return " South-Southeast";
    else if (degrees < 191.25)
        return "South";
    else if (degrees < 213.75)
        return "South-Southwest";
    else if (degrees < 236.25)
        return "Southwest";
    else if (degrees < 258.75)
        return "West-Southwest";
    else if (degrees < 281.25)
        return "West";
    else if (degrees < 303.75)
        return "North-Northwest";
    else if (degrees < 326.25)
        return "Northwest";
    else
        return "North-Northwest";
}

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
                    fprintf(stderr, "Failed to create dir: %s (errno: %d)\n",
                            tmp, errno);
                    return -1;
                }
            }

            *p = '/';
        }
    }

    if (mkdir(tmp, mode) != 0) {
        if (errno != EEXIST) {
            fprintf(stderr, "Failed to create dir: %s (errno: %d)\n", tmp,
                    errno);
            return -1;
        }
    }

    return 0;
}

int open_meteo_api_init(WeatherConfig* config) {
    if (!config) {
        return -1;
    }

    /* Copy configuration */
    g_config = *config;

    /* Create cache directory if it doesn't exist */
    if (mkdir_recursive(g_config.cache_dir, 0755) != 0) {
        fprintf(stderr, "[METEO] Warning: Failed to create cache directory\n");
    }

    /* Initialize curl globally */
    curl_global_init(CURL_GLOBAL_DEFAULT);

    printf("[METEO] API initialized\n");
    printf("[METEO] Cache dir: %s\n", g_config.cache_dir);
    printf("[METEO] Cache TTL: %d seconds\n", g_config.cache_ttl);
    printf("[METEO] Cache enabled: %s\n", g_config.use_cache ? "yes" : "no");

    return 0;
}

int open_meteo_api_get_current(Location* location, WeatherData** data) {
    if (!location || !data) {
        fprintf(stderr, "[METEO] Invalid parameters\n");
        return -1;
    }

    /* Generate cache filepath using MD5 */
    char* cache_file =
        generate_cache_filepath(location->latitude, location->longitude);
    if (!cache_file) {
        fprintf(stderr, "[METEO] Failed to generate cache filepath\n");
        return -2;
    }

    printf("[METEO] Cache file: %s\n", cache_file);

    /* Check cache validity */
    if (g_config.use_cache && is_cache_valid(cache_file, g_config.cache_ttl)) {
        printf("[METEO] Cache HIT - loading from file\n");

        int result = load_weather_from_cache(cache_file, data);
        free(cache_file);

        if (result == 0) {
            return 0; /* Success - loaded from cache */
        }

        fprintf(stderr, "[METEO] Cache load failed, fetching from API\n");
    } else {
        if (g_config.use_cache) {
            printf("[METEO] Cache MISS - fetching from API\n");
        } else {
            printf("[METEO] Cache disabled - fetching from API\n");
        }
    }

    /* Fetch from API */
    int result = fetch_weather_from_api(location, data);

    if (result != 0) {
        fprintf(stderr, "[METEO] API fetch failed\n");
        free(cache_file);
        return -3;
    }

    /* Save RAW JSON to cache (preserves original API structure) */
    if (g_config.use_cache && (*data)->_raw_json_cache) {
        if (save_raw_json_to_cache(cache_file, (*data)->_raw_json_cache) == 0) {
            printf("[METEO] Saved to cache\n");
        } else {
            fprintf(stderr, "[METEO] Failed to save cache\n");
        }

        /* Free the raw JSON after saving */
        free((*data)->_raw_json_cache);
        (*data)->_raw_json_cache = NULL;
    }

    free(cache_file);
    return 0;
}

void open_meteo_api_free_current(WeatherData* data) {
    if (data) {
        /* Free raw JSON cache if it exists */
        if (data->_raw_json_cache) {
            free(data->_raw_json_cache);
            data->_raw_json_cache = NULL;
        }
        free(data);
    }
}

void open_meteo_api_cleanup(void) {
    curl_global_cleanup();
    printf("[METEO] API cleaned up\n");
}

const char* open_meteo_api_get_description(int weather_code) {
    for (size_t i = 0;
         i < sizeof(weather_descriptions) / sizeof(weather_descriptions[0]) - 1;
         i++) {
        if (weather_descriptions[i].code == weather_code) {
            return weather_descriptions[i].description;
        }
    }
    return weather_descriptions[sizeof(weather_descriptions) /
                                    sizeof(weather_descriptions[0]) -
                                1]
        .description;
}

char* open_meteo_api_build_json_response(WeatherData* data, float lat,
                                         float lon) {
    if (!data) {
        return NULL;
    }

    /* Generate cache filepath to load raw JSON */
    char* cache_file = generate_cache_filepath(lat, lon);
    if (!cache_file) {
        fprintf(stderr, "[METEO] Failed to generate cache filepath\n");
        return NULL;
    }

    /* Load raw JSON from cache file */
    json_error_t error;
    json_t*      root = json_load_file(cache_file, 0, &error);
    free(cache_file);

    if (!root) {
        fprintf(stderr, "[METEO] Failed to load raw JSON from cache: %s\n",
                error.text);
        return NULL;
    }

    /* Add helpful descriptions to the JSON */
    json_t* current = json_object_get(root, "current");
    if (current) {
        /* Add weather code description */
        json_t* weather_code = json_object_get(current, "weather_code");
        if (weather_code && json_is_integer(weather_code)) {
            int         code        = json_integer_value(weather_code);
            const char* description = open_meteo_api_get_description(code);
            json_object_set_new(current, "weather_description",
                                json_string(description));
        }

        /* Add wind direction name */
        json_t* wind_direction = json_object_get(current, "wind_direction_10m");
        if (wind_direction && json_is_integer(wind_direction)) {
            int         degrees        = json_integer_value(wind_direction);
            const char* direction_name = get_wind_direction_name(degrees);
            json_object_set_new(current, "wind_direction_name",
                                json_string(direction_name));
        }
    }

    /* Convert JSON object to formatted string */
    char* json_str = json_dumps(root, JSON_INDENT(2) | JSON_PRESERVE_ORDER);

    /* Cleanup */
    json_decref(root);

    return json_str;
}

int open_meteo_api_parse_query(const char* query, float* lat, float* lon) {
    if (!query || !lat || !lon) {
        return -1;
    }

    /* Parse query string: lat=X&lon=Y or lat=X&long=Y */
    char query_copy[512];
    strncpy(query_copy, query, sizeof(query_copy) - 1);
    query_copy[sizeof(query_copy) - 1] = '\0';

    char* token     = strtok(query_copy, "&");
    int   found_lat = 0, found_lon = 0;

    while (token != NULL) {
        if (strncmp(token, "lat=", 4) == 0) {
            *lat      = atof(token + 4);
            found_lat = 1;
        } else if (strncmp(token, "lon=", 4) == 0 ||
                   strncmp(token, "long=", 5) == 0) {
            char* value = strchr(token, '=');
            if (value) {
                *lon      = atof(value + 1);
                found_lon = 1;
            }
        }
        token = strtok(NULL, "&");
    }

    if (found_lat && found_lon) {
        return 0;
    }

    return -1;
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
        fprintf(stderr, "[METEO] Out of memory\n");
        return 0;
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    return realsize;
}

/**
 * Generate cache filepath using MD5 hash of coordinates
 * Caller must free the returned string
 */
static char* generate_cache_filepath(float lat, float lon) {
    /* Create unique key from coordinates */
    char cache_key[256];
    int  key_len =
        snprintf(cache_key, sizeof(cache_key), "weather_%.6f_%.6f", lat, lon);

    if (key_len < 0 || key_len >= (int)sizeof(cache_key)) {
        fprintf(stderr, "[METEO] Failed to create cache key\n");
        return NULL;
    }

    /* Calculate MD5 hash */
    char hash[HASH_MD5_STRING_LENGTH];
    if (hash_md5_string(cache_key, strlen(cache_key), hash, sizeof(hash)) !=
        0) {
        fprintf(stderr, "[METEO] Failed to calculate MD5 hash\n");
        return NULL;
    }

    /* Build full filepath: cache_dir/hash.json */
    char* filepath = malloc(512);
    if (!filepath) {
        fprintf(stderr, "[METEO] Failed to allocate memory for filepath\n");
        return NULL;
    }

    snprintf(filepath, 512, "%s/%s.json", g_config.cache_dir, hash);

    return filepath;
}

/**
 * Check if cache file exists and is not expired
 */
static int is_cache_valid(const char* filepath, int ttl_seconds) {
    struct stat file_stat;

    /* Check if file exists */
    if (stat(filepath, &file_stat) != 0) {
        return 0; /* File doesn't exist */
    }

    /* Check if file is recent enough */
    time_t now = time(NULL);
    double age = difftime(now, file_stat.st_mtime);

    if (age > ttl_seconds) {
        return 0; /* Cache expired */
    }

    return 1; /* Cache is valid */
}

/**
 * Load weather data from cache file
 */
static int load_weather_from_cache(const char* filepath, WeatherData** data) {
    json_error_t error;
    json_t*      root = json_load_file(filepath, 0, &error);

    if (!root) {
        fprintf(stderr, "[METEO] Failed to load cache: %s\n", error.text);
        return -1;
    }

    /* Allocate weather data */
    *data = (WeatherData*)calloc(1, sizeof(WeatherData));
    if (!*data) {
        json_decref(root);
        return -2;
    }

    /* Get current weather and units from Open-Meteo API format */
    json_t* current       = json_object_get(root, "current");
    json_t* current_units = json_object_get(root, "current_units");

    if (!current || !current_units) {
        fprintf(stderr,
                "[METEO] Cache file missing 'current' or 'current_units'\n");
        json_decref(root);
        free(*data);
        return -3;
    }

    /* Extract values from API format */
    json_t* temp           = json_object_get(current, "temperature_2m");
    json_t* temp_unit      = json_object_get(current_units, "temperature_2m");
    json_t* windspeed      = json_object_get(current, "wind_speed_10m");
    json_t* windspeed_unit = json_object_get(current_units, "wind_speed_10m");
    json_t* winddirection  = json_object_get(current, "wind_direction_10m");
    json_t* winddirection_unit =
        json_object_get(current_units, "wind_direction_10m");
    json_t* precipitation = json_object_get(current, "precipitation");
    json_t* precipitation_unit =
        json_object_get(current_units, "precipitation");
    json_t* humidity     = json_object_get(current, "relative_humidity_2m");
    json_t* pressure     = json_object_get(current, "surface_pressure");
    json_t* weather_code = json_object_get(current, "weather_code");
    json_t* is_day       = json_object_get(current, "is_day");
    json_t* time_str     = json_object_get(current, "time");

    /* Copy values */
    if (temp)
        (*data)->temperature = json_real_value(temp);
    if (temp_unit)
        strncpy((*data)->temperature_unit, json_string_value(temp_unit),
                sizeof((*data)->temperature_unit) - 1);
    if (windspeed)
        (*data)->windspeed = json_real_value(windspeed);
    if (windspeed_unit)
        strncpy((*data)->windspeed_unit, json_string_value(windspeed_unit),
                sizeof((*data)->windspeed_unit) - 1);
    if (winddirection)
        (*data)->winddirection = json_integer_value(winddirection);
    if (winddirection_unit)
        strncpy((*data)->winddirection_unit,
                json_string_value(winddirection_unit),
                sizeof((*data)->winddirection_unit) - 1);
    if (precipitation)
        (*data)->precipitation = json_real_value(precipitation);
    if (precipitation_unit)
        strncpy((*data)->precipitation_unit,
                json_string_value(precipitation_unit),
                sizeof((*data)->precipitation_unit) - 1);
    if (humidity)
        (*data)->humidity = json_real_value(humidity);
    if (pressure)
        (*data)->pressure = json_real_value(pressure);
    if (weather_code)
        (*data)->weather_code = json_integer_value(weather_code);
    if (is_day)
        (*data)->is_day = json_integer_value(is_day);

    /* Parse timestamp */
    if (time_str) {
        /* For now, use current time - could parse ISO string if needed */
        (*data)->timestamp = time(NULL);
    }

    /* Get location from API response */
    json_t* latitude  = json_object_get(root, "latitude");
    json_t* longitude = json_object_get(root, "longitude");

    if (latitude)
        (*data)->latitude = json_real_value(latitude);
    if (longitude)
        (*data)->longitude = json_real_value(longitude);

    json_decref(root);
    return 0;
}

/**
 * Save raw JSON response from API to cache file
 * This preserves the original API structure (current/current_units or
 * hourly/hourly_units)
 */
static int save_raw_json_to_cache(const char* filepath, const char* json_str) {
    if (!filepath || !json_str) {
        return -1;
    }

    /* Parse JSON to validate and format it */
    json_error_t error;
    json_t*      json = json_loadb(json_str, strlen(json_str), 0, &error);
    if (json == NULL) {
        fprintf(stderr, "[METEO] Invalid JSON to cache: %s\n", error.text);
        return -2;
    }

    /* Save with proper formatting (2-space indent, preserve order) */
    if (json_dump_file(json, filepath, JSON_INDENT(2) | JSON_PRESERVE_ORDER) !=
        0) {
        fprintf(stderr, "[METEO] Failed to save JSON to file: %s\n", filepath);
        json_decref(json);
        return -3;
    }

    json_decref(json);
    return 0;
}

/**
 * Build API URL with parameters
 */
static char* build_api_url(float lat, float lon) {
    char* url = malloc(1024);
    if (!url) {
        return NULL;
    }

    snprintf(url, 1024,
             "%s?latitude=%.6f&longitude=%.6f"
             "&current=temperature_2m,relative_humidity_2m,"
             "apparent_temperature,is_day,precipitation,weather_code,"
             "surface_pressure,wind_speed_10m,wind_direction_10m"
             "&timezone=GMT",
             API_BASE_URL, lat, lon);

    return url;
}

/**
 * Parse weather JSON from API response
 */
static int parse_weather_json(const char* json_str, WeatherData* data,
                              float lat, float lon) {
    json_error_t error;
    json_t*      root = json_loadb(json_str, strlen(json_str), 0, &error);

    if (!root) {
        fprintf(stderr, "[METEO] JSON parse error: %s\n", error.text);
        return -1;
    }

    /* Get current weather */
    json_t* current       = json_object_get(root, "current");
    json_t* current_units = json_object_get(root, "current_units");

    if (!current || !current_units) {
        json_decref(root);
        return -2;
    }

    /* Extract values */
    json_t* temp           = json_object_get(current, "temperature_2m");
    json_t* temp_unit      = json_object_get(current_units, "temperature_2m");
    json_t* windspeed      = json_object_get(current, "wind_speed_10m");
    json_t* windspeed_unit = json_object_get(current_units, "wind_speed_10m");
    json_t* winddirection  = json_object_get(current, "wind_direction_10m");
    json_t* winddirection_unit =
        json_object_get(current_units, "wind_direction_10m");
    json_t* precipitation = json_object_get(current, "precipitation");
    json_t* precipitation_unit =
        json_object_get(current_units, "precipitation");
    json_t* humidity     = json_object_get(current, "relative_humidity_2m");
    json_t* pressure     = json_object_get(current, "surface_pressure");
    json_t* weather_code = json_object_get(current, "weather_code");
    json_t* is_day       = json_object_get(current, "is_day");
    json_t* time_str     = json_object_get(current, "time");

    /* Copy values */
    if (temp)
        data->temperature = json_real_value(temp);
    if (temp_unit)
        strncpy(data->temperature_unit, json_string_value(temp_unit),
                sizeof(data->temperature_unit) - 1);
    if (windspeed)
        data->windspeed = json_real_value(windspeed);
    if (windspeed_unit)
        strncpy(data->windspeed_unit, json_string_value(windspeed_unit),
                sizeof(data->windspeed_unit) - 1);
    if (winddirection)
        data->winddirection = json_integer_value(winddirection);
    if (winddirection_unit)
        strncpy(data->winddirection_unit, json_string_value(winddirection_unit),
                sizeof(data->winddirection_unit) - 1);
    if (precipitation)
        data->precipitation = json_real_value(precipitation);
    if (precipitation_unit)
        strncpy(data->precipitation_unit, json_string_value(precipitation_unit),
                sizeof(data->precipitation_unit) - 1);
    if (humidity)
        data->humidity = json_real_value(humidity);
    if (pressure)
        data->pressure = json_real_value(pressure);
    if (weather_code)
        data->weather_code = json_integer_value(weather_code);
    if (is_day)
        data->is_day = json_integer_value(is_day);

    /* Parse timestamp (convert ISO string to Unix timestamp) */
    if (time_str) {
        /* For simplicity, use current time */
        data->timestamp = time(NULL);
    }

    /* Set location */
    data->latitude  = lat;
    data->longitude = lon;

    json_decref(root);
    return 0;
}

/**
 * Fetch weather data from Open-Meteo API
 */
static int fetch_weather_from_api(Location* location, WeatherData** data) {
    CURL*       curl;
    CURLcode    res;
    MemoryChunk chunk = {0};

    /* Build API URL */
    char* url = build_api_url(location->latitude, location->longitude);
    if (!url) {
        return -1;
    }

    printf("[METEO] Fetching: %s\n", url);

    /* Initialize curl */
    curl = curl_easy_init();
    if (!curl) {
        free(url);
        return -2;
    }

    /* Set curl options */
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "just-weather/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    /* Perform request */
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "[METEO] CURL error: %s\n", curl_easy_strerror(res));
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
        fprintf(stderr, "[METEO] HTTP error: %ld\n", http_code);
        curl_easy_cleanup(curl);
        free(url);
        if (chunk.data)
            free(chunk.data);
        return -4;
    }

    curl_easy_cleanup(curl);
    free(url);

    /* Allocate weather data */
    *data = (WeatherData*)calloc(1, sizeof(WeatherData));
    if (!*data) {
        if (chunk.data)
            free(chunk.data);
        return -5;
    }

    /* Parse JSON */
    int result = parse_weather_json(chunk.data, *data, location->latitude,
                                    location->longitude);

    if (result != 0) {
        if (chunk.data)
            free(chunk.data);
        free(*data);
        *data = NULL;
        return -6;
    }

    /* Store raw JSON for caching (save original API response) */
    /* This is temporarily stored in data structure for saving to cache */
    /* Will be freed after cache save */
    (*data)->_raw_json_cache = chunk.data; /* Transfer ownership */

    printf("[METEO] Successfully fetched weather data\n");
    return 0;
}