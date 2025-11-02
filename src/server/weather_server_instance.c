#include "weather_server_instance.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//-----------------Internal Functions-----------------

int weather_server_instance_on_request(void* context);

//----------------------------------------------------

int weather_server_instance_initiate(WeatherServerInstance* instance,
                                     HTTPServerConnection*  connection) {
    instance->connection = connection;

    http_server_connection_set_callback(instance->connection, instance,
                                        weather_server_instance_on_request);

    return 0;
}

int weather_server_instance_initiate_ptr(HTTPServerConnection*   connection,
                                         WeatherServerInstance** instance_ptr) {
    if (instance_ptr == NULL) {
        return -1;
    }

    WeatherServerInstance* instance =
        (WeatherServerInstance*)malloc(sizeof(WeatherServerInstance));
    if (instance == NULL) {
        return -2;
    }

    int result = weather_server_instance_initiate(instance, connection);
    if (result != 0) {
        free(instance);
        return result;
    }

    *(instance_ptr) = instance;

    return 0;
}

int weather_server_instance_on_request(void* context) {
    WeatherServerInstance* inst = (WeatherServerInstance*)context;
    HTTPServerConnection*  conn = inst->connection;

    printf("method: %s\n", conn->method);
    printf("url: %s\n", conn->request_path);
    printf("content size:\n%zu\n", conn->content_len);
    printf("body:\n%s\n", conn->body);

    const char* body_to_send =

        //        "<html>"
        //                               "<head><title>Weather
        //                               Server</title></head>"
        //                               "<body>"
        //                               "<h1>Welcome to the Weather
        //                               Server</h1>"
        //                               "<p>This is a simple HTML page served
        //                               by your " "non-blocking HTTP
        //                               server.</p>"
        //                               "</body>"
        //                               "</html>";

        "{\n"
        "  \"location\": {\n"
        "    \"latitude\": 51.5074,\n"
        "    \"longitude\": -0.1278\n"
        "  },\n"
        "  \"temperature_c\": 21.3,\n"
        "  \"humidity_percent\": 62,\n"
        "  \"windspeed_mps\": 5.4\n"
        "}";

    // Construct HTTP response header
    char header[256];
    int  header_len = snprintf(header, sizeof(header),
                               "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/json\r\n"
                                "Content-Length: %zu\r\n"
                                "\r\n",
                               strlen(body_to_send));

    size_t   total_len = header_len + strlen(body_to_send);
    uint8_t* response  = malloc(total_len + 1);
    if (!response) {
        perror("Out of mem");
        return -1;
    }
    memcpy(response, header, header_len);
    strcpy((char*)response + header_len, body_to_send);

    conn->write_buffer = response;
    conn->write_size   = total_len;
    return 0;
}

void weather_server_instance_work(WeatherServerInstance* instance,
                                  uint64_t               mon_time) {}

void weather_server_instance_dispose(WeatherServerInstance* instance) {}

void weather_server_instance_dispose_ptr(WeatherServerInstance** instance_ptr) {
    if (instance_ptr == NULL || *(instance_ptr) == NULL) {
        return;
    }

    weather_server_instance_dispose(*(instance_ptr));
    free(*(instance_ptr));
    *(instance_ptr) = NULL;
}
