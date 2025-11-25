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
    int n = recv(c->fd, buf, len, 0);

    if (n < 0) {
        // Error occurred
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available right now - this is NOT an error in
            // non-blocking mode Return -1 to indicate "try again later"
            return -1;
        }
        // Real error (connection reset, etc)
        return -1;
    }

    if (n == 0) {
        // Connection closed by peer (EOF)
        // Return 0 to indicate EOF
        return 0;
    }

    // Successfully read n bytes
    return n;
}

void tcp_client_disconnect(TCPClient* c) {
    if (c->fd >= 0) {
        close(c->fd);
    }

    c->fd = -1;
}

void tcp_client_dispose(TCPClient* c) { tcp_client_disconnect(c); }