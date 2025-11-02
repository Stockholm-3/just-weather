/// Simple TCP based HTTP client that writes and reads all.
/// This is a blocking HTTPClient
/// Written on top of TCPClient.h
#ifndef __HTTPClient_h_
#define __HTTPClient_h_

#include "TCPClient.h"
#include <netdb.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef void (*response_callback)(const char *response);

typedef struct {
  TCPClient tcp_client;
  char *host;

} HTTPClient;

void HTTPClient_init(HTTPClient *c, int FD);

// Create HTTPClient on heap
// Make sure to use HTTPClient_DisposePtr to dispose it correctly
int HTTPClient_initPtr(HTTPClient **c, int FD);

int HTTPClient_connect(HTTPClient *c, const char *host, const char *port);

/// Creats relevant headers from method and endpoint.
/// Uses Connection: close header to intsantly close.
/// Adds correct body length header
int HTTPClient_Write(HTTPClient *c, const char *endpoint, const char *method,
                     const char *body);

// Reads all data that it can into the give buffer at once.
// If the buffer is too small the message will be cut of.
int HTTPClient_Read(HTTPClient *c, uint8_t *buf, int len,
                    response_callback callback);

void HTTPClient_Disconnect(HTTPClient *c);

void HTTPClient_Dispose(HTTPClient *c);

void HTTPClient_DisposePtr(HTTPClient *c);

#endif // __HTTPClient_h_
