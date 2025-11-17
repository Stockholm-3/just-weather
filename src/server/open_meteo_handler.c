/*  open_meteo_handler.c -  open_meteo_handler - endpoint handler for HTTP
 * server */

#include "open_meteo_handler.h"

#include "open_meteo_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* HTTP status codes */
#define HTTP_OK 200
#define HTTP_BAD_REQUEST 400
#define HTTP_INTERNAL_ERROR 500

/* Build error JSON response */
static char* build_error_response(const char* error_msg, int code) {
    char* json = malloc(512);
    if (!json) {
        return NULL;
    }

    snprintf(json, 512,
             "{\n"
             "  \"error\": true,\n"
             "  \"code\": %d,\n"
             "  \"message\": \"%s\"\n"
             "}",
             code, error_msg ? error_msg : "Unknown error");

    return json;
}

/* Initialize weather server module */
int open_meteo_handler_init(void) {
    WeatherConfig config = {.cache_dir = "./cache/weather_cache",
                            .cache_ttl = 900, /* 15 minutes */
                            .use_cache = true};

    return open_meteo_api_init(&config);
}

/* Handle GET /v1/current endpoint */
int open_meteo_handler_current(const char* query_string, char** response_json,
                               int* status_code) {
    if (!response_json || !status_code) {
        return -1;
    }

    *response_json = NULL;
    *status_code   = HTTP_INTERNAL_ERROR;

    /* Parse query parameters */
    float lat, lon;
    if (open_meteo_api_parse_query(query_string, &lat, &lon) != 0) {
        *response_json =
            build_error_response("Invalid query parameters. Expected format: "
                                 "lat=XX.XXXX&long=YY.YYYY",
                                 HTTP_BAD_REQUEST);
        *status_code = HTTP_BAD_REQUEST;
        return -1;
    }

    /* Create location */
    Location location = {
        .latitude = lat, .longitude = lon, .name = "Query Location"};

    /* Get current weather */
    WeatherData* weather_data = NULL;
    int          result = open_meteo_api_get_current(&location, &weather_data);

    if (result != 0 || !weather_data) {
        *response_json = build_error_response(
            "Failed to fetch weather data from Open-Meteo API",
            HTTP_INTERNAL_ERROR);
        *status_code = HTTP_INTERNAL_ERROR;
        return -1;
    }

    /* Build JSON response */
    *response_json = open_meteo_api_build_json_response(weather_data, lat, lon);

    /* Cleanup */
    open_meteo_api_free_current(weather_data);

    if (!*response_json) {
        *status_code = HTTP_INTERNAL_ERROR;
        return -1;
    }

    *status_code = HTTP_OK;
    return 0;
}

/* Cleanup weather server module */
void open_meteo_handler_cleanup(void) { open_meteo_api_cleanup(); }