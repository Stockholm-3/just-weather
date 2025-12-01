/**
 * weather_location_handler.c - Implementation of the combined handler
 */

#include "weather_location_handler.h"

#include "geocoding_api.h"
#include "open_meteo_api.h"
#include "open_meteo_handler.h"
#include "response_builder.h"

#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global state for lazy initialization */
static bool g_initialized = false;

/* Internal functions */
static int parse_city_query(const char* query, char* city, size_t city_size,
                            char* country, size_t country_size, char* region,
                            size_t region_size);
static int ensure_initialized(void);

/* ============= Lazy Initialization ============= */

static int ensure_initialized(void) {
    if (g_initialized) {
        return 0; /* Already initialized */
    }

    printf("[WEATHER_LOCATION] Initializing modules...\n");

    /* FIX: Initialize Weather API FIRST */
    /* This will create ./cache/ and set up caching */
    if (open_meteo_handler_init() != 0) {
        fprintf(stderr, "[WEATHER_LOCATION] Failed to init weather API\n");
        return -1;
    }

    /* Initialize geocoding API */
    GeocodingConfig geo_config = {.cache_dir   = "./cache/geo_cache",
                                  .cache_ttl   = 604800, /* 7 days */
                                  .use_cache   = true,
                                  .max_results = 10,
                                  .language    = "eng"};

    if (geocoding_api_init(&geo_config) != 0) {
        fprintf(stderr, "[WEATHER_LOCATION] Failed to init geocoding API\n");
        return -1;
    }

    g_initialized = true;
    printf("[WEATHER_LOCATION] All modules initialized successfully\n");
    return 0;
}

/* ============= Public API ============= */

int weather_location_handler_init(void) {
    /* Explicit initialization (optional) */
    return ensure_initialized();
}

