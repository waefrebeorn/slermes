#include <math.h>
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
/* PoP: agent_rate_limit_tracker_u_safe_float @ agent/rate_limit_tracker.py:_safe_float */
/* Faithful: try float(value) except -> default. Input "value\tdefault". Parse-ok
 * -> "%.17g" (the twin formats with '.17g' too, so ints/0.0 agree); else the
 * default text. Was: printed %g (0 for 0.0), returned 0, early-returned "" -> "0". */
char *agent_rate_limit_tracker_u_safe_float(const char *arg) {
    const char *a = arg ? arg : "";
    const char *tab = strchr(a, '\t');
    char val[128];
    size_t vlen = tab ? (size_t)(tab - a) : strlen(a);
    if (vlen >= sizeof val) vlen = sizeof val - 1;
    memcpy(val, a, vlen); val[vlen] = '\0';
    const char *def = tab ? tab + 1 : "0.0";
    char *end = NULL;
    double d = strtod(val, &end);
    if (end == val || (*end != '\0' && *end != ' ' && *end != '\t' && *end != '\n' && *end != '\r'))
        return strdup(def);
    char buf[64];
    if (!isfinite(d)) snprintf(buf, sizeof buf, "%s", (d != d) ? "nan" : (d < 0 ? "-inf" : "inf"));
    else if (d == (double)(long long)d && fabs(d) < 1e16) snprintf(buf, sizeof buf, "%lld.0", (long long)d);
    else { snprintf(buf, sizeof buf, "%.15g", d); if (!strpbrk(buf, ".eEnN")) strncat(buf, ".0", sizeof buf - strlen(buf) - 1); }
    return strdup(buf);
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
/* PoP: agent_rate_limit_tracker_u_fmt_count @ agent/rate_limit_tracker.py:_fmt_count */
/* Faithful: n>=1e6 -> "%.1fM"; n>=1e3 -> "%.1fK"; else str(int(n)). Was: echoed
 * the raw arg text for <1000 and printed/returned 0. Returns owned string. */
char *agent_rate_limit_tracker_u_fmt_count(const char *arg) {
    double n = strtod(arg ? arg : "", NULL);
    char buf[64];
    if (n >= 1000000.0) snprintf(buf, sizeof buf, "%.1fM", n / 1000000.0);
    else if (n >= 1000.0) snprintf(buf, sizeof buf, "%.1fK", n / 1000.0);
    else snprintf(buf, sizeof buf, "%ld", (long)n);
    return strdup(buf);
}

/* PoP: agent_rate_limit_tracker_u_fmt_seconds @ agent/rate_limit_tracker.py:_fmt_seconds */
/* PoP: agent_rate_limit_tracker_u_fmt_seconds @ agent/rate_limit_tracker.py:_fmt_seconds */
/* Faithful: s=max(0,int(seconds)); <60 "{s}s"; <3600 "{m}m {sec}s"|"{m}m";
 * else "{h}h {m}m"|"{h}h". Logic was correct but printed/returned 0. Returns string. */
char *agent_rate_limit_tracker_u_fmt_seconds(const char *arg) {
    long sec = (long)strtod(arg ? arg : "", NULL);
    if (sec < 0) sec = 0;
    char buf[64];
    if (sec < 60) snprintf(buf, sizeof buf, "%lds", sec);
    else if (sec < 3600) { long m = sec / 60, r = sec % 60; if (r) snprintf(buf,sizeof buf,"%ldm %lds",m,r); else snprintf(buf,sizeof buf,"%ldm",m); }
    else { long h = sec / 3600, m = (sec % 3600) / 60; if (m) snprintf(buf,sizeof buf,"%ldh %ldm",h,m); else snprintf(buf,sizeof buf,"%ldh",h); }
    return strdup(buf);
}

/* PoP: agent_rate_limit_tracker_u_bar @ agent/rate_limit_tracker.py:_bar */
/* PoP: agent_rate_limit_tracker_u_bar @ agent/rate_limit_tracker.py:_bar */
/* Faithful: filled=max(0,min(width,int(pct/100*width))); "[█*filled + ░*(width-filled)]".
 * INPUT is JSON [pct, width]. Was: "pct\twidth", clamped width<1 to 1 (python
 * allows 0 -> "[]"), printed and returned 0. Returns owned UTF-8 string. */
char *agent_rate_limit_tracker_u_bar(const char *arg) {
    double pct = 0.0; int width = 20;
    json_t *j = arg ? json_parse(arg, NULL) : NULL;
    if (j && json_is_array(j)) {
        if (j->c.count > 0 && json_is_number(j->c.items[0])) pct = j->c.items[0]->num_val;
        if (j->c.count > 1 && json_is_number(j->c.items[1])) width = (int)j->c.items[1]->num_val;
    }
    if (j) json_free(j);
    if (width < 0) width = 0;
    if (width > 1000) width = 1000;
    int filled = (int)(pct / 100.0 * (double)width);
    if (filled < 0) filled = 0;
    if (filled > width) filled = width;
    size_t cap = (size_t)width * 3 + 4;
    char *out = malloc(cap); size_t w = 0;
    out[w++] = '[';
    for (int i = 0; i < filled; i++) { out[w++]=(char)0xE2; out[w++]=(char)0x96; out[w++]=(char)0x88; } /* block */
    for (int i = filled; i < width; i++) { out[w++]=(char)0xE2; out[w++]=(char)0x96; out[w++]=(char)0x91; } /* light shade */
    out[w++] = ']'; out[w] = '\0';
    return out;
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

/* PoP: agent_rate_limit_tracker_u_safe_int @ agent/rate_limit_tracker.py:_safe_int */
/* Faithful: int(float(value)) except (TypeError,ValueError) -> default.
 * Input "value\tdefault"(default 0). Returns python str(int) or the default text. */
char *agent_rate_limit_tracker_u_safe_int(const char *arg) {
    const char *a = arg ? arg : "";
    const char *tab = strchr(a, '\t');
    char val[128];
    size_t vlen = tab ? (size_t)(tab - a) : strlen(a);
    if (vlen >= sizeof val) vlen = sizeof val - 1;
    memcpy(val, a, vlen); val[vlen] = '\0';
    const char *def = tab ? tab + 1 : "0";
    char *end = NULL;
    double d = strtod(val, &end);
    if (end == val || (*end != '\0' && *end != ' ' && *end != '\t' && *end != '\n' && *end != '\r'))
        return strdup(def);
    char buf[64]; snprintf(buf, sizeof buf, "%ld", (long)d);
    return strdup(buf);
}
