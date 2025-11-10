// tests/tcp_client_tests.c
#define _GNU_SOURCE
#include "tests.h"
#include "../TCPClient.h"
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

typedef struct {
    int listen_fd;
    unsigned short port;    // выбранный порт
    pthread_mutex_t mtx;
    pthread_cond_t cv;
    int ready;              // сервер привязался и слушает
    int done;               // обработал один запрос
} MockSrv;

static void* server_thread(void* arg) {
    MockSrv* s = (MockSrv*)arg;

    // v6 сокет, принимаем и v4 (::ffff:127.0.0.1)
    int fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return NULL; }

    int off = 0;
    setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in6 addr = {0};
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(0); // порт 0 → ОС выдаст свободный

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); close(fd); return NULL; }
    if (listen(fd, 1) < 0) { perror("listen"); close(fd); return NULL; }

    // узнаём порт
    struct sockaddr_in6 bnd = {0}; socklen_t bl = sizeof(bnd);
    if (getsockname(fd, (struct sockaddr*)&bnd, &bl) == 0) {
        pthread_mutex_lock(&s->mtx);
        s->listen_fd = fd;
        s->port = ntohs(bnd.sin6_port);
        s->ready = 1;
        pthread_cond_broadcast(&s->cv);
        pthread_mutex_unlock(&s->mtx);
    } else { perror("getsockname"); close(fd); return NULL; }

    // один клиент
    int cfd = accept(fd, NULL, NULL);
    if (cfd >= 0) {
        char buf[256];
        ssize_t n = recv(cfd, buf, sizeof(buf)-1, 0);
        if (n > 0) {
            // простая логика: на любое — pong\n
            const char* resp = "pong\n";
            send(cfd, resp, strlen(resp), MSG_NOSIGNAL);
        }
        close(cfd);
    }

    close(fd);
    pthread_mutex_lock(&s->mtx);
    s->done = 1;
    pthread_mutex_unlock(&s->mtx);
    return NULL;
}

static int wait_ready(MockSrv* s, int ms) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec  += ms/1000;
    ts.tv_nsec += (ms%1000)*1000000L;
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }

    pthread_mutex_lock(&s->mtx);
    while (!s->ready) {
        if (pthread_cond_timedwait(&s->cv, &s->mtx, &ts) == ETIMEDOUT) {
            pthread_mutex_unlock(&s->mtx);
            return -1;
        }
    }
    pthread_mutex_unlock(&s->mtx);
    return 0;
}

TEST(tcp_client_ping_pong) {
    MockSrv S = { .listen_fd = -1, .port = 0, .ready = 0, .done = 0 };
    pthread_mutex_init(&S.mtx, NULL);
    pthread_cond_init(&S.cv, NULL);

    pthread_t th;
    assert(pthread_create(&th, NULL, server_thread, &S) == 0);
    assert(wait_ready(&S, 2000) == 0); // сервер поднялся

    // порт -> строка
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%hu", S.port);

    // клиент
    TCPClient c; TCPClient_Initiate(&c, -1);
    assert(TCPClient_Connect(&c, "127.0.0.1", portstr) == 0);

    const char* msg = "ping\n";
    assert(TCPClient_Write(&c, (const uint8_t*)msg, (int)strlen(msg)) == (int)strlen(msg));

    // читаем неблокирующим recv; даём чуть времени серверу
    uint8_t buf[256]; int total = 0;
    for (int i=0; i<50; ++i) { // до ~500мс
        int n = TCPClient_Read(&c, buf+total, (int)sizeof(buf)-1-total);
        if (n > 0) { total += n; break; }
        usleep(10000); // 10мс
    }
    buf[total] = 0;
    // ожидаем "pong\n"
    assert(total > 0);
    assert(strstr((char*)buf, "pong") != NULL);

    TCPClient_Disconnect(&c);

    pthread_join(th, NULL);
    pthread_mutex_destroy(&S.mtx);
    pthread_cond_destroy(&S.cv);
}

// корневой раннер для main.c
void tcp_client_all(void) {
    RUN_SUB_TEST(tcp_client_ping_pong);
}
