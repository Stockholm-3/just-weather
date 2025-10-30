#include "TCPClient.h"

int TCPClient_Initiate(TCPClient* c, int fd) {
    c->fd = fd;
    return 0;
}

int TCPClient_Connect(TCPClient* c, const char* host, const char* port) {
    if (c->fd >= 0)
        return -1;

    struct addrinfo  hints = {0};
    struct addrinfo* res   = NULL;

    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host, port, &hints, &res) != 0)
        return -1;

    /*
    Funktionen getaddrinfo() kan ge en länkad lista av adressförslag för samma
    värd och port. Till exempel kan en server ha både IPv4- och IPv6-adresser,
    eller flera nätverkskort.

    Varje nod i listan (struct addrinfo) innehåller en möjlig adress att prova.
    Om första adressen inte fungerar (t.ex. connect() misslyckas), försöker man
    nästa.
    */

    int fd = -1;
    for (struct addrinfo* rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (fd < 0)
            continue;

        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;

        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    if (fd < 0)
        return -1;

    c->fd = fd;
    return 0;
}

int TCPClient_Write(TCPClient* c, const uint8_t* buf, int len) {
    return send(c->fd, buf, len, 0);
}

int TCPClient_Read(TCPClient* c, uint8_t* buf, int len) {
    return recv(c->fd, buf, len, 0);
}

// Writes all data from buf to socket
int TCPClient_WriteAll(TCPClient* c, const uint8_t* buf, int len) {
    int total_sent = 0;

    while (total_sent < len) {
        int n = TCPClient_Write(c, buf + total_sent, len - total_sent);
        if (n < 0)
            return -1;
        total_sent += n;
    }

    return total_sent;
}

// Revives all data that can fit in the give buf
// if messages is longer that buf the messages will be cut of
int TCPClient_ReadAll(TCPClient* c, uint8_t* buf, int len) {
    int total_received = 0;

    while (total_received < len) {
        int n = TCPClient_Read(c, buf + total_received, len - total_received);
        if (n < 0)
            return -1; // error
        if (n == 0)
            break; // connection closed
        total_received += n;
    }

    return total_received;
}

void TCPClient_Disconnect(TCPClient* c) {
    if (c->fd >= 0)
        close(c->fd);

    c->fd = -1;
}

void TCPClient_Dispose(TCPClient* c) { TCPClient_Disconnect(c); }
