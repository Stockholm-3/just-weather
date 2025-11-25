/* open_meteo_stub.c - lightweight stub for Open-Meteo API used by fuzzing */

#include "../../src/server/open_meteo_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int open_meteo_api_init(WeatherConfig* config) {
    (void)config;
    return 0;
}

int open_meteo_api_get_current(Location* location, WeatherData** data) {
    if (!location || !data)
        return -1;

    WeatherData* wd = (WeatherData*)malloc(sizeof(WeatherData));
    if (!wd)
        return -1;

    wd->timestamp = 0;
    wd->weather_code = 0;
    wd->temperature = 20.0;
    strncpy(wd->temperature_unit, "C", sizeof(wd->temperature_unit));
    wd->windspeed = 1.0;
    strncpy(wd->windspeed_unit, "m/s", sizeof(wd->windspeed_unit));
    wd->winddirection = 0;
    wd->precipitation = 0.0;
    wd->humidity = 50.0;
    wd->pressure = 1013.25;
    wd->is_day = 1;
    wd->latitude = location->latitude;
    wd->longitude = location->longitude;
    strncpy(wd->city_name, location->name ? location->name : "unknown", sizeof(wd->city_name)-1);
    wd->city_name[sizeof(wd->city_name)-1] = '\0';
    wd->_raw_json_cache = NULL;

    *data = wd;
    return 0;
}

void open_meteo_api_free_current(WeatherData* data) {
    if (data) free(data);
}

void open_meteo_api_cleanup(void) {
}

const char* open_meteo_api_get_description(int weather_code) {
    (void)weather_code;
    return "Clear sky (stub)";
}

char* open_meteo_api_build_json_response(WeatherData* data, float lat, float lon) {
    if (!data)
        return NULL;

    char* out = (char*)malloc(512);
    if (!out)
        return NULL;

    snprintf(out, 512,
             "{\n  \"location\": {\n    \"latitude\": %.4f,\n    \"longitude\": %.4f\n  },\n  \"temperature_c\": %.2f,\n  \"humidity_percent\": %.0f\n}",
             lat, lon, data->temperature, data->humidity);

    return out;
}

int open_meteo_api_parse_query(const char* query, float* lat, float* lon) {
    if (!query || !lat || !lon)
        return -1;

    /* Very permissive parser to make fuzzing easier */
    int matched = 0;
    if (sscanf(query, "lat=%f&lon=%f", lat, lon) == 2)
        matched = 1;
    else if (sscanf(query, "lat=%f&long=%f", lat, lon) == 2)
        matched = 1;
    else {
        /* fallback: try to extract two floats anywhere */
        float a = 0, b = 0;
        if (sscanf(query, "%f%*[^0-9.-]%f", &a, &b) == 2) {
            *lat = a; *lon = b; matched = 1;
        }
    }

    return matched ? 0 : -1;
}

int open_meteo_api_get_city_name(float lat, float lon, char* city_name, size_t size) {
    if (!city_name || size == 0) return -1;
    snprintf(city_name, size, "%.4f,%.4f", lat, lon);
    return 0;
}
