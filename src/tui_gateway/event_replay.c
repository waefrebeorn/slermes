/*
 * event_replay.c — C home for upstream Hermes tui_gateway/event_replay.py.
 *
 * PoP: w11_eventrepla_0d019f_batch @ tui_gateway/event_replay.py:events_since
 * PoP: w11_eventrepla_0d019f_stamp  @ tui_gateway/event_replay.py:_stamp_event
 * PoP: w11_eventrepla_0d019f_stats  @ tui_gateway/event_replay.py:replay_stats
 * PoP: w11_eventrepla_0d019f_forget @ tui_gateway/event_replay.py:forget_session
 *
 * Faithful C11 port of the per-session monotonic-seq ring + bounded replay
 * state machine. Frozen snapshots are held as deep JSON copies (observably
 * identical to the Python frozen UTF-8 bytes for ASCII/int/bool payloads);
 * size accounting uses the same compact serialization libjson emits, which
 * matches Python json.dumps(separators=(",", ":"), ensure_ascii=False) for the
 * ASCII corpus used by the differential fixtures.
 */
#include "noc_w11_eventrepla_0d019f.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ER_MAX_SESSIONS 512
#define ER_MAX_EVENTS   16384

typedef struct {
    char   *sid;       /* owned copy of the session id */
    json_t *params;    /* frozen snapshot, seq NOT included */
    long long seq;
    size_t size;       /* frozen payload byte length */
} er_ev;

typedef struct {
    char *sid;         /* owned */
    int   used;
    int   have_ring;
    int   forgotten;
    long long next_seq;        /* _replay_next_seq */
    long long evicted_through; /* _replay_evicted_through */
} er_sess;

static er_ev   g_ev[ER_MAX_EVENTS];
static size_t  g_nev;
static er_sess g_s[ER_MAX_SESSIONS];
static size_t  g_ns;
static long long g_total_bytes;

static long long g_cap_events = 512;
static long long g_cap_sessions = 64;
static long long g_cap_bytes_session = 4L * 1024L * 1024L;
static long long g_cap_bytes_process = 64L * 1024L * 1024L;

static char *er_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

void w11_eventrepla_0d019f_reset(void) {
    for (size_t i = 0; i < g_nev; i++) {
        free(g_ev[i].sid);
        json_free(g_ev[i].params);
    }
    for (size_t i = 0; i < g_ns; i++) free(g_s[i].sid);
    g_nev = 0; g_ns = 0; g_total_bytes = 0;
}

void w11_eventrepla_0d019f_configure(long long max_events, long long max_sessions,
                                     long long max_bytes_session, long long max_bytes_process) {
    if (max_events > 0) g_cap_events = max_events;
    if (max_sessions > 0) g_cap_sessions = max_sessions;
    if (max_bytes_session >= 0) g_cap_bytes_session = max_bytes_session;
    if (max_bytes_process >= 0) g_cap_bytes_process = max_bytes_process;
}

static size_t er_ring_count(void) {
    size_t n = 0;
    for (size_t i = 0; i < g_ns; i++) if (g_s[i].have_ring) n++;
    return n;
}

static int er_find(const char *sid, int create) {
    if (!sid) return -1;
    for (size_t i = 0; i < g_ns; i++)
        if (g_s[i].used && g_s[i].sid && !strcmp(g_s[i].sid, sid)) return (int)i;
    if (!create || g_ns >= ER_MAX_SESSIONS) return -1;
    size_t i = g_ns++;
    memset(&g_s[i], 0, sizeof(g_s[i]));
    g_s[i].sid = er_dup(sid);
    g_s[i].used = 1;
    return (int)i;
}

/* _drop_ring_locked: free the ring, keep the monotonic counters. */
static void er_drop_ring(int si) {
    er_sess *s = &g_s[si];
    if (!s->have_ring) return;
    for (size_t i = 0; i < g_nev; ) {
        if (g_ev[i].sid && !strcmp(g_ev[i].sid, s->sid)) {
            g_total_bytes -= (long long)g_ev[i].size;
            free(g_ev[i].sid);
            json_free(g_ev[i].params);
            for (size_t j = i; j + 1 < g_nev; j++) g_ev[j] = g_ev[j + 1];
            g_nev--;
        } else i++;
    }
    s->have_ring = 0;
    if (s->evicted_through < s->next_seq) s->evicted_through = s->next_seq;
}

