#include "tcp_client.h"

#include <errno.h>
#include <stddef.h>

int tcp_client_initiate(TCPClient* c, int fd) {
    c->fd = fd;
    return 0;
}

int tcp_client_connect(TCPClient* c, const char* host, const char* port) {
    if (c->fd >= 0) {
        return -1;
    }

    struct addrinfo  hints = {0};
    struct addrinfo* res   = NULL;

    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host, port, &hints, &res) != 0) {
        return -1;
    }

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

        if (fd < 0) {
            continue;
        }

        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }

        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    if (fd < 0) {
        return -1;
    }

    c->fd = fd;
    return 0;
}

int tcp_client_write(TCPClient* c, const uint8_t* buf, size_t len) {
    return send(c->fd, buf, len, MSG_NOSIGNAL);
}

int tcp_client_read(TCPClient* c, uint8_t* buf, size_t len) {
    int n = recv(c->fd, buf, len, 0); // or MSG_DONTWAIT
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; // no data available right now
        }
        return -1; // real error
    }
    return n;
}

// Writes all data from buf to socket
int tcp_client_write_all(TCPClient* c, const uint8_t* buf, size_t len) {
    int total_sent = 0;

    while (total_sent < len) {
        int n = tcp_client_write(c, buf + total_sent, len - total_sent);
        if (n < 0) {
            return -1;
        }
        total_sent += n;
    }

    return total_sent;
}

// Revives all data that can fit in the give buf
// if messages is longer that buf the messages will be cut of
int tcp_client_read_all(TCPClient* c, uint8_t* buf, size_t len) {
    int total_received = 0;

    while (total_received < len) {
        int n = tcp_client_read(c, buf + total_received, len - total_received);
        if (n < 0) {
            return -1; // error
        }
        if (n == 0) {
            break; // connection closed
        }
        total_received += n;
    }

    return total_received;
}

void tcp_client_disconnect(TCPClient* c) {
    if (c->fd >= 0) {
        close(c->fd);
    }

    c->fd = -1;
}

void tcp_client_dispose(TCPClient* c) { tcp_client_disconnect(c); }
