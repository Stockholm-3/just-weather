#include "client_tcp.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

ClientTCP* client_tcp_create() {
    ClientTCP* tcp = malloc(sizeof(ClientTCP));
    if (!tcp) {
        return NULL;
    }
    tcp->fd = -1;
    return tcp;
}

void client_tcp_destroy(ClientTCP* tcp) {
    if (!tcp) {
        return;
    }
    client_tcp_close(tcp);
    free(tcp);
}

int client_tcp_connect(ClientTCP* tcp, const char* host, int port,
                       int timeout_ms) {
    if (!tcp || !host) {
        return -1;
    }

    if (tcp->fd >= 0) {
        return -1;
    }

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo  hints = {0};
    struct addrinfo* res   = NULL;

    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int gai_result = getaddrinfo(host, port_str, &hints, &res);
    if (gai_result != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(gai_result));
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

        int connect_result = connect(fd, rp->ai_addr, rp->ai_addrlen);

        if (connect_result == 0) {
            break;
        }

        if (errno == EINPROGRESS) {
            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(fd, &write_fds);

            struct timeval tv;
            tv.tv_sec  = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

            int select_result = select(fd + 1, NULL, &write_fds, NULL, &tv);

            if (select_result > 0) {
                int       error     = 0;
                socklen_t error_len = sizeof(error);
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &error_len) ==
                        0 &&
                    error == 0) {
                    break;
                }
            }
        }

        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);

    if (fd < 0) {
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    tcp->fd = fd;
    return 0;
}

int client_tcp_send(ClientTCP* tcp, const void* data, size_t len) {
    if (!tcp || tcp->fd < 0 || !data) {
        return -1;
    }

    size_t total_sent = 0;
    while (total_sent < len) {
        ssize_t sent =
            send(tcp->fd, (const char*)data + total_sent, len - total_sent, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        total_sent += sent;
    }

    return 0;
}

int client_tcp_recv(ClientTCP* tcp, void* buffer, size_t len, int timeout_ms) {
    if (!tcp || tcp->fd < 0 || !buffer) {
        return -1;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(tcp->fd, &read_fds);

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int select_result = select(tcp->fd + 1, &read_fds, NULL, NULL, &tv);

    if (select_result < 0) {
        return -1;
    }

    if (select_result == 0) {
        errno = ETIMEDOUT;
        return -1;
    }

    ssize_t received = recv(tcp->fd, buffer, len, 0);
    if (received < 0) {
        return -1;
    }

    return (int)received;
}

void client_tcp_close(ClientTCP* tcp) {
    if (!tcp || tcp->fd < 0) {
        return;
    }
    close(tcp->fd);
    tcp->fd = -1;
}