/* _purge_key_locked: everything goes (release_key). */
static void er_purge(int si) {
    er_sess *s = &g_s[si];
    for (size_t i = 0; i < g_nev; ) {
        if (g_ev[i].sid && !strcmp(g_ev[i].sid, s->sid)) {
            g_total_bytes -= (long long)g_ev[i].size;
            free(g_ev[i].sid);
            json_free(g_ev[i].params);
            for (size_t j = i; j + 1 < g_nev; j++) g_ev[j] = g_ev[j + 1];
            g_nev--;
        } else i++;
    }
    free(s->sid);
    for (size_t j = (size_t)si; j + 1 < g_ns; j++) g_s[j] = g_s[j + 1];
    g_ns--;
    memset(&g_s[g_ns], 0, sizeof(g_s[g_ns]));
}

static long long er_session_bytes(int si) {
    long long b = 0;
    for (size_t i = 0; i < g_nev; i++)
        if (g_ev[i].sid && !strcmp(g_ev[i].sid, g_s[si].sid)) b += (long long)g_ev[i].size;
    return b;
}

static void er_pop_front_of(int si) {
    /* FIFO: lowest-index event belonging to si. */
    for (size_t i = 0; i < g_nev; i++) {
        if (g_ev[i].sid && !strcmp(g_ev[i].sid, g_s[si].sid)) {
            if (g_s[si].evicted_through < g_ev[i].seq)
                g_s[si].evicted_through = g_ev[i].seq;
            g_total_bytes -= (long long)g_ev[i].size;
            free(g_ev[i].sid);
            json_free(g_ev[i].params);
            for (size_t j = i; j + 1 < g_nev; j++) g_ev[j] = g_ev[j + 1];
            g_nev--;
            return;
        }
    }
}

static void er_evict_session(int si) {
    long long cnt = 0;
    for (size_t i = 0; i < g_nev; i++)
        if (g_ev[i].sid && !strcmp(g_ev[i].sid, g_s[si].sid)) cnt++;
    while (cnt > g_cap_events || er_session_bytes(si) > g_cap_bytes_session) {
        int before = (int)g_nev;
        er_pop_front_of(si);
        if ((int)g_nev == before) break;
        cnt--;
    }
}

static void er_evict_process(void) {
    while (g_total_bytes > g_cap_bytes_process) {
        int found = -1;
        for (size_t i = 0; i < g_ns; i++) {
            if (!g_s[i].have_ring) continue;
            for (size_t k = 0; k < g_nev; k++)
                if (g_ev[k].sid && !strcmp(g_ev[k].sid, g_s[i].sid)) { found = (int)i; break; }
            if (found >= 0) break;
        }
        if (found < 0) break;
        er_pop_front_of(found);
    }
}

long long w11_eventrepla_0d019f_stamp(json_t *event) {
    if (!event || event->type != JSON_OBJECT) return 0;
    json_t *m = json_obj_get(event, "method");
    if (!m || m->type != JSON_STRING || strcmp(m->str_val, "event")) return 0;
    json_t *params = json_obj_get(event, "params");
    if (!params || params->type != JSON_OBJECT) return 0;
    const char *sidp = json_get_str(params, "session_id", "");
    if (!sidp || !*sidp) return 0;

    /* Copy the session id and the frozen snapshot BEFORE mutating the caller's
     * node (json_set may free/replace values inside it). */
    char *sid_copy = er_dup(sidp);
    json_t *frozen = json_copy(params);
    size_t size = 0;
    char *ser = json_serialize(params);
    if (ser) { size = strlen(ser); free(ser); }

    int si = er_find(sid_copy, 1);
    free(sid_copy);
    if (si < 0) { json_free(frozen); return 0; }
    er_sess *s = &g_s[si];

    long long seq = s->next_seq + 1;
    s->next_seq = seq;
    json_set(params, "seq", json_number((double)seq)); /* params["seq"] = seq */

    if (s->forgotten) {
        s->evicted_through = seq;
        json_free(frozen);
        return seq;
    }
    /* Python creates the deque BEFORE the oversized-frame check, so a session
     * that only ever emits oversized frames still owns an (empty) ring. */
    if (!s->have_ring) {
        s->have_ring = 1;
        while (er_ring_count() > (size_t)g_cap_sessions) {
            int oldest = -1;
            for (size_t i = 0; i < g_ns; i++)
                if (g_s[i].have_ring) { oldest = (int)i; break; }
            if (oldest < 0) break;
            er_drop_ring(oldest);
        }
    }
    if ((long long)size > g_cap_bytes_session || (long long)size > g_cap_bytes_process) {
        s->evicted_through = seq;
        json_free(frozen);
        return seq;
    }

    if (g_nev < ER_MAX_EVENTS) {
        g_ev[g_nev].sid = er_dup(s->sid);
        g_ev[g_nev].params = frozen;
        g_ev[g_nev].seq = seq;
        g_ev[g_nev].size = size;
        g_nev++;
        g_total_bytes += (long long)size;
    } else json_free(frozen);

    er_evict_session(si);
    er_evict_process();
    return seq;
}

