/*
 * prune_advisor.c — faithful C11 port of the pure sizing/formatting/advice
 * helpers in upstream Hermes hermes_cli/prune_advisor.py (gate w10_pruneadvis_d472b3).
 *
 * PoP: w10_pruneadvis_d472b3_low            @ hermes_cli/prune_advisor.py:DiskSnapshot.low
 * PoP: w10_pruneadvis_d472b3_retention_days @ hermes_cli/prune_advisor.py:_retention_days
 * PoP: w10_pruneadvis_d472b3_gb             @ hermes_cli/prune_advisor.py:_gb
 * PoP: w10_pruneadvis_d472b3_sessions       @ hermes_cli/prune_advisor.py:_sessions
 * PoP: w10_pruneadvis_d472b3_helps          @ hermes_cli/prune_advisor.py:Advice.helps
 * PoP: w10_pruneadvis_d472b3_advice_lines   @ hermes_cli/prune_advisor.py:advice_lines
 * PoP: w10_pruneadvis_d472b3_notice_payload @ hermes_cli/prune_advisor.py:notice_payload
 *
 * Filesystem/DB/side-effect code is out of scope; these helpers carry the
 * deterministic decision and text logic. 64-bit byte values may be passed as
 * decimal strings (numbers above 2^53 lose precision as JSON doubles).
 */
#include "noc_w10_pruneadvis_d472b3.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#define GIB 1073741824LL
#define LOW_FREE_FLOOR (2 * GIB)
#define LOW_FREE_HEADROOM (20 * GIB)

/* Python int(): number truncates, bool -> 0/1, strict decimal string, else fail. */
static int py_int(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_BOOL)   { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_STRING && v->str_val) {
        const char *s = v->str_val;
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s) return 0;
        char *end = NULL;
        long long x = strtoll(s, &end, 10);
        if (end == s) return 0;
        while (*end && isspace((unsigned char)*end)) end++;
        if (*end) return 0;
        *out = x;
        return 1;
    }
    return 0;
}
static long long i64(const json_t *v, long long def) {
    long long x = 0; return py_int(v, &x) ? x : def;
}

static int disk_low(long long db, long long free, long long total) {
    (void)db;
    if (total <= 0) return 0;
    double free_pct = (double)free * 100.0 / (double)total;
    return free < LOW_FREE_FLOOR || (free_pct < 10.0 && free < LOW_FREE_HEADROOM);
}

json_t *w10_pruneadvis_d472b3_low(const json_t *db, const json_t *free, const json_t *total) {
    return json_bool(disk_low(i64(db,0), i64(free,0), i64(total,0)));
}

json_t *w10_pruneadvis_d472b3_retention_days(const json_t *sessions) {
    long long days = 90;
    if (sessions && sessions->type == JSON_OBJECT) {
        json_t *v = json_obj_get(sessions, "retention_days");
        if (!v || v->type == JSON_NULL) days = 90;
        else if (!py_int(v, &days)) days = 90;
        if (days < 0) days = 90;
    }
    return json_number((double)days);
}

static void fmt_gb(long long n, char *buf, size_t cap) {
    if (n >= GIB) snprintf(buf, cap, "%.1f GB", (double)n / (double)GIB);
    else {
        long long m = n > 0 ? n : 0;
        snprintf(buf, cap, "%.0f MB", (double)m / 1048576.0);
    }
}
json_t *w10_pruneadvis_d472b3_gb(const json_t *n) {
    char buf[64]; fmt_gb(i64(n,0), buf, sizeof(buf)); return json_string(buf);
}
json_t *w10_pruneadvis_d472b3_sessions(const json_t *n) {
    long long c = i64(n,0);
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld session%s", c, c == 1 ? "" : "s");
    return json_string(buf);
}

static int advice_helps(long long db, long long free, long long total,
                        long long reclaim, long long prunable) {
    if (prunable == 0) return 0;
    return reclaim >= GIB || !disk_low(db - reclaim, free + reclaim, total);
}
json_t *w10_pruneadvis_d472b3_helps(const json_t *db, const json_t *free,
                                    const json_t *total, const json_t *reclaim,
                                    const json_t *prunable) {
    return json_bool(advice_helps(i64(db,0), i64(free,0), i64(total,0), i64(reclaim,0), i64(prunable,0)));
}

