#ifndef WEATHER_SERVER_INSTANCE_H
#define WEATHER_SERVER_INSTANCE_H

#include "http_server/http_server_connection.h"
#include "smw.h"

typedef struct {
    HTTPServerConnection* connection;

} WeatherServerInstance;

int WeatherServerInstance_Initiate(WeatherServerInstance* _Instance,
                                   HTTPServerConnection*  _Connection);
int WeatherServerInstance_InitiatePtr(HTTPServerConnection*   _Connection,
                                      WeatherServerInstance** _InstancePtr);

void WeatherServerInstance_Work(WeatherServerInstance* _Instance,
                                uint64_t               _MonTime);

void WeatherServerInstance_Dispose(WeatherServerInstance* _Instance);
void WeatherServerInstance_DisposePtr(WeatherServerInstance** _InstancePtr);

#endif // WEATHER_SERVER_INSTANCE_H
