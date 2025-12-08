#ifndef CLIENT_TCP_H
#define CLIENT_TCP_H

#include <stddef.h>

typedef struct {
    int fd;
} ClientTCP;

ClientTCP* client_tcp_create();
void       client_tcp_destroy(ClientTCP* tcp);
int        client_tcp_connect(ClientTCP* tcp, const char* host, int port,
                              int timeout_ms);
int        client_tcp_send(ClientTCP* tcp, const void* data, size_t len);
int  client_tcp_recv(ClientTCP* tcp, void* buffer, size_t len, int timeout_ms);
void client_tcp_close(ClientTCP* tcp);

#endif
