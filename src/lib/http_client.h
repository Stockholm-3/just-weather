/// Simple TCP based HTTP client that writes and reads all.
/// This is a blocking HTTPClient
/// Written on top of TCPClient.h
#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "tcp_client.h"

#include <netdb.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef void (*ResponseCallback)(const char* response);

typedef struct {
    TCPClient tcp_client;
    char*     host;

} HttpClient;

void http_client_init(HttpClient* c, int fd);

// Create HTTPClient on heap
// Make sure to use HTTPClient_DisposePtr to dispose it correctly
int http_client_init_ptr(HttpClient** c, int fd);

int http_client_connect(HttpClient* c, const char* host, const char* port);

/// Creats relevant headers from method and endpoint.
/// Uses Connection: close header to intsantly close.
/// Adds correct body length header
int http_client_write(HttpClient* c, const char* endpoint, const char* method,
                     const char* body);

// Reads all data that it can into the give buffer at once.
// If the buffer is too small the message will be cut of.
int http_client_read(HttpClient* c, uint8_t* buf, int len,
                    ResponseCallback callback);

void http_client_disconnect(HttpClient* c);

void http_client_dispose(HttpClient* c);

void http_client_dispose_ptr(HttpClient* c);

#endif // HTTP_CLIENT_H
