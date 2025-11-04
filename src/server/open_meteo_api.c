/* open_meteo_api.c - Open meteo API implementation for just-weather server */

#include "open_meteo_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <jansson.h>

/*============= Configuration =============*/

#define API_BASE_URL "https://api.open-meteo.com/v1/forecast"
#define DEFAULT_CACHE_DIR "./cache/"
#define DEFAULT_CACHE_TTL 900  /* 15 minutes */

static WeatherConfig g_config = {
    .cache_dir = DEFAULT_CACHE_DIR,
    .cache_ttl = DEFAULT_CACHE_TTL,
    .use_cache = true
};

/*============= Internal structures =============*/

typedef struct {
    char*  data;
    size_t size;
} HttpResponse;

typedef struct {
    int code;
    const char* description;
} WeatherCodeMap;

/*============= Weather code descriptions =============*/

static const WeatherCodeMap weather_codes[] = {
    {0, "Clear sky"},
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
    {80, "Slight rain showers"},
    {81, "Moderate rain showers"},
    {82, "Violent rain showers"},
    {85, "Slight snow showers"},
    {86, "Heavy snow showers"},
    {95, "Thunderstorm"},
    {96, "Thunderstorm with slight hail"},
    {99, "Thunderstorm with heavy hail"},
    {-1, "Unknown"}
};

/*============= Internal functions =============*/

static size_t http_write_callback(void* contents, size_t size, size_t nmemb, void* userp);
static int http_get(const char* url, HttpResponse* response);
static void http_free(HttpResponse* response);
static int create_directory(const char* path);
static bool file_exists(const char* path);
static void simple_hash(const char* input, char* output);
static int get_cache_path(char* buffer, size_t size, const char* url);
static bool is_cache_valid(const char* filepath, int ttl);
static time_t parse_iso_datetime(const char* time_str);
static int parse_current_weather(json_t* json, WeatherData** data);

/*============= Public API Implementation =============*/

int open_meteo_api_init(WeatherConfig* config) {
    if (config != NULL) {
        g_config = *config;
    }
    
    if (g_config.use_cache) {
        return create_directory(g_config.cache_dir);
    }
    
    return 0;
}

int open_meteo_api_get_current(Location* location, WeatherData** data) {
    if (location == NULL || data == NULL) {
        return -1;
    }
    
    /* Build URL */
    char url[512];
    snprintf(url, sizeof(url),
        "%s?latitude=%.4f&longitude=%.4f"
        "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
        "is_day,precipitation,rain,weather_code,pressure_msl,"
        "wind_speed_10m,wind_direction_10m"
        "&timezone=GMT",
        API_BASE_URL, location->latitude, location->longitude);
    
    /* Check cache */
    char cache_path[512];
    HttpResponse response = {0};
    json_error_t error;
    json_t* json = NULL;
    
    if (g_config.use_cache) {
        get_cache_path(cache_path, sizeof(cache_path), url);
        
        if (is_cache_valid(cache_path, g_config.cache_ttl)) {
            json = json_load_file(cache_path, 0, &error);
            if (json != NULL) {
                int result = parse_current_weather(json, data);
                json_decref(json);
                return result;
            }
        }
    }
    
    /* Fetch from API */
    if (http_get(url, &response) != 0) {
        return -2;
    }
    
    /* Parse JSON */
    json = json_loadb(response.data, response.size, 0, &error);
    http_free(&response);
    
    if (json == NULL) {
        fprintf(stderr, "JSON parse error: %s\n", error.text);
        return -3;
    }
    
    /* Save to cache */
    if (g_config.use_cache) {
        json_dump_file(json, cache_path, JSON_INDENT(2));
    }
    
    /* Parse weather data */
    int result = parse_current_weather(json, data);
    json_decref(json);
    
    if (result == 0 && *data) {
        /* Get city name from coordinates */
        open_meteo_api_get_city_name(location->latitude, location->longitude, 
                                     (*data)->city_name, sizeof((*data)->city_name));
        
        /* Store coordinates */
        (*data)->latitude = location->latitude;
        (*data)->longitude = location->longitude;
    }
    
    return result;
}

