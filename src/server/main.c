#include "open_meteo_api.h"
#include "open_meteo_handler.h"
#include "smw.h"
#include "utils.h"
#include "weather_server.h"

#include <stdio.h>

int main() {
    smw_init();

    if (open_meteo_handler_init() != 0) {
        printf("Failed to initialize Open-Meteo handler\n");
        return 1;
    }
    printf("Open-Meteo API initialized\n");
    printf("Open-meteo handler ready: GET /v1/current?lat=X&long=Y\n\n");

    WeatherServer server;
    weather_server_initiate(&server);

    while (1) {
        smw_work(system_monotonic_ms());
    }

    open_meteo_handler_cleanup();
    weather_server_dispose(&server);
    smw_dispose();

    return 0;
}
