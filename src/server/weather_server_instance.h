#ifndef WEATHER_SERVER_INSTANCE_H
#define WEATHER_SERVER_INSTANCE_H

#include "http_server/http_server_connection.h"
#include <jansson.h>  // include Jansson

struct MemoryBlock {
    char *memory;
    size_t size;
};

typedef struct {
    double temperature;
    double windspeed;
    double winddirection;
    double humidity;
    int weathercode;
    char time[64];
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

// Updated function declaration for MemoryBlock
int get_weather_data(double latitude, double longitude, struct MemoryBlock* out);
int parse_weather_json(const char* json_text, Meteo* meteo);

#endif // WEATHER_SERVER_INSTANCE_H