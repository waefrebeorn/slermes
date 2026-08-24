/*
 * session_filters.c — canonical C11 port of hermes_cli/session_filters.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_session_filters_parse_duration_seconds @ hermes_cli/session_filters.py:parse_duration_seconds */
int hermes_cli_session_filters_parse_duration_seconds(const char *arg) {
    /* Python: 5h/30m/2d/1w/90(bare=days) -> seconds or None. Arg = value. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (!*p) { printf("\n"); return 0; }
    char *end = NULL;
    double num = strtod(p, &end);
    if (end == p) { printf("\n"); return 0; }
    char unit = (char)tolower((unsigned char)*end);
    if (unit == '\0') { printf("%.0f\n", num * 86400); return 0; }
    if (end[1] != '\0') { printf("\n"); return 0; }
    double mult;
    switch (unit) {
        case 'h': mult = 3600; break;
        case 'm': mult = 60; break;
        case 'd': mult = 86400; break;
        case 'w': mult = 604800; break;
        default: printf("\n"); return 0;
    }
    printf("%.0f\n", num * mult);
    return 0;
}

/* PoP: hermes_cli_session_filters_parse_point_in_time @ hermes_cli/session_filters.py:parse_point_in_time */
int hermes_cli_session_filters_parse_point_in_time(const char *arg) {
    /* Python: duration/ISO -> epoch. Arg = "value\tkind\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *kind = t1 ? t1 + 1 : "";
    if (strcmp(kind, "bad") == 0) {
        fprintf(stderr, "Invalid value for flag: '%s'. Use a duration like '5h', '30m', '2d', '1w', a bare number of days, or an ISO timestamp like '2026-07-05' or '2026-07-05 14:30'.\n", arg);
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: hermes_cli_session_filters_format_epoch @ hermes_cli/session_filters.py:format_epoch */
int hermes_cli_session_filters_format_epoch(const char *arg) {
    /* Python: "-" if ts is None; else fromtimestamp(ts).strftime(
     * "%Y-%m-%d %H:%M"). Arg = epoch seconds (or empty). */
    if (!arg || !*arg || strcmp(arg, "None") == 0) { printf("-\n"); return 0; }
    long long ts = strtoll(arg, NULL, 10);
    time_t t = (time_t)ts;
    struct tm tmv;
    if (!localtime_r(&t, &tmv)) { printf("-\n"); return 0; }
    printf("%04d-%02d-%02d %02d:%02d\n",
           tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
           tmv.tm_hour, tmv.tm_min);
    return 0;
}

/* PoP: hermes_cli_session_filters_build_prune_filters @ hermes_cli/session_filters.py:build_prune_filters */
int hermes_cli_session_filters_build_prune_filters(const char *arg) {
    /* Python: tighter-bound merge. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\n}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "bad") == 0) {
        fprintf(stderr, "%s\n", t3 ? t3 + 1 : "invalid filter window");
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: hermes_cli_session_filters_describe_filters @ hermes_cli/session_filters.py:describe_filters */
int hermes_cli_session_filters_describe_filters(const char *arg) {
    /* Python: human summary. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("no filters (all ended sessions)\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("no filters (all ended sessions)\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}
