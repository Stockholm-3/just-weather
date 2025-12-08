#include "api/weather_client.h"
#include "cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXIT_INVALID_ARGS 1
#define EXIT_NETWORK_ERROR 2

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cli_print_usage(argv[0]);
        return EXIT_INVALID_ARGS;
    }

    WeatherClient* client = weather_client_create("localhost", 10680);
    if (!client) {
        fprintf(stderr, "Failed to create weather client\n");
        return EXIT_NETWORK_ERROR;
    }

    const char* command   = argv[1];
    int         exit_code = 0;

    if (strcmp(command, "interactive") == 0 || strcmp(command, "-i") == 0) {
        cli_interactive_mode(client);
    } else {
        exit_code = cli_execute_command(client, argc, argv);
        if (exit_code == EXIT_INVALID_ARGS) {
            cli_print_usage(argv[0]);
        }
    }

    weather_client_destroy(client);
    return exit_code;
}
