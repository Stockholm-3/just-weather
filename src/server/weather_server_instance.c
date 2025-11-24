#include "weather_server_instance.h"

#include "open_meteo_handler.h"

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

    printf("[WEATHER] onRequest: %s %s\n",
           conn->method ? conn->method : "UNKNOWN",
           conn->request_path ? conn->request_path : "UNKNOWN");

    // Parse URL to get path and query
    char path[256]  = {0};
    char query[512] = {0};

    if (conn->request_path) {
        char* question_mark = strchr(conn->request_path, '?');
        if (question_mark) {
            size_t path_len = question_mark - conn->request_path;
            if (path_len < sizeof(path)) {
                strncpy(path, conn->request_path, path_len);
                path[path_len] = '\0';
            }
            if (strlen(question_mark + 1) < sizeof(query)) {
                strcpy(query, question_mark + 1);
            }
        } else {
            if (strlen(conn->request_path) < sizeof(path)) {
                strcpy(path, conn->request_path);
            }
        }
    }

    // ============================================================
    // Weather endpoint: /v1/current (GET or POST)
    // ============================================================
    if ((strcmp(conn->method, "GET") == 0 ||
         strcmp(conn->method, "POST") == 0) &&
        strcmp(path, "/v1/current") == 0) {

        printf("[WEATHER] Routing to Open-Meteo API\n");

        char* json_response = NULL;
        int   status_code   = 0;

        open_meteo_handler_current(query, &json_response, &status_code);

        if (json_response) {
            char header[256];
            int  header_len =
                snprintf(header, sizeof(header),
                         "HTTP/1.1 %d %s\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: %zu\r\n"
                         "Connection: close\r\n"
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

                printf("[WEATHER] Weather response prepared: %zu bytes\n",
                       total_len);
            }

            free(json_response);
            return 0;
        }
    }

    // ============================================================
    // Echo endpoint: / or /echo (GET or POST ONLY)
    // ============================================================
    if (strcmp(path, "/echo") == 0 || strcmp(path, "/") == 0) {

        // Check method - only GET and POST allowed
        if (strcmp(conn->method, "GET") != 0 &&
            strcmp(conn->method, "POST") != 0) {
            printf("[WEATHER] Method not allowed: %s (echo accepts only "
                   "GET/POST)\n",
                   conn->method);

            const char* body = "{\n"
                               "  \"error\": \"Method Not Allowed\",\n"
                               "  \"message\": \"Echo endpoint supports only "
                               "GET and POST methods\",\n"
                               "  \"allowed_methods\": [\"GET\", \"POST\"],\n"
                               "  \"your_method\": \"%s\"\n"
                               "}\n";

            char body_with_method[512];
            int  body_len =
                snprintf(body_with_method, sizeof(body_with_method), body,
                         conn->method ? conn->method : "UNKNOWN");

            char header[256];
            int  header_len = snprintf(header, sizeof(header),
                                       "HTTP/1.1 405 Method Not Allowed\r\n"
                                        "Content-Type: application/json\r\n"
                                        "Allow: GET, POST\r\n"
                                        "Content-Length: %d\r\n"
                                        "Connection: close\r\n"
                                        "\r\n",
                                       body_len);

            size_t   total_len = header_len + body_len;
            uint8_t* response  = malloc(total_len);

            if (response) {
                memcpy(response, header, header_len);
                memcpy(response + header_len, body_with_method, body_len);

                conn->write_buffer = response;
                conn->write_size   = total_len;

                printf("[WEATHER] 405 response sent for method: %s\n",
                       conn->method);
            }

            return 0;
        }

        // Method is GET or POST - process echo
        printf("[WEATHER] Echo endpoint - returning raw request (method: %s)\n",
               conn->method);

        size_t request_size = conn->read_buffer_size;

        printf("[WEATHER] Echo: raw request size = %zu bytes\n", request_size);

        // Build HTTP response header
        char header[256];
        int  header_len = snprintf(header, sizeof(header),
                                   "HTTP/1.1 200 OK\r\n"
                                    "Content-Type: text/plain\r\n"
                                    "Content-Length: %zu\r\n"
                                    "Connection: close\r\n"
                                    "\r\n",
                                   request_size);

        // Allocate response
        size_t   total_len = header_len + request_size;
        uint8_t* response  = malloc(total_len);

        if (!response) {
            fprintf(stderr, "[WEATHER] Failed to allocate %zu bytes for echo\n",
                    total_len);

            const char* error     = "HTTP/1.1 500 Internal Server Error\r\n"
                                    "Content-Length: 0\r\n"
                                    "Connection: close\r\n"
                                    "\r\n";
            size_t      error_len = strlen(error);
            uint8_t*    error_response = malloc(error_len);
            if (error_response) {
                memcpy(error_response, error, error_len);
                conn->write_buffer = error_response;
                conn->write_size   = error_len;
            }
            return -1;
        }

        // Copy header
        memcpy(response, header, header_len);

        // Copy raw request (ECHO!)
        memcpy(response + header_len, conn->read_buffer, request_size);

        conn->write_buffer = response;
        conn->write_size   = total_len;

        printf(
            "[WEATHER] Echo response ready: %zu bytes (%d header + %zu body)\n",
            total_len, header_len, request_size);

        return 0;
    }

    // ============================================================
    // 404 Not Found for other paths
    // ============================================================
    printf("[WEATHER] 404 Not Found: %s\n", path);

    const char* body_404 = "{\n"
                           "  \"error\": \"Not Found\",\n"
                           "  \"path\": \"%s\",\n"
                           "  \"message\": \"Available endpoints: /v1/current "
                           "(weather), / (echo), /echo (echo)\"\n"
                           "}\n";

    char body[512];
    int  body_len = snprintf(body, sizeof(body), body_404, path);

    char header[256];
    int  header_len = snprintf(header, sizeof(header),
                               "HTTP/1.1 404 Not Found\r\n"
                                "Content-Type: application/json\r\n"
                                "Content-Length: %d\r\n"
                                "Connection: close\r\n"
                                "\r\n",
                               body_len);

    size_t   total_len = header_len + body_len;
    uint8_t* response  = malloc(total_len);

    if (response) {
        memcpy(response, header, header_len);
        memcpy(response + header_len, body, body_len);

        conn->write_buffer = response;
        conn->write_size   = total_len;

        printf("[WEATHER] 404 response sent\n");
    }

    return 0;
}

void weather_server_instance_work(WeatherServerInstance* instance,
                                  uint64_t               mon_time) {
    // Nothing to do here - all work is event-driven
}

void weather_server_instance_dispose(WeatherServerInstance* instance) {
    // Nothing to dispose
}

void weather_server_instance_dispose_ptr(WeatherServerInstance** instance_ptr) {
    if (instance_ptr == NULL || *(instance_ptr) == NULL) {
        return;
    }

    weather_server_instance_dispose(*(instance_ptr));
    free(*(instance_ptr));
    *(instance_ptr) = NULL;
}