/* Pull the scalar fields out of an Advice-shaped JSON object. */
typedef struct { long long db, free, total, retention, prunable, total_s, reclaim; int estimate_ok; } Advice;
static Advice read_advice(const json_t *a) {
    Advice x = {0};
    if (!a || a->type != JSON_OBJECT) return x;
    json_t *d = json_obj_get(a, "disk");
    x.db = i64(d ? json_obj_get(d, "db_bytes") : NULL, 0);
    x.free = i64(d ? json_obj_get(d, "free_bytes") : NULL, 0);
    x.total = i64(d ? json_obj_get(d, "total_bytes") : NULL, 0);
    x.retention = i64(json_obj_get(a, "retention_days"), 0);
    x.prunable = i64(json_obj_get(a, "prunable_sessions"), 0);
    x.total_s = i64(json_obj_get(a, "total_sessions"), 0);
    x.reclaim = i64(json_obj_get(a, "reclaim_bytes"), 0);
    json_t *e = json_obj_get(a, "estimate_ok");
    x.estimate_ok = (e && e->type == JSON_BOOL) ? e->bool_val : 1;
    return x;
}
static void push_line(json_t *arr, const char *fmt, ...) {
    char buf[2048]; va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    json_append(arr, json_string(buf));
}

static json_t *lines_for(const Advice *x, const char *p) {
    json_t *arr = json_array();
    char gfree[64], gtotal[64], gdb[64], greclaim[64];
    fmt_gb(x->free, gfree, sizeof(gfree));
    fmt_gb(x->total, gtotal, sizeof(gtotal));
    fmt_gb(x->db, gdb, sizeof(gdb));
    fmt_gb(x->reclaim, greclaim, sizeof(greclaim));
    push_line(arr, "Low disk space: %s free of %s. Your session history (state.db) uses %s.",
              gfree, gtotal, gdb);
    push_line(arr, "Auto-prune is off. Turning it on deletes ended sessions inactive for more than "
                   "%lld days, once a day (pinned and open sessions are kept).", x->retention);
    if (!x->estimate_ok) {
        push_line(arr, "Could not read state.db to estimate how much pruning would free.");
    } else if (x->prunable == 0) {
        if (x->total_s) {
            char sbuf[64]; snprintf(sbuf, sizeof(sbuf), "%lld session%s", x->total_s, x->total_s == 1 ? "" : "s");
            push_line(arr, "Pruning will not help right now: none of your %s is older than %lld days. "
                           "Something else is filling this disk.", sbuf, x->retention);
        } else {
            push_line(arr, "Pruning will not help right now: there is no session history to delete. "
                           "Something else is filling this disk.");
        }
    } else if (!advice_helps(x->db, x->free, x->total, x->reclaim, x->prunable)) {
        char pbuf[64]; snprintf(pbuf, sizeof(pbuf), "%lld session%s", x->prunable, x->prunable == 1 ? "" : "s");
        push_line(arr, "Pruning would remove only about %s of history (%s), which will not fix this "
                       "disk on its own.", greclaim, pbuf);
    } else {
        char tbuf[64]; snprintf(tbuf, sizeof(tbuf), "%lld session%s", x->total_s, x->total_s == 1 ? "" : "s");
        push_line(arr, "Pruning would remove about %s of history (%lld of %s).", greclaim, x->prunable, tbuf);
    }
    if (x->estimate_ok && x->prunable)
        push_line(arr, "Deleted history frees space inside state.db; to give it back to the disk, run "
                       "`hermes sessions optimize` with other Hermes windows closed.");
    push_line(arr, "%sautoprune on to enable, %sautoprune off to keep everything (never asked again), "
                   "%sautoprune later to be reminded in a week.", p, p, p);
    return arr;
}

json_t *w10_pruneadvis_d472b3_advice_lines(const json_t *advice, const char *command_prefix) {
    Advice x = read_advice(advice);
    return lines_for(&x, (command_prefix && *command_prefix) ? command_prefix : "/");
}

json_t *w10_pruneadvis_d472b3_notice_payload(const json_t *advice) {
    Advice x = read_advice(advice);
    char gfree[64]; fmt_gb(x.free, gfree, sizeof(gfree));
    json_t *lines = lines_for(&x, "/");
    /* Join the lines with "\n" for the detail field. */
    char detail[8192]; detail[0] = '\0';
    size_t used = 0;
    for (size_t i = 0; i < lines->c.count; i++) {
        json_t *ln = lines->c.items[i];
        const char *s = (ln && ln->type == JSON_STRING) ? ln->str_val : "";
        size_t sl = strlen(s);
        if (used + sl + 2 < sizeof(detail)) {
            if (used) { detail[used++] = '\n'; }
            memcpy(detail + used, s, sl); used += sl; detail[used] = '\0';
        }
    }
    json_t *o = json_object();
    char text[256];
    snprintf(text, sizeof(text), "\xe2\x9a\xa0 Low disk (%s free) \xc2\xb7 /autoprune on|off|later", gfree);
    json_set(o, "text", json_string(text));
    json_set(o, "detail", json_string(detail));
    json_set(o, "level", json_string("warn"));
    json_set(o, "kind", json_string("sticky"));
    json_set(o, "ttl_ms", json_null());
    json_set(o, "key", json_string("sessions.auto_prune"));
    json_set(o, "id", json_string("sessions.auto_prune"));
    json_free(lines);
    return o;
}
