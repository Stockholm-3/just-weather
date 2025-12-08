#ifndef WEATHER_CLIENT_H
#define WEATHER_CLIENT_H

#include <jansson.h>
#include <stddef.h>
#include <time.h>

typedef struct WeatherClient WeatherClient;

WeatherClient* weather_client_create(const char* host, int port);
void           weather_client_destroy(WeatherClient* client);

json_t* weather_client_get_current(WeatherClient* client, double lat,
                                   double lon, char** error);

json_t* weather_client_get_weather_by_city(WeatherClient* client,
                                           const char*    city,
                                           const char*    country,
                                           const char* region, char** error);

json_t* weather_client_search_cities(WeatherClient* client, const char* query,
                                     char** error);

json_t* weather_client_get_homepage(WeatherClient* client, char** error);

json_t* weather_client_echo(WeatherClient* client, char** error);

void weather_client_clear_cache(WeatherClient* client);
void weather_client_set_timeout(WeatherClient* client, int timeout_ms);

#endif
