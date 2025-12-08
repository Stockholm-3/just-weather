#include "http_client.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_url(const char* url, char* hostname, int* port, char* path);
static int send_request(HttpClient* client, const char* host, const char* path);
static int receive_response(HttpClient* client);
static int parse_headers(const char* data, size_t len, int* status_code,
                         size_t* content_length, int* chunked);
static int decode_chunked(const uint8_t* in, size_t in_len, char** out,
                          size_t* out_len);

HttpClient* http_client_create(int timeout_ms) {
    HttpClient* client = malloc(sizeof(HttpClient));
    if (!client) {
        return NULL;
    }

    client->tcp           = client_tcp_create();
    client->status_code   = 0;
    client->response_body = NULL;
    client->response_size = 0;
    client->timeout_ms    = timeout_ms > 0 ? timeout_ms : 5000;

    if (!client->tcp) {
        free(client);
        return NULL;
    }

    return client;
}

void http_client_destroy(HttpClient* client) {
    if (!client) {
        return;
    }

    if (client->response_body) {
        free(client->response_body);
    }

    if (client->tcp) {
        client_tcp_destroy(client->tcp);
    }

    free(client);
}

int http_client_get(HttpClient* client, const char* url, char** error) {
    if (!client || !url) {
        if (error) {
            *error = strdup("Invalid parameters");
        }
        return -1;
    }

    char hostname[256];
    int  port;
    char path[512];

    if (parse_url(url, hostname, &port, path) != 0) {
        if (error) {
            *error = strdup("Failed to parse URL");
        }
        return -1;
    }

    if (client_tcp_connect(client->tcp, hostname, port, client->timeout_ms) !=
        0) {
        if (error) {
            *error = strdup("Connection failed");
        }
        return -1;
    }

    if (send_request(client, hostname, path) != 0) {
        if (error) {
            *error = strdup("Failed to send request");
        }
        client_tcp_close(client->tcp);
        return -1;
    }

    if (receive_response(client) != 0) {
        if (error) {
            *error = strdup("Failed to receive response");
        }
        client_tcp_close(client->tcp);
        return -1;
    }

    client_tcp_close(client->tcp);

    if (client->status_code < 200 || client->status_code >= 600) {
        if (error) {
            char err_msg[100];
            snprintf(err_msg, sizeof(err_msg), "HTTP %d", client->status_code);
            *error = strdup(err_msg);
        }
        return -1;
    }

    return 0;
}

int http_client_get_status_code(HttpClient* client) {
    return client ? client->status_code : 0;
}

const char* http_client_get_body(HttpClient* client) {
    return client ? client->response_body : NULL;
}

size_t http_client_get_body_size(HttpClient* client) {
    return client ? client->response_size : 0;
}

static int parse_url(const char* url, char* hostname, int* port, char* path) {
    if (url == NULL || hostname == NULL || port == NULL || path == NULL) {
        return -1;
    }

    *port = 80;
    strcpy(path, "/");

    const char* start = url;
    if (strncmp(url, "http://", 7) == 0) {
        start = url + 7;
        *port = 80;
    } else if (strncmp(url, "https://", 8) == 0) {
        start = url + 8;
        *port = 443;
    }

    const char* end = start;
    while (*end && *end != ':' && *end != '/') {
        end++;
    }

    int hostname_len = end - start;
    if (hostname_len == 0 || hostname_len > 255) {
        return -1;
    }

    strncpy(hostname, start, hostname_len);
    hostname[hostname_len] = '\0';

    if (*end == ':') {
        end++;
        const char* port_start = end;
        while (*end && *end != '/') {
            end++;
        }

        int port_len = end - port_start;
        if (port_len > 0 && port_len < 16) {
            char port_str[16];
            strncpy(port_str, port_start, port_len);
            port_str[port_len] = '\0';
            *port              = atoi(port_str);
        }
    }

    if (*end == '/') {
        strncpy(path, end, 511);
        path[511] = '\0';
    }

    return 0;
}

static int send_request(HttpClient* client, const char* host,
                        const char* path) {
    char request[2048];
    int  len = snprintf(request, sizeof(request),
                        "GET %s HTTP/1.1\r\n"
                         "Host: %s\r\n"
                         "User-Agent: just-weather-client/1.0\r\n"
                         "Accept: application/json\r\n"
                         "Connection: close\r\n"
                         "\r\n",
                        path, host);

    if (len < 0 || len >= (int)sizeof(request)) {
        return -1;
    }

    return client_tcp_send(client->tcp, request, len);
}

