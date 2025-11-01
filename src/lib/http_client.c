#include "http_client.h"

#include "tcp_client.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void http_client_init(HttpClient* c, int fd) {
    TCPClient tcp_client;
    tcp_client_initiate(&tcp_client, fd);
    c->tcp_client = tcp_client;
}

int http_client_init_ptr(HttpClient** c, int fd) {
    HttpClient* http_client = (HttpClient*)malloc(sizeof(HttpClient));
    if (!http_client) {
        return -1;
    }

    http_client_init(http_client, fd);
    *c = http_client;
    return 0;
}

int http_client_connect(HttpClient* c, const char* host, const char* port) {
    c->host = (char*)host;
    return tcp_client_connect(&c->tcp_client, host, port);
}

int http_client_write(HttpClient* c, const char* endpoint, const char* method,
                      const char* body) {
    char* response_body = NULL;
    if (!body) {
        response_body = "";
    } else {
        response_body = (char*)body;
    }

    size_t endpoint_len = strlen(endpoint);
    size_t method_len   = strlen(method);
    size_t body_len     = strlen(response_body);

    size_t request_len = endpoint_len + method_len + body_len + 120;

    char* request = malloc(request_len);
    if (!request) {
        return -1;
    }

    int bytes_written =
        snprintf(request, request_len,
                 "%s %s HTTP/1.1\r\nHost: %s\r\nContent-Length: "
                 "%zu\r\nConnection: close\r\n\r\n%s",
                 method, endpoint, c->host, body_len, response_body);

    if (bytes_written < 0) {
        free(request);
        return -1;
    }

    int sent = tcp_client_write_all(&c->tcp_client, (const uint8_t*)request,
                                    bytes_written);
    free(request);

    return sent;
}

int http_client_read(HttpClient* c, uint8_t* buf, int len,
                     ResponseCallback callback) {
    int recived = tcp_client_read_all(&c->tcp_client, buf, len);
    callback((char*)buf);
    return recived;
}

void http_client_disconnect(HttpClient* c) {
    tcp_client_disconnect(&c->tcp_client);
}

void http_client_dispose(HttpClient* c) { tcp_client_dispose(&c->tcp_client); }

void http_client_dispose_ptr(HttpClient* c) {
    http_client_dispose(c);
    free(c);
}