int weather_location_handler_by_city(const char* query_string,
                                     char** response_json, int* status_code) {
    if (!response_json || !status_code) {
        return -1;
    }

    /* Automatic initialization on first call */
    if (ensure_initialized() != 0) {
        *response_json = response_builder_error(
            HTTP_INTERNAL_ERROR,
            response_builder_get_error_type(HTTP_INTERNAL_ERROR),
            "Failed to initialize geocoding module");
        *status_code = HTTP_INTERNAL_ERROR;
        return -1;
    }

    *response_json = NULL;
    *status_code   = HTTP_INTERNAL_ERROR;

    /* Parse query parameters */
    char city[128]  = {0};
    char country[8] = {0};
    char region[64] = {0};

    if (parse_city_query(query_string, city, sizeof(city), country,
                         sizeof(country), region, sizeof(region)) != 0) {
        *response_json = response_builder_error(
            HTTP_BAD_REQUEST, response_builder_get_error_type(HTTP_BAD_REQUEST),
            "Invalid query parameters. Expected: city=<name>&country=<code>");
        *status_code = HTTP_BAD_REQUEST;
        return -1;
    }

    if (city[0] == '\0') {
        *response_json = response_builder_error(
            HTTP_BAD_REQUEST, response_builder_get_error_type(HTTP_BAD_REQUEST),
            "Missing required parameter: city");
        *status_code = HTTP_BAD_REQUEST;
        return -1;
    }

    printf("[WEATHER_LOCATION] Request for city: %s%s%s%s%s\n", city,
           region[0] ? ", " : "", region, country[0] ? " (" : "",
           country[0] ? country : "");

    /* 1. Find city coordinates via geocoding */
    GeocodingResponse* geo_response = NULL;
    int                result;

    if (region[0] != '\0') {
        result = geocoding_api_search_detailed(
            city, region, country[0] ? country : NULL, &geo_response);
    } else {
        result = geocoding_api_search(city, country[0] ? country : NULL,
                                      &geo_response);
    }

    if (result != 0 || !geo_response || geo_response->count == 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "City not found: %s", city);
        *response_json = response_builder_error(
            HTTP_NOT_FOUND, response_builder_get_error_type(HTTP_NOT_FOUND),
            error_msg);
        *status_code = HTTP_NOT_FOUND;

        if (geo_response) {
            geocoding_api_free_response(geo_response);
        }
        return -1;
    }

    /* Take the best result */
    GeocodingResult* best_location = geocoding_api_get_best_result(
        geo_response, country[0] ? country : NULL);
    if (!best_location) {
        *response_json = response_builder_error(
            HTTP_INTERNAL_ERROR,
            response_builder_get_error_type(HTTP_INTERNAL_ERROR),
            "Failed to determine best location");
        *status_code = HTTP_INTERNAL_ERROR;
        geocoding_api_free_response(geo_response);
        return -1;
    }

    printf("[WEATHER_LOCATION] Found: %s, %s (%.4f, %.4f)\n",
           best_location->name, best_location->country, best_location->latitude,
           best_location->longitude);

    /* 2. Fetch weather for the found coordinates */
    Location location = {.latitude  = best_location->latitude,
                         .longitude = best_location->longitude,
                         .name      = best_location->name};

    WeatherData* weather_data = NULL;
    result = open_meteo_api_get_current(&location, &weather_data);

    if (result != 0 || !weather_data) {
        *response_json = response_builder_error(
            HTTP_INTERNAL_ERROR,
            response_builder_get_error_type(HTTP_INTERNAL_ERROR),
            "Failed to fetch weather data");
        *status_code = HTTP_INTERNAL_ERROR;
        geocoding_api_free_response(geo_response);
        return -1;
    }

    /* 3. Build JSON response with city and weather information */
    json_t* data = json_object();

    /* Add location information */
    json_t* location_obj = json_object();
    json_object_set_new(location_obj, "name", json_string(best_location->name));
    json_object_set_new(location_obj, "country",
                        json_string(best_location->country));
    json_object_set_new(location_obj, "country_code",
                        json_string(best_location->country_code));

    if (best_location->admin1[0]) {
        json_object_set_new(location_obj, "region",
                            json_string(best_location->admin1));
    }

    json_object_set_new(location_obj, "latitude",
                        json_real(best_location->latitude));
    json_object_set_new(location_obj, "longitude",
                        json_real(best_location->longitude));

    if (best_location->population > 0) {
        json_object_set_new(location_obj, "population",
                            json_integer(best_location->population));
    }

    if (best_location->timezone[0]) {
        json_object_set_new(location_obj, "timezone",
                            json_string(best_location->timezone));
    }

    json_object_set_new(data, "location", location_obj);

    /* Add weather data */
    json_t* weather_obj = json_object();
    json_object_set_new(weather_obj, "temperature",
                        json_real(weather_data->temperature));
    json_object_set_new(weather_obj, "temperature_unit",
                        json_string(weather_data->temperature_unit));
    json_object_set_new(weather_obj, "weather_code",
                        json_integer(weather_data->weather_code));
    json_object_set_new(weather_obj, "weather_description",
                        json_string(open_meteo_api_get_description(
                            weather_data->weather_code)));
    json_object_set_new(weather_obj, "windspeed",
                        json_real(weather_data->windspeed));
    json_object_set_new(weather_obj, "windspeed_unit",
                        json_string(weather_data->windspeed_unit));
    json_object_set_new(weather_obj, "winddirection",
                        json_integer(weather_data->winddirection));
    json_object_set_new(weather_obj, "winddirection_name",
                        json_string(open_meteo_api_get_wind_direction(
                            weather_data->winddirection)));
    json_object_set_new(weather_obj, "humidity",
                        json_real(weather_data->humidity));
    json_object_set_new(weather_obj, "pressure",
                        json_real(weather_data->pressure));
    json_object_set_new(weather_obj, "precipitation",
                        json_real(weather_data->precipitation));
    json_object_set_new(weather_obj, "is_day",
                        json_boolean(weather_data->is_day));

    json_object_set_new(data, "current_weather", weather_obj);

    /* Cleanup weather data */
    open_meteo_api_free_current(weather_data);
    geocoding_api_free_response(geo_response);

    /* Build standardized response */
    *response_json = response_builder_success(data);

    if (!*response_json) {
        json_decref(data);
        *status_code = HTTP_INTERNAL_ERROR;
        return -1;
    }

    *status_code = HTTP_OK;
    printf("[WEATHER_LOCATION] Response generated successfully\n");
    return 0;
}

