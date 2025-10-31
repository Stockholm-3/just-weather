
#ifndef WEATHER_SERVER_H
#define WEATHER_SERVER_H

#include "http_server/http_server.h"
#include "linked_list.h"
#include "smw.h"
#include "weather_server_instance.h"

typedef struct {
    HTTPServer httpServer;

    LinkedList* instances;

    smw_task* task;

} WeatherServer;

int WeatherServer_Initiate(WeatherServer* _Server);
int WeatherServer_InitiatePtr(WeatherServer** _ServerPtr);

void WeatherServer_Dispose(WeatherServer* _Server);
void WeatherServer_DisposePtr(WeatherServer** _ServerPtr);

#endif // WEATHER_SERVER_H
