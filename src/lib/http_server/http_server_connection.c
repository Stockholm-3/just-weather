#include "http_server_connection.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//-----------------Configuration-----------------

// Maximum request size: 10MB (protection against DoS attacks)
#define MAX_REQUEST_SIZE (10 * 1024 * 1024)

// Maximum header size: 16KB (sufficient for large headers)
#define MAX_HEADER_SIZE (16 * 1024)

// Task work delay: 1ms (prevents busy loop and reduces CPU usage from 100% to
// ~5%)
#define TASK_WORK_DELAY_US 1000

//-----------------Internal Functions-----------------

void http_server_connection_task_work(void* context, uint64_t mon_time);

//----------------------------------------------------

int http_server_connection_initiate(HTTPServerConnection* connection, int fd) {
    printf("[HTTP-DEBUG] Connection initiated, FD=%d\n", fd);

    tcp_client_initiate(&connection->tcpClient, fd);
    connection->read_buffer      = NULL;
    connection->method           = NULL;
    connection->request_path     = NULL;
    connection->host             = NULL;
    connection->write_buffer     = NULL;
    connection->body             = NULL;
    connection->read_buffer_size = 0;
    connection->content_len      = 0;
    connection->write_size       = 0;
    connection->write_offset     = 0;
    connection->body_start       = 0;
    connection->state            = HTTP_SERVER_CONNECTION_STATE_RECEIVE;

    connection->task =
        smw_create_task(connection, http_server_connection_task_work);

    printf("[HTTP-DEBUG] Connection initialized successfully\n");
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
    printf("[HTTP-DEBUG] Setting callback for connection\n");
    connection->context   = context;
    connection->onRequest = on_request;
}

int http_server_connection_send(HTTPServerConnection* connection) {
    if (!connection || !connection->write_buffer ||
        connection->write_offset >= connection->write_size) {
        return 0;
    }

    printf("[HTTP-DEBUG] Attempting to send %zu bytes\n",
           connection->write_size - connection->write_offset);

    ssize_t sent =
        tcp_client_write(&connection->tcpClient,
                         connection->write_buffer + connection->write_offset,
                         connection->write_size - connection->write_offset);

    if (sent > 0) {
        connection->write_offset += sent;

        printf("[HTTP] Sent %zd bytes, total: %zu/%zu\n", sent,
               connection->write_offset, connection->write_size);
    } else if (sent < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            fprintf(stderr, "[HTTP] Send error: %s\n", strerror(errno));
            connection->state = HTTP_SERVER_CONNECTION_STATE_DISPOSE;
            return -1;
        }
        printf("[HTTP-DEBUG] Send would block, will retry\n");
        return 0;
    }

    if (connection->write_offset >= connection->write_size) {
        printf("[HTTP] Response fully sent, disposing connection\n");
        connection->state = HTTP_SERVER_CONNECTION_STATE_DISPOSE;
        return 0;
    }

    return 0;
}