void open_meteo_api_free_current(WeatherData* data) {
    if (data != NULL) {
        free(data);
    }
}

void open_meteo_api_cleanup(void) {
    /* Cleanup if needed */
}

const char* open_meteo_api_get_description(int weather_code) {
    for (int i = 0; weather_codes[i].code != -1; i++) {
        if (weather_codes[i].code == weather_code) {
            return weather_codes[i].description;
        }
    }
    return "Unknown";
}

char* open_meteo_api_build_json_response(WeatherData* data, float lat, float lon) {
    if (!data) {
        return NULL;
    }
    
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", gmtime(&data->timestamp));
    
    /* Allocate buffer for JSON */
    char* json = malloc(2048);
    if (!json) {
        return NULL;
    }
    
    snprintf(json, 2048,
        "{\n"
        "  \"latitude\": %.4f,\n"
        "  \"longitude\": %.4f,\n"
        "  \"timezone\": \"GMT\",\n"
        "  \"current\": {\n"
        "    \"time\": \"%s\",\n"
        "    \"temperature_2m\": %.1f,\n"
        "    \"relative_humidity_2m\": %.0f,\n"
        "    \"wind_speed_10m\": %.1f,\n"
        "    \"wind_direction_10m\": %d,\n"
        "    \"weather_code\": %d,\n"
        "    \"weather_description\": \"%s\"\n"
        "    \"precipitation\": %.1f,\n"
        "    \"pressure_msl\": %.0f,\n"
        "    \"is_day\": %d\n"
        "  },\n"
        "  \"current_units\": {\n"
        "    \"time\": \"iso8601\",\n"
        "    \"temperature_2m\": \"%s\",\n"
        "    \"relative_humidity_2m\": \"%%\",\n"
        "    \"wind_speed_10m\": \"%s\",\n"
        "    \"wind_direction_10m\": \"%s\",\n"
        "    \"weather_code\": \"wmo code\",\n"
        "    \"precipitation\": \"%s\",\n"
        "    \"pressure_msl\": \"hPa\",\n"
        "    \"is_day\": \"\"\n"
        "  },\n"
        "}",
        lat, lon,
        time_str,
        data->temperature,
        data->humidity,
        data->windspeed,
        data->winddirection,
        data->weather_code,
        open_meteo_api_get_description(data->weather_code),
        data->precipitation,
        data->pressure,
        data->is_day,
        data->temperature_unit[0] != '\0' ? data->temperature_unit : "°C",
        data->windspeed_unit[0] != '\0' ? data->windspeed_unit : "km/h",
        data->winddirection_unit[0] != '\0' ? data->winddirection_unit : "°",
        data->precipitation_unit[0] != '\0' ? data->precipitation_unit : "mm"
        
    );
    
    return json;
}

int open_meteo_api_parse_query(const char* query, float* lat, float* lon) {
    if (!query || !lat || !lon) {
        return -1;
    }
    
    *lat = 0.0f;
    *lon = 0.0f;
    
    /* Parse format: "lat=37.7749&long=-122.4194" or "lat=37.7749&lon=-122.4194" */
    char* query_copy = strdup(query);
    if (!query_copy) {
        return -1;
    }
    
    char* token = strtok(query_copy, "&");
    int lat_found = 0, lon_found = 0;
    
    while (token != NULL) {
        if (strncmp(token, "lat=", 4) == 0) {
            *lat = atof(token + 4);
            lat_found = 1;
        } else if (strncmp(token, "long=", 5) == 0 || strncmp(token, "lon=", 4) == 0) {
            *lon = atof(token + (strncmp(token, "long=", 5) == 0 ? 5 : 4));
            lon_found = 1;
        }
        token = strtok(NULL, "&");
    }
    
    free(query_copy);
    
    if (!lat_found || !lon_found) {
        return -1;
    }
    
    /* Validate ranges */
    if (*lat < -90.0f || *lat > 90.0f || *lon < -180.0f || *lon > 180.0f) {
        return -1;
    }
    
    return 0;
}

