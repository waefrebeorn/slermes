/*
 * process_registry_wait.c — C port of upstream Hermes tools/process_registry_wait.py.
 * PoP: procwait_max_wait_timeout @ tools/process_registry_wait.py:max_wait_timeout
 * PoP: procwait_process_deadline_floor @ tools/process_registry_wait.py:process_deadline_floor
 */
#include "noc_procwait.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Python int(str): optional surrounding whitespace, sign, decimal digits with
 * single underscores between digits. Returns 0 on ValueError/TypeError. */
static int py_int(const char *s, long long *out) {
    if (!s) return 0;
    while (isspace((unsigned char)*s)) s++;
    int sign = 1;
    if (*s == '+' || *s == '-') { if (*s == '-') sign = -1; s++; }
    char buf[256];
    size_t k = 0;
    int have = 0, last_us = 0;
    for (; *s; s++) {
        if (*s >= '0' && *s <= '9') {
            if (k + 1 < sizeof(buf)) buf[k++] = *s;
            have = 1; last_us = 0;
        } else if (*s == '_' && have && !last_us && s[1] >= '0' && s[1] <= '9') {
            last_us = 1;
        } else {
            break;
        }
    }
    while (isspace((unsigned char)*s)) s++;
    if (!have || *s) return 0;
    buf[k] = '\0';
    errno = 0;
    long long v = strtoll(buf, NULL, 10);
    if (errno == ERANGE) return 0;
    *out = sign * v;
    return 1;
}

int procwait_max_wait_timeout(void) {
    long long v;
    if (py_int(getenv("TERMINAL_TIMEOUT"), &v)) return (int)v;
    return 180;
}

/* Python float(str) subset: surrounding whitespace, sign, decimal/exponent,
 * inf/nan, and underscores between digits. Returns 0 on failure. */
static int py_float(const char *s, double *out) {
    if (!s) return 0;
    while (isspace((unsigned char)*s)) s++;
    if (!*s) return 0;
    char buf[512];
    size_t k = 0;
    for (; *s; s++) {
        if (*s == '_') continue;
        if (k + 1 < sizeof(buf)) buf[k++] = *s;
    }
    buf[k] = '\0';
    char *end;
    errno = 0;
    double d = strtod(buf, &end);
    while (isspace((unsigned char)*end)) end++;
    if (end == buf || *end) return 0;
    *out = d;
    return 1;
}

int procwait_process_deadline_floor(const char *action, const char *timeout_str,
                                    int timeout_is_null, double *out) {
    if (!action || strcmp(action, "wait")) return 0; /* non-wait -> None */
    double max_timeout = (double)procwait_max_wait_timeout();
    if (timeout_is_null) { *out = max_timeout; return 1; }

    double requested;
    if (!py_float(timeout_str, &requested)) { *out = max_timeout; return 1; }
    if (!(requested > 0)) return 0; /* <= 0 or NaN -> None */
    *out = (requested < max_timeout) ? requested : max_timeout;
    return 1;
}
