#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "rtc.h"

const char *request = "GET / HTTP/1.1\r\n"
"Host: 192.168.56.102:8080\r\n"
"User-Agent: curl/7.43.0\r\n"
"Accept: */*\r\n"
"Connection: close\r\n"
"\r\n";
ssize_t request_len;
clock_t shutdown_at;
typedef struct {
    clock_t connect_at;
} status_t;
clock_t *latencies;
long latency_cap;
long latency_len;

static void on_read(rtc_peer_t *peer, char *buf, size_t len) {
    buf[len] = '\0';
    rtc_close(peer);
}

static void on_connect(rtc_peer_t *peer) {
    status_t *status = peer->user;
    if (peer->user == NULL) {
        status = peer->user = malloc(sizeof(status_t));
    }
    status->connect_at = clock();

    int fd = peer->fd;
    ssize_t writed = write(fd, request, request_len);
    if (writed == -1) {
        perror("write");
        return;
    }
    if (writed != request_len) {
        printf("write %ld != %ld\n", writed, request_len);
        rtc_close(peer);
        return;
    }
}

static void on_close(rtc_peer_t *peer) {
    rtc_reconnect(peer);

    clock_t latency = clock() - ((status_t*)peer->user)->connect_at;
    while (latency_len >= latency_cap) {
        latency_cap *= 2;
        latencies = realloc(latencies, sizeof(clock_t) * latency_cap);
    }
    latencies[latency_len] = latency;
    ++latency_len;
}

static void on_timer(rtc_t *rtc) {
    if (clock() >= shutdown_at) {
        rtc_shutdown(rtc);
    }
}

static void clockf(clock_t c, char *buf) {
    clock_t s = c / CLOCKS_PER_SEC;
    clock_t ms = c / (CLOCKS_PER_SEC / 1000);
    if (s == 0) {
        sprintf(buf, "%ldms", ms);
    } else {
        sprintf(buf, "%ld.%ldms", s, ms);
    }
}

#define max(a, b) a > b ? a : b
#define min(a, b) a < b ? a : b

int main() {
    const char *host = "localhost";
    const char *port = "8880";
    int count = 1000;
    int seconds = 60;
    shutdown_at = clock() + seconds * CLOCKS_PER_SEC;
    latency_cap = count * seconds;
    latency_len = 0;
    latencies = malloc(sizeof(clock_t) * latency_cap);

    request_len = strlen(request);

    rtc_t rtc;
    if (rtc_init(&rtc) == -1) {
        return -1;
    }
    rtc.on_connect = on_connect;
    rtc.on_read = on_read;
    rtc.on_close = on_close;
    rtc.on_timer = on_timer;
    int i;
    for(i=0; i<count; ++i) {
        rtc_peer_t *peer = malloc(sizeof(rtc_peer_t));
        memset(peer, 0, sizeof(rtc_peer_t));
        int ret = rtc_connect(&rtc, peer, host, port);
        if (ret != 0) {
            printf("error %d\n", ret);
            return -1;
        }
    }
    if (rtc_loop(&rtc) == -1) {
        return -1;
    }

    clock_t sum_c = 0;
    clock_t min_c = 1000 * CLOCKS_PER_SEC;
    clock_t max_c = 0;
    for(i=0; i<latency_len; ++i) {
        clock_t latency = latencies[i];
        sum_c += latency;
        min_c = min(min_c, latency);
        max_c = max(max_c, latency);
    }
    char min_buf[12];
    char max_buf[12];
    char avg_buf[12];
    clockf(min_c, min_buf),
    clockf(max_c, max_buf),
    clockf(sum_c / latency_len, avg_buf);
    printf("-- Request\n");
    printf("Per/Sec: %ld\n", latency_len / seconds);
    printf("Total  : %ld\n", latency_len);
    printf("Seconds: %d\n", seconds);
    printf("-- Latency\n");
    printf("Min: %s\n", min_buf);
    printf("Avg: %s\n", avg_buf);
    printf("Max: %s\n", max_buf);
    return 0;
}
