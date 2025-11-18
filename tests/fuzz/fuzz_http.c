#include "src/lib/http_server/http_server_connection.h"
#include "weather_server_instance.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestInput(const uint8_t* data, size_t size) {
    // Initialize a weather server instance
    WeatherServerInstance* instance = weather_server_instance_create();
    if (!instance) {
        return 0; // Failed to create instance
    }
    // Create a mock HTTP server connection using the fuzzed data
    HTTPServerConnection* connection =
        http_server_connection_create_from_data(data, size);
    if (!connection) {
        weather_server_instance_dispose(instance);
        return 0; // Failed to create connection
    }

    // Initiate the weather server instance with the connection
    if (weather_server_instance_initiate(instance, connection) != 0) {
        http_server_connection_dispose(connection);
        weather_server_instance_dispose(instance);
        return 0; // Failed to initiate instance
    }
    // Simulate part of the work with fuzzed data
    weather_server_instance_work(instance, (uint64_t)size);
    // Clean things up
    weather_server_instance_dispose(instance);
    http_server_connection_dispose(connection);
    return 0;
}
WeatherServerInstance* weather_server_instance_create() {
    WeatherServerInstance* instance =
        (WeatherServerInstance*)malloc(sizeof(WeatherServerInstance));
    if (instance) {
        instance->connection = NULL;
    }
    return instance;
}
HTTPServerConnection*
http_server_connection_create_from_data(const uint8_t* data, size_t size) {
    HTTPServerConnection* connection =
        (HTTPServerConnection*)malloc(sizeof(HTTPServerConnection));
    if (connection) {
        // Fill in some fields with fuzzed data
        connection->read_buffer = (uint8_t*)malloc(size);
        if (connection->read_buffer) {
            memcpy(connection->read_buffer, data, size);
            connection->read_buffer_size = size;
        } else {
            free(connection);
            return NULL;
        }
    }
    return connection;
}
