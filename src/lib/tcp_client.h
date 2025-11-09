#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <fcntl.h>
#include <netdb.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    int fd;
} TCPClient;

int tcp_client_initiate(TCPClient* c, int fd);

int tcp_client_connect(TCPClient* c, const char* host, const char* port);

ssize_t tcp_client_write(TCPClient* c, const uint8_t* buf, size_t len);
ssize_t tcp_client_read(TCPClient* c, uint8_t* buf, size_t len);

void tcp_client_disconnect(TCPClient* c);

void tcp_client_dispose(TCPClient* c);

#endif // TCP_CLIENT_H
