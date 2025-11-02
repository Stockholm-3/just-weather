#include "HTTPClient.h"

#include "TCPClient.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void HTTPClient_init(HTTPClient* c, int FD) {
    TCPClient tcp_client;
    TCPClient_Initiate(&tcp_client, FD);
    c->tcp_client = tcp_client;
}

int HTTPClient_initPtr(HTTPClient** c, int FD) {
    HTTPClient* http_client = (HTTPClient*)malloc(sizeof(HTTPClient));
    if (!http_client) {
        return -1;
    }

    HTTPClient_init(http_client, FD);
    *c = http_client;
    return 0;
}

int HTTPClient_connect(HTTPClient* c, const char* host, const char* port) {
    c->host = (char*)host;
    return TCPClient_Connect(&c->tcp_client, host, port);
}

int HTTPClient_Write(HTTPClient* c, const char* endpoint, const char* method,
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

    int sent = TCPClient_WriteAll(&c->tcp_client, (const uint8_t*)request,
                                  bytes_written);
    free(request);

    return sent;
}

int HTTPClient_Read(HTTPClient* c, uint8_t* buf, int len,
                    response_callback callback) {
    int recived = TCPClient_ReadAll(&c->tcp_client, buf, len);
    callback((char*)buf);
    return recived;
}

void HTTPClient_Disconnect(HTTPClient* c) {
    TCPClient_Disconnect(&c->tcp_client);
}

void HTTPClient_Dispose(HTTPClient* c) { TCPClient_Dispose(&c->tcp_client); }

void HTTPClient_DisposePtr(HTTPClient* c) {
    HTTPClient_Dispose(c);
    free(c);
}
