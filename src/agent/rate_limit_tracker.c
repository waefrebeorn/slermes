/*
 * rate_limit_tracker.c — canonical C11 port of agent/rate_limit_tracker.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_rate_limit_tracker_usage_pct @ agent/rate_limit_tracker.py:usage_pct */
int agent_rate_limit_tracker_usage_pct(const char *arg) {
    /* Python: if limit <= 0 -> 0.0; else (used / limit) * 100.0.
     * Arg = "used\tlimit". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char used_s[64], limit_s[64];
    size_t ulen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (ulen >= sizeof(used_s)) ulen = sizeof(used_s) - 1;
    memcpy(used_s, arg, ulen); used_s[ulen] = '\0';
    const char *lim = tab ? tab + 1 : "0";
    if (strlen(lim) >= sizeof(limit_s)) { snprintf(limit_s, sizeof(limit_s), "%.60s", lim); }
    else snprintf(limit_s, sizeof(limit_s), "%s", lim);
    double used = strtod(used_s, NULL);
    double limit = strtod(limit_s, NULL);
    if (limit <= 0) { printf("0\n"); return 0; }
    printf("%g\n", (used / limit) * 100.0);
    return 0;
}

/* PoP: agent_rate_limit_tracker_remaining_seconds_now @ agent/rate_limit_tracker.py:remaining_seconds_now */
int agent_rate_limit_tracker_remaining_seconds_now(const char *arg) {
    /* Python: max(0.0, reset_seconds - (now - captured_at)).
     * Arg = "captured_at\treset_seconds". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char cap_s[64], res_s[64];
    size_t clen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (clen >= sizeof(cap_s)) clen = sizeof(cap_s) - 1;
    memcpy(cap_s, arg, clen); cap_s[clen] = '\0';
    const char *rs = tab ? tab + 1 : "0";
    snprintf(res_s, sizeof(res_s), "%s", rs);
    double captured = strtod(cap_s, NULL);
    double reset = strtod(res_s, NULL);
    double remaining = reset - ((double)time(NULL) - captured);
    if (remaining < 0) remaining = 0;
    printf("%g\n", remaining);
    return 0;
}

/* PoP: agent_rate_limit_tracker_has_data @ agent/rate_limit_tracker.py:has_data */
int agent_rate_limit_tracker_has_data(const char *arg) {
    /* Python: self.captured_at > 0. */
    if (!arg || !*arg) return 0;
    return strtod(arg, NULL) > 0;
}

/* PoP: agent_rate_limit_tracker_age_seconds @ agent/rate_limit_tracker.py:age_seconds */
int agent_rate_limit_tracker_age_seconds(const char *arg) {
    /* Python: if not has_data (captured_at <= 0) -> float("inf");
     * else time.time() - self.captured_at. Arg = "captured_at" epoch. */
    if (!arg || !*arg) { printf("inf\n"); return 0; }
    double captured = strtod(arg, NULL);
    if (captured <= 0) { printf("inf\n"); return 0; }
    printf("%g\n", (double)time(NULL) - captured);
    return 0;
}

/* PoP: agent_rate_limit_tracker_u_safe_float @ agent/rate_limit_tracker.py:_safe_float */
int agent_rate_limit_tracker_u_safe_float(const char *arg) {
    /* Python: float(value) or default. Arg = "value\tdefault". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char val[128];
    size_t vlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (vlen >= sizeof(val)) vlen = sizeof(val) - 1;
    memcpy(val, arg, vlen); val[vlen] = '\0';
    const char *def = tab ? tab + 1 : "0";
    char *end = NULL;
    double d = strtod(val, &end);
    if (end == val || (*end != '\0' && *end != ' ')) { printf("%s\n", def); return 0; }
    printf("%g\n", d);
    return 0;
}

/* PoP: agent_rate_limit_tracker_parse_rate_limit_headers @ agent/rate_limit_tracker.py:parse_rate_limit_headers */
int agent_rate_limit_tracker_parse_rate_limit_headers(const char *arg) {
    /* Python: header parse. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: agent_rate_limit_tracker_u_fmt_count @ agent/rate_limit_tracker.py:_fmt_count */
int agent_rate_limit_tracker_u_fmt_count(const char *arg) {
    /* Python: n>=1e6 -> "%.1fM"; n>=1e3 -> "%.1fK"; else str(n). Arg = n. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    double n = strtod(arg, NULL);
    if (n >= 1000000.0) printf("%.1fM\n", n / 1000000.0);
    else if (n >= 1000.0) printf("%.1fK\n", n / 1000.0);
    else printf("%s\n", arg);
    return 0;
}

/* PoP: agent_rate_limit_tracker_u_fmt_seconds @ agent/rate_limit_tracker.py:_fmt_seconds */
int agent_rate_limit_tracker_u_fmt_seconds(const char *arg) {
    /* Python: 58s / 2m 14s / 58m 57s / 1h 2m. Arg = seconds. */
    if (!arg || !*arg) { printf("0s\n"); return 0; }
    long s = (long)strtod(arg, NULL);
    if (s < 0) s = 0;
    if (s < 60) { printf("%lds\n", s); return 0; }
    if (s < 3600) {
        long m = s / 60, sec = s % 60;
        if (sec) printf("%ldm %lds\n", m, sec);
        else printf("%ldm\n", m);
        return 0;
    }
    long h = s / 3600, m = (s % 3600) / 60;
    if (m) printf("%ldh %ldm\n", h, m);
    else printf("%ldh\n", h);
    return 0;
}

/* PoP: agent_rate_limit_tracker_u_bar @ agent/rate_limit_tracker.py:_bar */
int agent_rate_limit_tracker_u_bar(const char *arg) {
    /* Python: [███░░░] — filled = clamp(pct/100*width), empty = rest.
     * Arg = "pct\twidth". */
    if (!arg || !*arg) { printf("[%s]\n", ""); return 0; }
    double pct = atof(arg);
    const char *tab = strchr(arg, '\t');
    int width = tab ? atoi(tab + 1) : 20;
    if (width < 1) width = 1;
    if (width > 200) width = 200;
    int filled = (int)(pct / 100.0 * width);
    if (filled < 0) filled = 0;
    if (filled > width) filled = width;
    printf("[");
    for (int i = 0; i < filled; i++) fputs("\xE2\x96\x88", stdout);
    for (int i = filled; i < width; i++) fputs("\xE2\x96\x91", stdout);
    printf("]\n");
    return 0;
}

/* PoP: agent_rate_limit_tracker_u_bucket_line @ agent/rate_limit_tracker.py:_bucket_line */
int agent_rate_limit_tracker_u_bucket_line(const char *arg) {
    /* Python: "  label bar pct% used/limit used (remaining left, resets in X)".
     * Arg = "label\tlabel_width\tlimit\tused\tremaining\tpct\treset_s". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    const char *t6 = t5 ? strchr(t5 + 1, '\t') : NULL;
    const char *label = arg;
    long lw = t1 ? strtol(t1 + 1, NULL, 10) : 20;
    long limit = t2 ? strtol(t2 + 1, NULL, 10) : 0;
    if (limit <= 0) {
        printf("  %-*s  (no data)\n", (int)lw, label);
        return 0;
    }
    long used = t3 ? strtol(t3 + 1, NULL, 10) : 0;
    long remaining = t4 ? strtol(t4 + 1, NULL, 10) : 0;
    double pct = t5 ? strtod(t5 + 1, NULL) : 0.0;
    const char *reset = t6 ? t6 + 1 : "0s";
    printf("  %-*s %5.1f%%  %ld/%ld used  (%ld left, resets in %s)\n",
           (int)lw, label, pct, used, limit, remaining, reset);
    return 0;
}

/* PoP: agent_rate_limit_tracker_format_rate_limit_display @ agent/rate_limit_tracker.py:format_rate_limit_display */
int agent_rate_limit_tracker_format_rate_limit_display(const char *arg) {
    /* Python: bucket display. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("No rate limit data yet — make an API request first.\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("No rate limit data yet — make an API request first.\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: agent_rate_limit_tracker_format_rate_limit_compact @ agent/rate_limit_tracker.py:format_rate_limit_compact */
int agent_rate_limit_tracker_format_rate_limit_compact(const char *arg) {
    /* Python: one-line RPM/RPH/TPM/TPH summary. Arg = "state\tparts". */
    if (!arg || !*arg) { printf("No rate limit data.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    if (strcmp(arg, "empty") == 0) { printf("No rate limit data.\n"); return 0; }
    printf("%s\n", t1 ? t1 + 1 : arg);
    return 0;
}
