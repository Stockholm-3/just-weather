// Simple TCP client.
// It has ReadAll and WriteAll helper functions.
#ifndef __TCPClient_h_
#define __TCPClient_h_

#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <netdb.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    int fd;
} TCPClient;

int TCPClient_Initiate(TCPClient* c, int _FD);

int TCPClient_Connect(TCPClient* c, const char* host, const char* port);

int TCPClient_Write(TCPClient* c, const uint8_t* buf, int len);
int TCPClient_Read(TCPClient* c, uint8_t* buf, int len);

int TCPClient_WriteAll(TCPClient* c, const uint8_t* buf, int len);

// Reads all data that it can into the give buffer at once.
// If the buffer is too small the message will be cut of.
int TCPClient_ReadAll(TCPClient* c, uint8_t* buf, int len);

void TCPClient_Disconnect(TCPClient* c);

void TCPClient_Dispose(TCPClient* c);

#endif // __TCPClient_h_
