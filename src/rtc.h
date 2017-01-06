#ifndef __RTC_H__
#define __RTC_H__

typedef struct {
    int fd;
    int is_reconnect;
    int is_close;
    int is_connect;
    int is_broken_read;
    const char *host;
    const char *port;
    void *user;
} rtc_peer_t;

typedef struct rtc_t {
    int epoll_fd;
    int epoll_size;
    int pending;
    int timer_interval_ms;
    volatile int is_shutdown;
    size_t read_buffer_size;
    void(*on_connect)(rtc_peer_t *peer);
    void(*on_read)(rtc_peer_t *peer, char* buf, size_t len);
    void(*on_close)(rtc_peer_t *peer);
    void(*on_timer)(struct rtc_t *rtc);
} rtc_t;

int rtc_init(rtc_t *rtc);
int rtc_connect(rtc_t *rtc, rtc_peer_t *peer, const char *host, const char *port);
int rtc_loop(rtc_t *rtc);
void rtc_close(rtc_peer_t* peer);
void rtc_shutdown(rtc_t* rtc);
void rtc_reconnect(rtc_peer_t *peer);
#endif
