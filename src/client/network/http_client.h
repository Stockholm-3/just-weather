#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "client_tcp.h"

#include <stddef.h>

typedef struct {
    ClientTCP* tcp;
    char       url[1024];
    int        status_code;
    char*      response_body;
    size_t     response_size;
    int        timeout_ms;
} HttpClient;

HttpClient* http_client_create(int timeout_ms);
void        http_client_destroy(HttpClient* client);
int         http_client_get(HttpClient* client, const char* url, char** error);
int         http_client_get_status_code(HttpClient* client);
const char* http_client_get_body(HttpClient* client);
size_t      http_client_get_body_size(HttpClient* client);

#endif
