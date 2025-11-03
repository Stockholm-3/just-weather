#ifndef WEATHER_SERVER_INSTANCE_H
#define WEATHER_SERVER_INSTANCE_H

#include "http_server/http_server_connection.h"
#include <jansson.h>  // include Jansson

typedef struct {
    double temperature;
    double windspeed;
    double winddirection;
    int weathercode;
    char time[32];
    char* data;
} Meteo;

typedef struct {
    double latitude;
    double longitude;
    HTTPServerConnection* connection;
} WeatherServerInstance;

int weather_server_instance_initiate(WeatherServerInstance* instance,
                                     HTTPServerConnection*  connection);
int weather_server_instance_initiate_ptr(HTTPServerConnection*   connection,
                                         WeatherServerInstance** instance_ptr);

void weather_server_instance_work(WeatherServerInstance* instance,
                                  uint64_t               mon_time);
void weather_server_instance_dispose(WeatherServerInstance* instance);
void weather_server_instance_dispose_ptr(WeatherServerInstance** instance_ptr);

// New functions
int get_weather_data(double latitude, double longitude, char** weather_json);
int parse_weather_json(const char* json_text, Meteo* meteo);

#endif // WEATHER_SERVER_INSTANCE_H