static int receive_response(HttpClient* client) {
    char   buffer[8192];
    size_t total_received = 0;
    char*  full_response  = NULL;

    while (1) {
        int received = client_tcp_recv(client->tcp, buffer, sizeof(buffer) - 1,
                                       client->timeout_ms);

        if (received < 0) {
            free(full_response);
            return -1;
        }

        if (received == 0) {
            break;
        }

        char* new_response =
            realloc(full_response, total_received + received + 1);
        if (!new_response) {
            free(full_response);
            return -1;
        }

        full_response = new_response;
        memcpy(full_response + total_received, buffer, received);
        total_received += received;
        full_response[total_received] = '\0';
    }

    if (!full_response || total_received == 0) {
        free(full_response);
        return -1;
    }

    size_t content_length = 0;
    int    is_chunked     = 0;

    if (parse_headers(full_response, total_received, &client->status_code,
                      &content_length, &is_chunked) != 0) {
        free(full_response);
        return -1;
    }

    const char* body_start = strstr(full_response, "\r\n\r\n");
    if (!body_start) {
        free(full_response);
        return -1;
    }
    body_start += 4;

    size_t header_len = body_start - full_response;
    size_t body_len   = total_received - header_len;

    if (is_chunked) {
        char*  decoded_body = NULL;
        size_t decoded_len  = 0;

        if (decode_chunked((uint8_t*)body_start, body_len, &decoded_body,
                           &decoded_len) == 0) {
            free(full_response);
            client->response_body = decoded_body;
            client->response_size = decoded_len;
            return 0;
        } else {
            free(full_response);
            return -1;
        }
    } else {
        client->response_body = malloc(body_len + 1);
        if (!client->response_body) {
            free(full_response);
            return -1;
        }

        memcpy(client->response_body, body_start, body_len);
        client->response_body[body_len] = '\0';
        client->response_size           = body_len;
        free(full_response);
        return 0;
    }
}

static int parse_headers(const char* data, size_t len, int* status_code,
                         size_t* content_length, int* chunked) {
    *status_code    = 0;
    *content_length = 0;
    *chunked        = 0;

    const char* line_end = strstr(data, "\r\n");
    if (!line_end) {
        return -1;
    }

    if (sscanf(data, "HTTP/%*d.%*d %d", status_code) != 1) {
        return -1;
    }

    const char* current = line_end + 2;
    while (current < data + len) {
        line_end = strstr(current, "\r\n");
        if (!line_end) {
            break;
        }

        if (line_end == current) {
            break;
        }

        if (strncasecmp(current, "Content-Length:", 15) == 0) {
            sscanf(current + 15, "%zu", content_length);
        } else if (strncasecmp(current, "Transfer-Encoding:", 18) == 0) {
            if (strstr(current, "chunked")) {
                *chunked = 1;
            }
        }

        current = line_end + 2;
    }

    return 0;
}

static int decode_chunked(const uint8_t* in, size_t in_len, char** out,
                          size_t* out_len) {
    if (!in || !out || !out_len) {
        return -1;
    }

    size_t pos   = 0;
    size_t alloc = 1024;
    char*  buf   = malloc(alloc);
    if (!buf) {
        return -2;
    }
    size_t buf_len = 0;

    while (pos < in_len) {
        size_t line_start = pos;
        while (pos < in_len &&
               !(in[pos] == '\r' && pos + 1 < in_len && in[pos + 1] == '\n')) {
            pos++;
        }

        if (pos >= in_len) {
            free(buf);
            return -3;
        }

        size_t line_len = pos - line_start;
        if (line_len == 0) {
            free(buf);
            return -4;
        }

        char* hex = malloc(line_len + 1);
        if (!hex) {
            free(buf);
            return -5;
        }
        memcpy(hex, in + line_start, line_len);
        hex[line_len] = '\0';

        char*         endptr     = NULL;
        unsigned long chunk_size = strtoul(hex, &endptr, 16);
        if (endptr == hex) {
            free(hex);
            free(buf);
            return -6;
        }
        free(hex);

        pos += 2;

        if (chunk_size == 0) {
            if (pos + 1 < in_len && in[pos] == '\r' && in[pos + 1] == '\n') {
                pos += 2;
            }
            break;
        }

        if (pos + chunk_size > in_len) {
            free(buf);
            return -7;
        }

        if (buf_len + chunk_size + 1 > alloc) {
            while (buf_len + chunk_size + 1 > alloc) {
                alloc *= 2;
            }
            char* nbuf = realloc(buf, alloc);
            if (!nbuf) {
                free(buf);
                return -8;
            }
            buf = nbuf;
        }

        memcpy(buf + buf_len, in + pos, chunk_size);
        buf_len += chunk_size;
        pos += chunk_size;

        if (pos + 1 >= in_len || in[pos] != '\r' || in[pos + 1] != '\n') {
            free(buf);
            return -9;
        }
        pos += 2;
    }

    if (buf_len + 1 > alloc) {
        char* nbuf = realloc(buf, buf_len + 1);
        if (!nbuf) {
            free(buf);
            return -10;
        }
        buf = nbuf;
    }
    buf[buf_len] = '\0';

    *out     = buf;
    *out_len = buf_len;
    return 0;
}
