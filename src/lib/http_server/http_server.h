
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "../tcp_server.h"
#include "http_server_connection.h"
#include "smw.h"

typedef int (*HTTPServer_OnConnection)(void*                 _Context,
                                       HTTPServerConnection* _Connection);

typedef struct {
    HTTPServer_OnConnection onConnection;

    TCPServer tcpServer;
    smw_task* task;

} HTTPServer;

int HTTPServer_Initiate(HTTPServer*             _Server,
                        HTTPServer_OnConnection _OnConnection);
int HTTPServer_InitiatePtr(HTTPServer_OnConnection _OnConnection,
                           HTTPServer**            _ServerPtr);

void HTTPServer_Dispose(HTTPServer* _Server);
void HTTPServer_DisposePtr(HTTPServer** _ServerPtr);

#endif //HTTP_SERVER_H
