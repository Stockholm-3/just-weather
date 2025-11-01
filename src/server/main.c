#include "smw.h"
#include "utils.h"
#include "weather_server.h"

int main() {
    smw_init();

    WeatherServer server;
    weather_server_initiate(&server);

    while (1) {
        smw_work(system_monotonic_ms());
    }

    weather_server_dispose(&server);

    smw_dispose();

    return 0;
}
