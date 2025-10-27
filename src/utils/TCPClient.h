#ifndef __TCPClient_h_
#define __TCPClient_h_

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct TCPClient {
    int fd;

} TCPClient;

int TCPClient_Initiate(TCPClient* c);

int TCPClient_Connect(TCPClient* c, const char* host, const char* port);

int TCPClient_Write(TCPClient* c, const uint8_t* buf, int len);
int TCPClient_Read(TCPClient* c, uint8_t* buf, int len);

void TCPClient_Disconnect(TCPClient* c);

void TCPClient_Dispose(TCPClient* c);

#endif // __TCPClient_h_