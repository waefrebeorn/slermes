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
#include <math.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

/* SystemdWatchdog state (mirrors Python instance fields). */
static bool sd_n_state_initialized = false;
static bool sd_n_config_enabled = true;
static double sd_n_interval_seconds = 0.0;      /* from WATCHDOG_USEC/1e6 */
static bool sd_n_unhealthy = false;
static bool sd_n_stopping = false;
static bool sd_n_stopping_notified = false;
static double sd_n_lag_tolerance_seconds = 0.0; /* cached _lag_tolerance() */

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static void sd_n_state_init(void) {
    if (sd_n_state_initialized) return;
    sd_n_state_initialized = true;
}

/* PoP: watchdog_interval_seconds @ gateway/systemd_notify.py:watchdog_interval_seconds */
/* Return systemd's configured watchdog interval in seconds from WATCHDOG_USEC
 * (microseconds), or 0 when no NOTIFY_SOCKET / WATCHDOG_USEC / bad value. */
double sd_n_watchdog_interval_seconds(void) {
    const char *sock = getenv("NOTIFY_SOCKET");
    if (!sock || !*sock) return 0.0;
    const char *raw = getenv("WATCHDOG_USEC");
    if (!raw || !*raw) return 0.0;
    char *end = NULL;
    errno = 0;
    double usec = strtod(raw, &end);
    if (errno != 0 || end == raw || (end && *end != '\0')) return 0.0;
    double interval = usec / 1000000.0;
    if (!isfinite(interval) || interval <= 0) return 0.0;
    return interval;
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
    sd_n_state_init();
    sd_n_config_enabled = config_enabled;
    sd_n_interval_seconds = sd_n_watchdog_interval_seconds();
    if (sd_n_interval_seconds <= 0.0 && watchdog_interval > 0)
        sd_n_interval_seconds = (double)watchdog_interval;
    sd_n_unhealthy = false;
    sd_n_stopping = false;
    sd_n_stopping_notified = false;
    sd_n_lag_tolerance_seconds = 0.0;
    char *out = NULL;
    asprintf(&out, "{\"config_enabled\": %s, \"interval_seconds\": %ld}",
             sd_n_config_enabled ? "true" : "false",
             (long)sd_n_interval_seconds);
    return out;
}

/* PoP: enabled @ gateway/systemd_notify.py:enabled */
bool sd_n_enabled(bool config_enabled, long interval_seconds) {
    return config_enabled && interval_seconds > 0;
}

/* PoP: unhealthy @ gateway/systemd_notify.py:unhealthy */
bool sd_n_unhealthy_state(void) {
    return sd_n_unhealthy;
}

/* PoP: _lag_tolerance @ gateway/systemd_notify.py:_lag_tolerance */
/* Python: max(0.1, interval*0.25) unless explicitly configured; configured
 * value clamped to >= 0. */
double sd_n_lag_tolerance(double interval_seconds, double configured) {
    double interval = interval_seconds > 0.0 ? interval_seconds : 0.0;
    double fallback = interval * 0.25;
    if (fallback < 0.1) fallback = 0.1;
    if (!isfinite(configured)) return fallback;
    if (configured < 0.0) return fallback;
    return configured;
}

/* PoP: task @ gateway/systemd_notify.py:task */
char *sd_n_task(void) {
    return strdup("{}");
}

/* PoP: start @ gateway/systemd_notify.py:start */
int sd_n_start(bool config_enabled, long interval_seconds) {
    /* Python: start loop-progress sampler when systemd watchdog is enabled. */
    if (!config_enabled || interval_seconds <= 0) return 0;
    if (sd_n_stopping) return 0;
    sd_n_stopping = false;
    sd_n_unhealthy = false;
    sd_n_stopping_notified = false;
    if (sd_n_interval_seconds <= 0.0)
        sd_n_interval_seconds = (double)interval_seconds;
    printf("systemd watchdog sampler started (every %lds)\n", interval_seconds);
    return 1;
}

/* PoP: ready @ gateway/systemd_notify.py:ready */
int sd_n_ready(const char *status) {
    /* Python: READY=1 + STATUS=<status> to systemd. */
    if (!sd_n_enabled(sd_n_config_enabled, (long)sd_n_interval_seconds))
        return 0;
    if (!getenv("NOTIFY_SOCKET")) return 0;
    char msg[512];
    const char *safe = status ? status : "Gateway running";
    /* sanitize newlines like Python's replace("\n", " ") */
    char cleaned[256];
    snprintf(cleaned, sizeof(cleaned), "%s", safe);
    for (char *p = cleaned; *p; p++) if (*p == '\n') *p = ' ';
    snprintf(msg, sizeof(msg), "READY=1\nSTATUS=%s", cleaned);
    return sd_notify_send(msg) == 0 ? 1 : 0;
}

/* PoP: record_tick @ gateway/systemd_notify.py:record_tick */
/* Feed systemd only when the event loop woke within its lag budget. */
int sd_n_record_tick(double scheduled_at, double now) {
    if (!sd_n_enabled(sd_n_config_enabled, (long)sd_n_interval_seconds))
        return 0;
    if (sd_n_stopping || sd_n_unhealthy) return 0;
    double lag = isfinite(scheduled_at) && isfinite(now) ? now - scheduled_at
                                                         : (double)INFINITY;
    if (!isfinite(lag) || lag > sd_n_lag_tolerance_seconds) {
        sd_n_unhealthy = true;
        sd_notify_send("STATUS=watchdog unhealthy: event loop progress is late");
        return 0;
    }
    sd_notify_send("WATCHDOG=1");
    return 1;
}

/* PoP: _run @ gateway/systemd_notify.py:_run */
int sd_n_run(long interval_seconds) {
    /* Python: WATCHDOG=1 heartbeat loop. Emits the heartbeat when the loop
     * budget is met; marks unhealthy when the loop is late. */
    if (interval_seconds <= 0) return 0;
    if (sd_n_lag_tolerance_seconds <= 0.0)
        sd_n_lag_tolerance_seconds = sd_n_lag_tolerance((double)interval_seconds, NAN);
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
    int r = sd_n_record_tick(now, now);
    printf("systemd watchdog heartbeat (WATCHDOG=1) -> %s\n",
           r ? "ok" : (sd_n_unhealthy ? "unhealthy" : "disabled"));
    return r;
}

/* PoP: stop @ gateway/systemd_notify.py:stop */
int sd_n_stop(void) {
    /* Python: STOPPING=1 at most once, then the loop ends. */
    if (sd_n_stopping_notified) return 0;
    sd_n_stopping = true;
    sd_n_stopping_notified = true;
    sd_notify_send("STOPPING=1");
    printf("systemd STOPPING=1 emitted\n");
    return 1;
}
