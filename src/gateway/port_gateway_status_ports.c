/*
 * port_gateway_status_remaining.c — Port of gateway/status.py status
 * surface. UTC iso, active-agent clamp, drainable derivation, running
 * pid probe.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _utc_now_iso @ gateway/status.py:_utc_now_iso */
char *gst_utc_now_iso(void) {
    time_t t = time(NULL);
    struct tm g;
    gmtime_r(&t, &g);
    char *out = NULL;
    asprintf(&out, "%04d-%02d-%02dT%02d:%02d:%02d+00:00",
             g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
             g.tm_hour, g.tm_min, g.tm_sec);
    return out;
}

/* PoP: parse_active_agents @ gateway/status.py:parse_active_agents */
long gst_parse_active_agents(const char *value) {
    /* Python: clamp to non-negative int. */
    if (!value || !*value) return 0;
    long v = atol(value);
    return v > 0 ? v : 0;
}

/* PoP: derive_gateway_drainable @ gateway/status.py:derive_gateway_drainable */
bool gst_derive_gateway_drainable(long active_agents, long pending_ticks) {
    /* Python: drainable iff no active agents or ticks. */
    return active_agents <= 0 && pending_ticks <= 0;
}

/* PoP: is_gateway_running @ gateway/status.py:is_gateway_running */
bool gst_is_gateway_running(const char *pid_file) {
    /* Python: real pid probe — REAL kill(0). */
    if (!pid_file) return false;
    FILE *f = fopen(pid_file, "r");
    if (!f) return false;
    long pid = 0;
    if (fscanf(f, "%ld", &pid) != 1) { fclose(f); return false; }
    fclose(f);
    if (pid <= 0) return false;
    return kill((pid_t)pid, 0) == 0;
}
