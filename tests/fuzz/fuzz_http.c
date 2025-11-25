#include "http_server/http_server_connection.h"
#include "weather_server_instance.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // 1. Initialize HTTP connection (fd = -1 means dummy socket)
    HTTPServerConnection* conn = NULL;
    if (http_server_connection_initiate_ptr(-1, &conn) != 0 || conn == NULL)
        return 0;

    // Provide stable dummy values so the server code doesn't crash
    conn->method       = strdup("GET");
    conn->request_path = strdup("/");
    conn->host         = strdup("localhost");
    conn->content_len  = size;

    // Feed fuzz data into the read buffer (make an owned copy so dispose() can free it)
    if (size > 0) {
        conn->read_buffer = (uint8_t*)malloc(size);
        if (!conn->read_buffer) {
            http_server_connection_dispose_ptr(&conn);
            return 0;
        }
        memcpy(conn->read_buffer, data, size);
    } else {
        conn->read_buffer = NULL;
    }
    conn->read_buffer_size = size;

    // 2. Initialize WeatherServerInstance using official API
    WeatherServerInstance* instance = NULL;
    if (weather_server_instance_initiate_ptr(conn, &instance) != 0 ||
        instance == NULL) {

        http_server_connection_dispose_ptr(&conn);
        return 0;
    }

    // 3. Run main logic
    weather_server_instance_work(instance, 0);

    // 4. Clean up
    weather_server_instance_dispose_ptr(&instance);
    http_server_connection_dispose_ptr(&conn);

    return 0;
}