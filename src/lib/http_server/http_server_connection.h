#ifndef HTTP_SERVER_CONNECTION_H
#define HTTP_SERVER_CONNECTION_H

#include "../tcp_client.h"
#include "smw.h"

#include <stdint.h>

typedef int (*HttpServerConnectionOnRequest)(void* context);

typedef struct {
    TCPClient tcpClient;

    void*                         context;
    HttpServerConnectionOnRequest onRequest;

    char* method;
    char* url;

    uint8_t* buffer;
    size_t   buffer_size;

    uint8_t* headers;

    SmwTask* task;

} HTTPServerConnection;

int http_server_connection_initiate(HTTPServerConnection* connection, int fd);
int http_server_connection_initiate_ptr(int                    fd,
                                        HTTPServerConnection** connection_ptr);

void http_server_connection_set_callback(
    HTTPServerConnection* connection, void* context,
    HttpServerConnectionOnRequest on_request);

void http_server_connection_dispose(HTTPServerConnection* connection);
void http_server_connection_dispose_ptr(HTTPServerConnection** connection_ptr);

#endif // HTTP_SERVER_CONNECTION_H