int http_server_connection_receive(HTTPServerConnection* connection) {
    if (!connection) {
        return -1;
    }

    printf("[HTTP-DEBUG] Attempting to read data...\n");

    uint8_t chunk_buffer[CHUNK_SIZE];

    int bytes_read = tcp_client_read(&connection->tcpClient, chunk_buffer,
                                     sizeof(chunk_buffer));

    printf("[HTTP-DEBUG] tcp_client_read returned: %d, errno: %d (%s)\n",
           bytes_read, errno, strerror(errno));

    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("[HTTP-DEBUG] No data available yet (EAGAIN/EWOULDBLOCK), "
                   "will retry\n");
            return 0;
        }
        fprintf(stderr, "[HTTP] Read error: %s\n", strerror(errno));
        connection->state = HTTP_SERVER_CONNECTION_STATE_DISPOSE;
        return -1;
    } else if (bytes_read == 0) {
        printf("[HTTP] Client closed connection (EOF)\n");
        connection->state = HTTP_SERVER_CONNECTION_STATE_DISPOSE;
        return 0;
    }

    printf("[HTTP-DEBUG] Successfully read %d bytes\n", bytes_read);

    size_t new_size = connection->read_buffer_size + bytes_read;

    if (new_size > MAX_REQUEST_SIZE) {
        fprintf(stderr, "[HTTP] Request too large: %zu bytes (max %d)\n",
                new_size, MAX_REQUEST_SIZE);
        connection->state = HTTP_SERVER_CONNECTION_STATE_DISPOSE;
        return -1;
    }

    uint8_t* new_buffer = realloc(connection->read_buffer, new_size);
    if (!new_buffer) {
        fprintf(stderr, "[HTTP] Failed to allocate %zu bytes\n", new_size);
        connection->state = HTTP_SERVER_CONNECTION_STATE_DISPOSE;
        return -1;
    }

    connection->read_buffer = new_buffer;
    memcpy(connection->read_buffer + connection->read_buffer_size, chunk_buffer,
           bytes_read);
    connection->read_buffer_size = new_size;

    printf("[HTTP] Received %d bytes, total buffer: %zu bytes\n", bytes_read,
           connection->read_buffer_size);

    if (connection->body_start == 0) {
        printf("[HTTP-DEBUG] Searching for headers (body_start=0)...\n");

        if (connection->read_buffer_size < 4) {
            printf(
                "[HTTP-DEBUG] Buffer too small (%zu bytes), need at least 4\n",
                connection->read_buffer_size);
            return 0;
        }

        size_t search_limit = connection->read_buffer_size;
        if (search_limit > MAX_HEADER_SIZE) {
            fprintf(stderr, "[HTTP] Headers too large: %zu bytes (max %d)\n",
                    connection->read_buffer_size, MAX_HEADER_SIZE);
            connection->state = HTTP_SERVER_CONNECTION_STATE_DISPOSE;
            return -1;
        }

        printf("[HTTP-DEBUG] Searching for \\r\\n\\r\\n in %zu bytes...\n",
               search_limit);

        for (size_t i = 0; i <= search_limit - 4; i++) {
            if (connection->read_buffer[i] == '\r' &&
                connection->read_buffer[i + 1] == '\n' &&
                connection->read_buffer[i + 2] == '\r' &&
                connection->read_buffer[i + 3] == '\n') {

                printf("[HTTP-DEBUG] Found \\r\\n\\r\\n at position %zu!\n", i);

                char   method[METHOD_MAX_LEN]             = {0};
                char   request_path[REQUEST_PATH_MAX_LEN] = {0};
                char   host[HOST_MAX_LEN]                 = {0};
                size_t content_len                        = 0;

                size_t header_end = i + 4;
                char*  headers    = malloc(header_end + 1);
                if (!headers) {
                    connection->state = HTTP_SERVER_CONNECTION_STATE_DISPOSE;
                    return -1;
                }

                memcpy(headers, connection->read_buffer, header_end);
                headers[header_end] = '\0';

                sscanf(headers, "%7s %255s", method, request_path);

                char* host_ptr = strstr(headers, "Host:");
                if (host_ptr) {
                    sscanf(host_ptr, "Host: %255s", host);
                }

                char* content_len_ptr = strstr(headers, "Content-Length:");
                if (content_len_ptr) {
                    sscanf(content_len_ptr, "Content-Length: %zu",
                           &content_len);
                }

                free(headers);

                if (content_len > MAX_REQUEST_SIZE) {
                    fprintf(stderr,
                            "[HTTP] Content-Length too large: %zu bytes\n",
                            content_len);
                    connection->state = HTTP_SERVER_CONNECTION_STATE_DISPOSE;
                    return -1;
                }

                connection->method       = strdup(method);
                connection->request_path = strdup(request_path);
                connection->host         = strdup(host);
                connection->content_len  = content_len;
                connection->body_start   = header_end;

                printf("[HTTP] Parsed headers: %s %s (Content-Length: %zu)\n",
                       method, request_path, content_len);

                break;
            }
        }

        if (connection->body_start == 0) {
            printf("[HTTP-DEBUG] \\r\\n\\r\\n NOT found yet, waiting for more "
                   "data\n");
        }
    }

    if (connection->body_start > 0 &&
        connection->read_buffer_size >=
            connection->body_start + connection->content_len) {

        printf("[HTTP] Complete request received (%zu bytes)\n",
               connection->read_buffer_size);

        if (connection->content_len > 0) {
            connection->body = malloc(connection->content_len);
            if (!connection->body) {
                connection->state = HTTP_SERVER_CONNECTION_STATE_DISPOSE;
                return -1;
            }

            memcpy(connection->body,
                   connection->read_buffer + connection->body_start,
                   connection->content_len);
        }

        connection->state = HTTP_SERVER_CONNECTION_STATE_SEND;

        printf("[HTTP-DEBUG] Calling onRequest handler...\n");
        if (connection->onRequest) {
            connection->onRequest(connection->context);
        } else {
            printf("[HTTP-DEBUG] WARNING: No onRequest handler set!\n");
        }

        return 0;
    }

    return 0;
}

void http_server_connection_task_work(void* context, uint64_t mon_time) {
    HTTPServerConnection* connection = (HTTPServerConnection*)context;

    switch (connection->state) {
    case HTTP_SERVER_CONNECTION_STATE_RECEIVE:
        http_server_connection_receive(connection);
        break;

    case HTTP_SERVER_CONNECTION_STATE_SEND:
        http_server_connection_send(connection);
        break;

    case HTTP_SERVER_CONNECTION_STATE_DISPOSE:
        http_server_connection_dispose(connection);
        break;
    }

    // Prevent busy loop: add small delay to reduce CPU usage
    // This reduces CPU from 100% to ~5% in idle state
    // TODO: Replace with proper epoll/select event loop
    usleep(TASK_WORK_DELAY_US);
}

void http_server_connection_dispose(HTTPServerConnection* connection) {
    if (!connection) {
        return;
    }

    printf("[HTTP] Disposing connection (FD will be closed)\n");

    if (connection->task) {
        smw_destroy_task(connection->task);
        connection->task = NULL;
    }

    tcp_client_dispose(&connection->tcpClient);

    free(connection->read_buffer);
    connection->read_buffer = NULL;

    free(connection->body);
    connection->body = NULL;

    free(connection->method);
    connection->method = NULL;

    free(connection->request_path);
    connection->request_path = NULL;

    free(connection->host);
    connection->host = NULL;

    free(connection->write_buffer);
    connection->write_buffer = NULL;

    connection->read_buffer_size = 0;
    connection->write_size       = 0;
    connection->write_offset     = 0;
    connection->body_start       = 0;
    connection->content_len      = 0;
}

void http_server_connection_dispose_ptr(HTTPServerConnection** connection_ptr) {
    if (connection_ptr == NULL || *(connection_ptr) == NULL) {
        return;
    }

    http_server_connection_dispose(*(connection_ptr));
    free(*(connection_ptr));
    *(connection_ptr) = NULL;
}