/*============= Reverse Geocoding =============*/

int open_meteo_api_get_city_name(float lat, float lon, char* city_name, size_t size) {
    if (!city_name || size == 0) {
        return -1;
    }
    
    /* Build geocoding URL */
    char url[512];
    snprintf(url, sizeof(url),
        "https://geocoding-api.open-meteo.com/v1/reverse?latitude=%.4f&longitude=%.4f&count=1",
        lat, lon);
    
    /* Fetch from geocoding API */
    HttpResponse response = {0};
    if (http_get(url, &response) != 0) {
        /* Fallback to coordinates */
        snprintf(city_name, size, "%.4f, %.4f", lat, lon);
        return 1;
    }
    
    /* Parse JSON */
    json_error_t error;
    json_t* json = json_loadb(response.data, response.size, 0, &error);
    http_free(&response);
    
    if (json == NULL) {
        snprintf(city_name, size, "%.4f, %.4f", lat, lon);
        return 1;
    }
    
    /* Extract city name */
    json_t* results = json_object_get(json, "results");
    if (json_is_array(results) && json_array_size(results) > 0) {
        json_t* first_result = json_array_get(results, 0);
        
        /* Try to get name */
        json_t* name = json_object_get(first_result, "name");
        if (json_is_string(name)) {
            strncpy(city_name, json_string_value(name), size - 1);
            city_name[size - 1] = '\0';
            json_decref(json);
            return 0;
        }
        
        /* Try admin1 (region) */
        json_t* admin1 = json_object_get(first_result, "admin1");
        if (json_is_string(admin1)) {
            strncpy(city_name, json_string_value(admin1), size - 1);
            city_name[size - 1] = '\0';
            json_decref(json);
            return 0;
        }
    }
    
    json_decref(json);
    
    /* Fallback */
    snprintf(city_name, size, "%.4f, %.4f", lat, lon);
    return 1;
}

/*============= HTTP functions =============*/

/* ВИПРАВЛЕНО: змінено тип userp з HttpResponse* на void* */
static size_t http_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    HttpResponse* response = (HttpResponse*)userp;  /* Cast до правильного типу */
    
    char* ptr = realloc(response->data, response->size + realsize + 1);
    
    if (ptr == NULL) {
        return 0;
    }
    
    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0;
    
    return realsize;
}

static int http_get(const char* url, HttpResponse* response) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return -1;
    }
    
    response->data = NULL;
    response->size = 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "just-weather/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        http_free(response);
        return -1;
    }
    
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    
    if (http_code != 200) {
        http_free(response);
        return -1;
    }
    
    return 0;
}

static void http_free(HttpResponse* response) {
    if (response && response->data) {
        free(response->data);
        response->data = NULL;
        response->size = 0;
    }
}

/*============= File system functions =============*/

static int create_directory(const char* path) {
#ifdef _WIN32
    return _mkdir(path);
#else
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        return mkdir(path, 0755);
    }
    return 0;
#endif
}

static bool file_exists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

static void simple_hash(const char* input, char* output) {
    unsigned long hash = 5381;
    int c;
    const char* str = input;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    sprintf(output, "%016lx", hash);
}

static int get_cache_path(char* buffer, size_t size, const char* url) {
    char hash[33];
    simple_hash(url, hash);
    snprintf(buffer, size, "%s%s.json", g_config.cache_dir, hash);
    return 0;
}

static bool is_cache_valid(const char* filepath, int ttl) {
    if (!file_exists(filepath)) {
        return false;
    }
    
    struct stat file_stat;
    if (stat(filepath, &file_stat) != 0) {
        return false;
    }
    
    time_t now = time(NULL);
    double age = difftime(now, file_stat.st_mtime);
    
    return age < ttl;
}

/*============= JSON Parsing =============*/

