#include "smw.h"
#include "utils.h"
#include "weather_server.h"
#include "weather_server_instance.h"

void test_fetch_and_parse_url(void);

int main(void) {
    test_fetch_and_parse_url();

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
