#ifndef CLI_H
#define CLI_H

#include "api/weather_client.h"

void cli_print_usage(const char* prog_name);
void cli_interactive_mode(WeatherClient* client);
int  cli_execute_command(WeatherClient* client, int argc, char* argv[]);

#endif