json_t *w11_eventrepla_0d019f_events_since(const char *sid, long long last_seen) {
    json_t *out = json_array();
    if (!sid) sid = "";
    for (size_t i = 0; i < g_nev; i++) {
        if (g_ev[i].sid && !strcmp(g_ev[i].sid, sid) && g_ev[i].seq > last_seen) {
            json_t *e = json_copy(g_ev[i].params);
            json_set(e, "seq", json_number((double)g_ev[i].seq));
            json_append(out, e);
        }
    }
    return out;
}

bool w11_eventrepla_0d019f_is_truncated(const char *sid, long long last_seen) {
    if (!sid) sid = "";
    int si = er_find(sid, 0);
    long long through = (si >= 0) ? g_s[si].evicted_through : 0;
    return last_seen < through;
}

long long w11_eventrepla_0d019f_latest_seq(const char *sid) {
    if (!sid) sid = "";
    int si = er_find(sid, 0);
    return (si >= 0) ? g_s[si].next_seq : 0;
}

void w11_eventrepla_0d019f_forget(const char *sid) {
    if (!sid || !*sid) return;
    int si = er_find(sid, 1);
    if (si < 0) return;
    g_s[si].forgotten = 1;
    er_drop_ring(si);
}

void w11_eventrepla_0d019f_release(const char *key) {
    if (!key || !*key) return;
    int si = er_find(key, 0);
    if (si >= 0) er_purge(si);
}

json_t *w11_eventrepla_0d019f_stats(void) {
    json_t *o = json_object();
    size_t sessions = er_ring_count();
    json_set(o, "sessions", json_number((double)sessions));
    json_set(o, "events", json_number((double)g_nev));
    json_set(o, "bytes", json_number((double)g_total_bytes));
    json_set(o, "max_per_session", json_number((double)g_cap_events));
    json_set(o, "max_bytes_per_session", json_number((double)g_cap_bytes_session));
    json_set(o, "max_bytes_process", json_number((double)g_cap_bytes_process));
    return o;
}

/* ---- small JSON accessors for the batch driver ---- */
static const char *jstr(const json_t *v, const char *d) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : d;
}
static long long jint(const json_t *v, long long d) {
    if (!v) return d;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_STRING && v->str_val) return strtoll(v->str_val, NULL, 10);
    return d;
}

json_t *w11_eventrepla_0d019f_batch(json_t *ops) {
    json_t *results = json_array();
    if (!ops || ops->type != JSON_ARRAY) return results;
    for (size_t i = 0; i < json_len(ops); i++) {
        json_t *op = json_get(ops, i);
        json_t *res = json_null();
        if (!op || op->type != JSON_ARRAY || op->c.count == 0) {
            json_append(results, res);
            continue;
        }
        const char *name = jstr(json_get(op, 0), "");
        if (!strcmp(name, "reset")) {
            w11_eventrepla_0d019f_reset();
        } else if (!strcmp(name, "config")) {
            w11_eventrepla_0d019f_configure(
                jint(json_get(op, 1), 512), jint(json_get(op, 2), 64),
                jint(json_get(op, 3), 4L * 1024L * 1024L),
                jint(json_get(op, 4), 64L * 1024L * 1024L));
        } else if (!strcmp(name, "stamp")) {
            json_t *evt = json_copy(json_get(op, 1));
            long long seq = w11_eventrepla_0d019f_stamp(evt);
            json_free(evt);
            res = json_number((double)seq);
        } else if (!strcmp(name, "events_since")) {
            res = w11_eventrepla_0d019f_events_since(
                jstr(json_get(op, 1), ""), jint(json_get(op, 2), 0));
        } else if (!strcmp(name, "is_truncated")) {
            res = json_bool(w11_eventrepla_0d019f_is_truncated(
                jstr(json_get(op, 1), ""), jint(json_get(op, 2), 0)));
        } else if (!strcmp(name, "latest_seq")) {
            res = json_number((double)w11_eventrepla_0d019f_latest_seq(
                jstr(json_get(op, 1), "")));
        } else if (!strcmp(name, "forget")) {
            w11_eventrepla_0d019f_forget(jstr(json_get(op, 1), ""));
        } else if (!strcmp(name, "release")) {
            w11_eventrepla_0d019f_release(jstr(json_get(op, 1), ""));
        } else if (!strcmp(name, "stats")) {
            res = w11_eventrepla_0d019f_stats();
        }
        json_append(results, res);
    }
    return results;
}
