/*
 * port_hermes_cli_timefmt.c — C11 port of hermes_cli/timefmt.py
 *
 * Small shared time-formatting helpers for CLI output.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "port_hermes_cli_timefmt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* PoP: relative_time @ hermes_cli/timefmt.py:relative_time */
char *tf_relative_time(double ts) {
    if (ts == 0) return strdup("?");
    double delta = (double)time(NULL) - ts;
    if (delta < 60) return strdup("just now");
    if (delta < 3600) {
        char *out = NULL;
        asprintf(&out, "%dm ago", (int)(delta / 60));
        return out;
    }
    if (delta < 86400) {
        char *out = NULL;
        asprintf(&out, "%dh ago", (int)(delta / 3600));
        return out;
    }
    if (delta < 172800) return strdup("yesterday");
    if (delta < 604800) {
        char *out = NULL;
        asprintf(&out, "%dd ago", (int)(delta / 86400));
        return out;
    }
    struct tm tm_info;
    localtime_r((time_t *)&ts, &tm_info);
    char *out = malloc(11);
    if (out) strftime(out, 11, "%Y-%m-%d", &tm_info);
    return out;
}
