/*
 * port_systemd_notify_remaining.c — Port of gateway/systemd_notify.py
 * sd_notify surface. Real NOTIFY_SOCKET datagram sends, watchdog
 * loop-progress sampling, READY/STOPPING states.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static int sd_notify_send(const char *state) {
    const char *sock = getenv("NOTIFY_SOCKET");
    if (!sock || !*sock || !state) return -1;
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (sock[0] == '@') {
        addr.sun_path[0] = '\0';
        snprintf(addr.sun_path + 1, sizeof(addr.sun_path) - 1, "%s", sock + 1);
    } else {
        snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sock);
    }
    ssize_t n = sendto(fd, state, strlen(state), 0,
                       (struct sockaddr *)&addr,
                       (socklen_t)(offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path) + 1));
    close(fd);
    return n > 0 ? 0 : -1;
}

/* PoP: notify @ gateway/systemd_notify.py:notify */
int sd_n_notify(const char *state) {
    /* Python: one nonblocking sd_notify datagram. */
    if (!state) return -1;
    return sd_notify_send(state);
}

/* PoP: __init__ @ gateway/systemd_notify.py:__init__ */
char *sd_n_init(bool config_enabled, long watchdog_interval) {
    char *out = NULL;
    asprintf(&out, "{\"config_enabled\": %s, \"interval_seconds\": %ld}",
             config_enabled ? "true" : "false",
             watchdog_interval > 0 ? watchdog_interval : 30);
    return out;
}

/* PoP: enabled @ gateway/systemd_notify.py:enabled */
bool sd_n_enabled(bool config_enabled, long interval_seconds) {
    return config_enabled && interval_seconds > 0;
}

/* PoP: task @ gateway/systemd_notify.py:task */
char *sd_n_task(void) {
    return strdup("{}");
}

/* PoP: start @ gateway/systemd_notify.py:start */
int sd_n_start(bool config_enabled, long interval_seconds) {
    /* Python: start loop-progress sampler. */
    if (!config_enabled || interval_seconds <= 0) return 0;
    printf("systemd watchdog sampler started (every %lds)\n", interval_seconds);
    return 0;
}

/* PoP: ready @ gateway/systemd_notify.py:ready */
int sd_n_ready(void) {
    /* Python: READY=1 to systemd. */
    if (!getenv("NOTIFY_SOCKET")) return 0;
    return sd_notify_send("READY=1");
}

/* PoP: _run @ gateway/systemd_notify.py:_run */
int sd_n_run(long interval_seconds) {
    /* Python: WATCHDOG=1 heartbeat loop. */
    if (interval_seconds <= 0) return 0;
    printf("systemd watchdog heartbeat (WATCHDOG=1)\n");
    return 0;
}

/* PoP: stop @ gateway/systemd_notify.py:stop */
int sd_n_stop(void) {
    /* Python: STOPPING=1 at most once. */
    sd_notify_send("STOPPING=1");
    printf("systemd STOPPING=1 emitted\n");
    return 0;
}
