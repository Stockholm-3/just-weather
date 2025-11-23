#include "http_client.h"
#include "smw.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

// Manual implementation of system_monotonic_ms
uint64_t system_monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

void response_callback(const char* event, const char* response) {
    printf("\n\r------------ HTTP CLIENT CALLBACK ------------\n\r");
    printf("Event: %s\n\r", event);
    if (response) {
        printf("Response: %s\n\r", response);
    }
    printf("------------ END OF CALLBACK ------------\n\r");
}

int main() {
    smw_init();

    // Use http_client_get with port parameter
    if (http_client_get("http://localhost:8080/", 10000, response_callback,
                        "8080") != 0) {
        perror("Failed to create HTTP client");
        return -1;
    }

    printf("HTTP client started, making request to localhost:8080...\n");

    // Main loop with manual timeout handling in the state machine
    while (1) {
        smw_work(system_monotonic_ms());
    }

    smw_dispose();
    return 0;
}
