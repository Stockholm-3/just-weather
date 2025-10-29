#include "HTTPServerConnection.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

//-----------------Internal Functions-----------------

void HTTPServerConnection_TaskWork(void* _Context, uint64_t _MonTime);

//----------------------------------------------------

int HTTPServerConnection_Initiate(HTTPServerConnection* _Connection, int _FD) {
    TCPClient_Initiate(&_Connection->tcpClient, _FD);
    _Connection->buffer = NULL;

    _Connection->task =
        smw_createTask(_Connection, HTTPServerConnection_TaskWork);

    return 0;
}

int HTTPServerConnection_InitiatePtr(int                    _FD,
                                     HTTPServerConnection** _ConnectionPtr) {
    if (_ConnectionPtr == NULL)
        return -1;

    HTTPServerConnection* _Connection =
        (HTTPServerConnection*)malloc(sizeof(HTTPServerConnection));
    if (_Connection == NULL)
        return -2;

    int result = HTTPServerConnection_Initiate(_Connection, _FD);
    if (result != 0) {
        free(_Connection);
        return result;
    }

    *(_ConnectionPtr) = _Connection;

    return 0;
}

void HTTPServerConnection_SetCallback(
    HTTPServerConnection* _Connection, void* _Context,
    HTTPServerConnection_OnRequest _OnRequest) {
    _Connection->context   = _Context;
    _Connection->onRequest = _OnRequest;
}

void HTTPServerConnection_TaskWork(void* _Context, uint64_t _MonTime) {
    HTTPServerConnection* _Connection = (HTTPServerConnection*)_Context;
    uint8_t               chunk_buffer[256];

    int bytes_read = TCPClient_Read(&_Connection->tcpClient, chunk_buffer,
                                    sizeof(chunk_buffer));

    if (bytes_read > 0) {
        int      new_size   = _Connection->buffer_size + bytes_read;
        uint8_t* new_buffer = realloc(_Connection->buffer, new_size);
        if (!new_buffer) {
            return;
        }
        _Connection->buffer = new_buffer;
        memcpy(_Connection->buffer + _Connection->buffer_size, chunk_buffer,
               bytes_read);
        _Connection->buffer_size += bytes_read;

        if (!_Connection->headers) {
            for (int i = 0; i <= _Connection->buffer_size - 4; i++) {
                if (_Connection->buffer[i] == '\r' &&
                    _Connection->buffer[i + 1] == '\n' &&
                    _Connection->buffer[i + 2] == '\r' &&
                    _Connection->buffer[i + 3] == '\n') {
                    int header_length    = i + 4;
                    _Connection->headers = malloc(header_length + 1);
                    if (!_Connection->headers) {
                        return;
                    }
                    memcpy(_Connection->headers, _Connection->buffer,
                           header_length);
                    _Connection->headers[header_length] = '\0';
                    break;
                }
            }

            if (_Connection->headers) {
                char method[8] = {0};
                char url[256]  = {0};

                char* data = (char*)_Connection->headers;

                // 1️⃣ Get the first word (method)
                sscanf(data, "%7s", method);

                // 2️⃣ Find and extract the Host header
                char* host_ptr = strstr(data, "Host:");
                if (host_ptr) {
                    sscanf(host_ptr, "Host: %255s", url);
                }

                printf("%s\n", method);
                printf("%s\n", url);

                _Connection->method = strdup(method);
                _Connection->url    = strdup(url);
                _Connection->onRequest(_Connection);
            }
        }
    }
}

void HTTPServerConnection_Dispose(HTTPServerConnection* _Connection) {
    TCPClient_Dispose(&_Connection->tcpClient);
    smw_destroyTask(_Connection->task);
}

void HTTPServerConnection_DisposePtr(HTTPServerConnection** _ConnectionPtr) {
    if (_ConnectionPtr == NULL || *(_ConnectionPtr) == NULL)
        return;

    HTTPServerConnection_Dispose(*(_ConnectionPtr));
    free(*(_ConnectionPtr));
    *(_ConnectionPtr) = NULL;
}