int weather_location_handler_search_cities(const char* query_string,
                                           char**      response_json,
                                           int*        status_code) {
    if (!response_json || !status_code) {
        return -1;
    }

    /* Automatic initialization on first call */
    if (ensure_initialized() != 0) {
        *response_json = response_builder_error(
            HTTP_INTERNAL_ERROR,
            response_builder_get_error_type(HTTP_INTERNAL_ERROR),
            "Failed to initialize geocoding module");
        *status_code = HTTP_INTERNAL_ERROR;
        return -1;
    }

    *response_json = NULL;
    *status_code   = HTTP_INTERNAL_ERROR;

    /* Parse query parameter */
    char query[256] = {0};
    if (sscanf(query_string, "query=%255[^&]", query) != 1 ||
        query[0] == '\0') {
        *response_json = response_builder_error(
            HTTP_BAD_REQUEST, response_builder_get_error_type(HTTP_BAD_REQUEST),
            "Missing required parameter: query");
        *status_code = HTTP_BAD_REQUEST;
        return -1;
    }

    /* Search for cities: check cache first; if missing, fetch and save.
     * This ensures an appropriate cache file is created when needed while
     * still using the cache when available.
     */
    GeocodingResponse* response = NULL;
    int                result   = geocoding_api_search(query, NULL, &response);

    if (result != 0 || !response) {
        *response_json = response_builder_error(
            HTTP_INTERNAL_ERROR,
            response_builder_get_error_type(HTTP_INTERNAL_ERROR),
            "Failed to search cities");
        *status_code = HTTP_INTERNAL_ERROR;
        return -1;
    }

    /* Build JSON response */
    json_t* data = json_object();
    json_object_set_new(data, "query", json_string(query));
    json_object_set_new(data, "count", json_integer(response->count));

    json_t* cities_array = json_array();
    for (int i = 0; i < response->count; i++) {
        GeocodingResult* city     = &response->results[i];
        json_t*          city_obj = json_object();

        json_object_set_new(city_obj, "name", json_string(city->name));
        json_object_set_new(city_obj, "country", json_string(city->country));
        json_object_set_new(city_obj, "country_code",
                            json_string(city->country_code));

        if (city->admin1[0]) {
            json_object_set_new(city_obj, "region", json_string(city->admin1));
        }

        json_object_set_new(city_obj, "latitude", json_real(city->latitude));
        json_object_set_new(city_obj, "longitude", json_real(city->longitude));

        if (city->population > 0) {
            json_object_set_new(city_obj, "population",
                                json_integer(city->population));
        }

        json_array_append_new(cities_array, city_obj);
    }

    json_object_set_new(data, "cities", cities_array);

    geocoding_api_free_response(response);

    /* Build standardized response */
    *response_json = response_builder_success(data);

    if (!*response_json) {
        json_decref(data);
        *status_code = HTTP_INTERNAL_ERROR;
        return -1;
    }

    *status_code = HTTP_OK;
    return 0;
}

void weather_location_handler_cleanup(void) {
    if (!g_initialized) {
        return;
    }

    geocoding_api_cleanup();
    open_meteo_handler_cleanup();
    g_initialized = false;
    printf("[WEATHER_LOCATION] Handler cleaned up\n");
}

/* ============= Internal Functions ============= */

static int parse_city_query(const char* query, char* city, size_t city_size,
                            char* country, size_t country_size, char* region,
                            size_t region_size) {
    if (!query || !city || !country || !region) {
        return -1;
    }

    /* Copy query for safe parsing */
    char query_copy[1024];
    strncpy(query_copy, query, sizeof(query_copy) - 1);
    query_copy[sizeof(query_copy) - 1] = '\0';

    /* Parse parameters: city=X&country=Y&region=Z */
    char* token      = strtok(query_copy, "&");
    int   found_city = 0;

    while (token != NULL) {
        if (strncmp(token, "city=", 5) == 0) {
            strncpy(city, token + 5, city_size - 1);
            city[city_size - 1] = '\0';
            found_city          = 1;
        } else if (strncmp(token, "country=", 8) == 0) {
            strncpy(country, token + 8, country_size - 1);
            country[country_size - 1] = '\0';
        } else if (strncmp(token, "region=", 7) == 0) {
            strncpy(region, token + 7, region_size - 1);
            region[region_size - 1] = '\0';
        }
        token = strtok(NULL, "&");
    }

    /* Normalize region: replace underscores and '+' with spaces (common URL
     * forms) */
    for (size_t i = 0; i < region_size && region[i]; ++i) {
        if (region[i] == '_' || region[i] == '+')
            region[i] = ' ';
    }

    return found_city ? 0 : -1;
}