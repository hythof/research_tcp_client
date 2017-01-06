#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "rtc.h"

#define UNUSE(X) (void)X

static void flush(rtc_peer_t *peer) {
    UNUSE(peer);
    puts("TODO implements");
}

static void fill(rtc_t *rtc, rtc_peer_t *peer, char *buf, size_t len) {
    while (1) {
        int readed = read(peer->fd, buf, len);
        if (readed == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            } else {
                perror("read");
                return;
            }
        } else if (readed > 0) {
            rtc->on_read(peer, buf, readed);
        } else if (readed == 0) {
            peer->is_broken_read = 1;
            return;
        } else {
            assert(readed >= -1);
            return;
        }
    }
}

static void join(rtc_t *rtc, rtc_peer_t *peer, int fd, int connected) {
   peer->fd = fd;
   peer->is_close = 0;
   peer->is_connect = connected;
   peer->is_broken_read = 0;
   peer->is_reconnect = 0;

   struct epoll_event event;
   event.events = connected ? EPOLLIN | EPOLLET : EPOLLIN | EPOLLOUT | EPOLLET;
   event.data.ptr = peer;
   if (epoll_ctl(rtc->epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
       perror("epoll_ctl");
       free(peer);
       return;
   }
   ++rtc->pending;
}

int rtc_init(rtc_t *rtc) {
    int size = 256;
    int fd = epoll_create(size);
    if (fd == -1) {
        perror("epoll_create");
        return -1;
    }
    rtc->epoll_fd = fd;
    rtc->epoll_size = size;
    rtc->pending = 0;
    rtc->is_shutdown = 0;
    rtc->timer_interval_ms = 1000;
    rtc->read_buffer_size = 8192;
    return 0;
}

int rtc_connect(rtc_t *rtc, rtc_peer_t *peer, const char *host, const char *port) {
   struct addrinfo hints;
   memset(&hints, 0, sizeof(struct addrinfo));
   hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
   hints.ai_socktype = SOCK_STREAM; /* TCP */
   hints.ai_flags = 0;
   hints.ai_protocol = 0;           /* Any protocol */

   struct addrinfo *result;
   int s = getaddrinfo(host, port, &hints, &result);
   if (s != 0) {
       perror(gai_strerror(s));
       return -1;
   }

   struct addrinfo *addr;
   for (addr = result; addr != NULL; addr = addr->ai_next) {
       int fd = socket(addr->ai_family, addr->ai_socktype | SOCK_NONBLOCK, addr->ai_protocol);
       if (fd == -1) {
           continue;
       }

       if (connect(fd, addr->ai_addr, addr->ai_addrlen) == -1) {
           if (errno == EINPROGRESS) {
               peer->host = host;
               peer->port = port;
               join(rtc, peer, fd, 0);
               freeaddrinfo(result);
               return 0;
           }
       } else {
           freeaddrinfo(result);
           join(rtc, peer, fd, 1);
           return 0;
       }
       close(fd);
   }

   freeaddrinfo(result);
   return -1;
}

static void try_reconnect(rtc_t *rtc, rtc_peer_t *peer) {
    if (peer->is_reconnect) {
        if (rtc_connect(rtc, peer, peer->host, peer->port)) {
            puts("fail reconnect");
        }
    }
}

int rtc_loop(rtc_t *rtc) {
    int epoll_fd = rtc->epoll_fd;
    int epoll_size = rtc->epoll_size;
    struct epoll_event events[epoll_size];
    char read_buffer[rtc->read_buffer_size];

    while(!rtc->is_shutdown && rtc->pending) {
        assert(rtc->pending > 0);

        int timeout = rtc->timer_interval_ms;
        int count = epoll_wait(epoll_fd, events, epoll_size, timeout);
        int i;
        for (i=0; i<count; ++i) {
            struct epoll_event *event = &events[i];
            rtc_peer_t *peer = (rtc_peer_t*)event->data.ptr;
            if (event->events & EPOLLOUT) {
                if (peer->is_connect) {
                    flush(peer);
                } else {
                    rtc->on_connect(peer);
                    peer->is_connect = 1;
                    event->events = EPOLLIN;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, peer->fd, event) == -1) {
                        perror("epoll_ctl");
                    }
                }
            }
            if (event->events & EPOLLIN) {
                fill(rtc, peer, read_buffer, rtc->read_buffer_size);
            }
            if (peer->is_close || peer->is_broken_read) {
                rtc->on_close(peer);
                if (peer->is_close || peer->is_broken_read) {
                    if (close(peer->fd) == -1) {
                        perror("close");
                    }
                    --rtc->pending;
                }
            }
            if (peer->is_reconnect) {
                try_reconnect(rtc, peer);
            }
        }
        rtc->on_timer(rtc);
    }
    return 0;
}

void rtc_close(rtc_peer_t* peer) {
   peer->is_close = 1;
}

void rtc_shutdown(rtc_t* rtc) {
   rtc->is_shutdown = 1;
}

void rtc_reconnect(rtc_peer_t *peer) {
    peer->is_reconnect = 1;
}
