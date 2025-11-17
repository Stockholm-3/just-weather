#include "weather_server_instance.h"

#include "open_meteo_handler.h"
#include "weather_location_handler.h"

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

    // Parse URL to get path and query
    char path[256]  = {0};
    char query[512] = {0};

    // Split request_path into path and query
    char* question_mark = strchr(conn->request_path, '?');
    if (question_mark) {
        size_t path_len = question_mark - conn->request_path;
        strncpy(path, conn->request_path, path_len);
        path[path_len] = '\0';
        strcpy(query, question_mark + 1);
    } else {
        strcpy(path, conn->request_path);
    }

    // ==================================================================
    // ENDPOINT: /v1/weather?city=<name>&country=<code>
    // Weather by city name (uses geocoding + weather API)
    // ==================================================================
    if (strcmp(conn->method, "GET") == 0 && strcmp(path, "/v1/weather") == 0) {
        printf("Routing to Weather Location Handler\n");

        char* json_response = NULL;
        int   status_code   = 0;

        // Call the city-based weather handler
        weather_location_handler_by_city(query, &json_response, &status_code);

        if (json_response) {
            char header[256];
            int  header_len =
                snprintf(header, sizeof(header),
                         "HTTP/1.1 %d %s\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: %zu\r\n"
                         "\r\n",
                         status_code, status_code == 200 ? "OK" : "Error",
                         strlen(json_response));

            size_t   total_len = header_len + strlen(json_response);
            uint8_t* response  = malloc(total_len + 1);
            if (response) {
                memcpy(response, header, header_len);
                strcpy((char*)response + header_len, json_response);

                conn->write_buffer = response;
                conn->write_size   = total_len;
            }

            free(json_response);
            return 0;
        }
    }

    // ==================================================================
    // ENDPOINT: /v1/cities?query=<search>
    // City search (autocomplete)
    // ==================================================================
    if (strcmp(conn->method, "GET") == 0 && strcmp(path, "/v1/cities") == 0) {
        printf("Routing to City Search Handler\n");

        char* json_response = NULL;
        int   status_code   = 0;

        weather_location_handler_search_cities(query, &json_response,
                                               &status_code);

        if (json_response) {
            char header[256];
            int  header_len =
                snprintf(header, sizeof(header),
                         "HTTP/1.1 %d %s\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: %zu\r\n"
                         "\r\n",
                         status_code, status_code == 200 ? "OK" : "Error",
                         strlen(json_response));

            size_t   total_len = header_len + strlen(json_response);
            uint8_t* response  = malloc(total_len + 1);
            if (response) {
                memcpy(response, header, header_len);
                strcpy((char*)response + header_len, json_response);

                conn->write_buffer = response;
                conn->write_size   = total_len;
            }

            free(json_response);
            return 0;
        }
    }

    // ==================================================================
    // ENDPOINT: /v1/current?lat=<lat>&lon=<lon>
    // Weather by coordinates
    // ==================================================================
    if (strcmp(conn->method, "GET") == 0 && strcmp(path, "/v1/current") == 0) {
        printf("Routing to Open-Meteo API\n");

        char* json_response = NULL;
        int   status_code   = 0;

        // Call weather handler
        open_meteo_handler_current(query, &json_response, &status_code);

        if (json_response) {
            // Construct HTTP response header
            char header[256];
            int  header_len =
                snprintf(header, sizeof(header),
                         "HTTP/1.1 %d %s\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: %zu\r\n"
                         "\r\n",
                         status_code, status_code == 200 ? "OK" : "Error",
                         strlen(json_response));

            size_t   total_len = header_len + strlen(json_response);
            uint8_t* response  = malloc(total_len + 1);
            if (response) {
                memcpy(response, header, header_len);
                strcpy((char*)response + header_len, json_response);

                conn->write_buffer = response;
                conn->write_size   = total_len;
            }

            free(json_response);
            return 0;
        }
    }

    // ==================================================================
    // DEFAULT RESPONSE (for unknown endpoints)
    // ==================================================================
    const char* body_to_send =
        "{\n"
        "  \"error\": true,\n"
        "  \"message\": \"Unknown endpoint\",\n"
        "  \"available_endpoints\": [\n"
        "    \"GET /v1/weather?city=<name>&country=<code>\",\n"
        "    \"GET /v1/current?lat=<lat>&lon=<lon>\",\n"
        "    \"GET /v1/cities?query=<search>\"\n"
        "  ]\n"
        "}";

    // Construct HTTP response header
    char header[256];
    int  header_len = snprintf(header, sizeof(header),
                               "HTTP/1.1 404 Not Found\r\n"
                                "Content-Type: application/json\r\n"
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