static time_t parse_iso_datetime(const char* time_str) {
    struct tm tm = {0};
    int year, month, day, hour, min;
    
    if (sscanf(time_str, "%4d-%2d-%2dT%2d:%2d", &year, &month, &day, &hour, &min) != 5) {
        return (time_t)-1;
    }
    
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_isdst = -1;
    
#ifdef _WIN32
    return _mkgmtime(&tm);
#else
    return timegm(&tm);
#endif
}

static int parse_current_weather(json_t* json, WeatherData** data) {
    json_t* current = json_object_get(json, "current");
    json_t* units = json_object_get(json, "current_units");
    
    if (!current || !units) {
        return -1;
    }
    
    *data = (WeatherData*)calloc(1, sizeof(WeatherData));
    if (*data == NULL) {
        return -2;
    }
    
    /* Parse timestamp */
    json_t* time_json = json_object_get(current, "time");
    if (json_is_string(time_json)) {
        (*data)->timestamp = parse_iso_datetime(json_string_value(time_json));
    }
    
    /* Parse weather code */
    json_t* code = json_object_get(current, "weather_code");
    if (json_is_integer(code)) {
        (*data)->weather_code = json_integer_value(code);
    }
    
    /* Parse temperature */
    json_t* temp = json_object_get(current, "temperature_2m");
    json_t* temp_unit = json_object_get(units, "temperature_2m");
    if (json_is_number(temp)) {
        (*data)->temperature = json_number_value(temp);
    }
    if (json_is_string(temp_unit)) {
        strncpy((*data)->temperature_unit, json_string_value(temp_unit), sizeof((*data)->temperature_unit) - 1);
        (*data)->temperature_unit[sizeof((*data)->temperature_unit) - 1] = '\0';
    }
    
    /* Parse wind speed */
    json_t* wind = json_object_get(current, "wind_speed_10m");
    json_t* wind_unit = json_object_get(units, "wind_speed_10m");
    if (json_is_number(wind)) {
        (*data)->windspeed = json_number_value(wind);
    }
    if (json_is_string(wind_unit)) {
        strncpy((*data)->windspeed_unit, json_string_value(wind_unit), sizeof((*data)->windspeed_unit) - 1);
        (*data)->windspeed_unit[sizeof((*data)->windspeed_unit) - 1] = '\0';
    }
    
    /* Parse wind direction */
    json_t* wind_dir = json_object_get(current, "wind_direction_10m");
    json_t* wind_dir_unit = json_object_get(units, "wind_direction_10m");
    if (json_is_number(wind_dir)) {
        (*data)->winddirection = json_number_value(wind_dir);
    }
    if (json_is_string(wind_dir_unit)) {
        strncpy((*data)->winddirection_unit, json_string_value(wind_dir_unit), sizeof((*data)->winddirection_unit) - 1);
        (*data)->winddirection_unit[sizeof((*data)->winddirection_unit) - 1] = '\0';
    }
    
    /* Parse precipitation */
    json_t* precip = json_object_get(current, "precipitation");
    json_t* precip_unit = json_object_get(units, "precipitation");
    if (json_is_number(precip)) {
        (*data)->precipitation = json_number_value(precip);
    }
    if (json_is_string(precip_unit)) {
        strncpy((*data)->precipitation_unit, json_string_value(precip_unit), sizeof((*data)->precipitation_unit) - 1);
        (*data)->precipitation_unit[sizeof((*data)->precipitation_unit) - 1] = '\0';
    }
    
    /* Parse humidity */
    json_t* humidity = json_object_get(current, "relative_humidity_2m");
    if (json_is_number(humidity)) {
        (*data)->humidity = json_number_value(humidity);
    }
    
    /* Parse pressure */
    json_t* pressure = json_object_get(current, "pressure_msl");
    if (json_is_number(pressure)) {
        (*data)->pressure = json_number_value(pressure);
    }
    
    /* Parse is_day */
    json_t* is_day = json_object_get(current, "is_day");
    if (json_is_integer(is_day)) {
        (*data)->is_day = json_integer_value(is_day);
    }
    
    return 0;
}