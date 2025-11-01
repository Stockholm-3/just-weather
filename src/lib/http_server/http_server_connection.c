#include "http_server_connection.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//-----------------Internal Functions-----------------

void http_server_connection_task_work(void* context, uint64_t mon_time);

//----------------------------------------------------

int http_server_connection_initiate(HTTPServerConnection* connection, int fd) {
    tcp_client_initiate(&connection->tcpClient, fd);
    connection->buffer = NULL;

    connection->task =
        smw_create_task(connection, http_server_connection_task_work);

    return 0;
}

int http_server_connection_initiate_ptr(int                    fd,
                                        HTTPServerConnection** connection_ptr) {
    if (connection_ptr == NULL) {
        return -1;
    }

    HTTPServerConnection* connection =
        (HTTPServerConnection*)malloc(sizeof(HTTPServerConnection));
    if (connection == NULL) {
        return -2;
    }

    int result = http_server_connection_initiate(connection, fd);
    if (result != 0) {
        free(connection);
        return result;
    }

    *(connection_ptr) = connection;

    return 0;
}

void http_server_connection_set_callback(
    HTTPServerConnection* connection, void* context,
    HttpServerConnectionOnRequest on_request) {
    connection->context   = context;
    connection->onRequest = on_request;
}

void http_server_connection_task_work(void* context, uint64_t mon_time) {
    HTTPServerConnection* connection = (HTTPServerConnection*)context;
    uint8_t               chunk_buffer[256];

    int bytes_read = tcp_client_read(&connection->tcpClient, chunk_buffer,
                                     sizeof(chunk_buffer));

    if (bytes_read > 0) {
        int      new_size   = connection->buffer_size + bytes_read;
        uint8_t* new_buffer = realloc(connection->buffer, new_size);
        if (!new_buffer) {
            return;
        }
        connection->buffer = new_buffer;
        memcpy(connection->buffer + connection->buffer_size, chunk_buffer,
               bytes_read);
        connection->buffer_size += bytes_read;

        if (!connection->headers) {
            for (int i = 0; i <= connection->buffer_size - 4; i++) {
                if (connection->buffer[i] == '\r' &&
                    connection->buffer[i + 1] == '\n' &&
                    connection->buffer[i + 2] == '\r' &&
                    connection->buffer[i + 3] == '\n') {
                    int header_length   = i + 4;
                    connection->headers = malloc(header_length + 1);
                    if (!connection->headers) {
                        return;
                    }
                    memcpy(connection->headers, connection->buffer,
                           header_length);
                    connection->headers[header_length] = '\0';
                    break;
                }
            }

            if (connection->headers) {
                char method[8] = {0};
                char url[256]  = {0};

                char* data = (char*)connection->headers;

                // 1️⃣ Get the first word (method)
                sscanf(data, "%7s", method);

                // 2️⃣ Find and extract the Host header
                char* host_ptr = strstr(data, "Host:");
                if (host_ptr) {
                    sscanf(host_ptr, "Host: %255s", url);
                }

                printf("%s\n", method);
                printf("%s\n", url);

                connection->method = strdup(method);
                connection->url    = strdup(url);
                connection->onRequest(connection);
            }
        }
    }
}

void http_server_connection_dispose(HTTPServerConnection* connection) {
    tcp_client_dispose(&connection->tcpClient);
    smw_destroy_task(connection->task);
}

void http_server_connection_dispose_ptr(HTTPServerConnection** connection_ptr) {
    if (connection_ptr == NULL || *(connection_ptr) == NULL) {
        return;
    }

    http_server_connection_dispose(*(connection_ptr));
    free(*(connection_ptr));
    *(connection_ptr) = NULL;
}
