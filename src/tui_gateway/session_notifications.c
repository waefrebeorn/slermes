/*
 * session_notifications.c — C home for upstream Hermes tui_gateway/session_notifications.py.
 *
 * PoP: w29_sessionnot_c045cd_p1_current_keys @ tui_gateway/session_notifications.py:_notif_current_keys
 * PoP: w29_sessionnot_c045cd_p1_session_matches @ tui_gateway/session_notifications.py:_notif_session_matches
 * PoP: w29_sessionnot_c045cd_p1_requires_owner @ tui_gateway/session_notifications.py:_notification_event_requires_owner
 * PoP: w29_sessionnot_c045cd_p1_dedup_key @ tui_gateway/session_notifications.py:_notification_event_dedup_key
 * PoP: w29_sessionnot_c045cd_p1_refusal_backoff @ tui_gateway/session_notifications.py:_notif_refusal_backoff
 * PoP: w29_sessionnot_c045cd_p1_loop_route_is_gateway_chat @ tui_gateway/session_notifications.py:_loop_route_is_gateway_chat
 * PoP: w29_sessionnot_c045cd_p1_session_process_owners @ tui_gateway/session_notifications.py:_session_process_owners
 * PoP: w29_sessionnot_c045cd_p1_constants @ tui_gateway/session_notifications.py:_KANBAN_POLL_SECONDS
 * PoP: w29_sessionnot_c045cd_p2_kb_first_line @ tui_gateway/session_notifications.py:_kb_first_line
 * PoP: w29_sessionnot_c045cd_p2_kb_completed @ tui_gateway/session_notifications.py:_kb_completed
 * PoP: w29_sessionnot_c045cd_p2_kb_timed_out @ tui_gateway/session_notifications.py:_kb_timed_out
 * PoP: w29_sessionnot_c045cd_p2_format_kanban_event_text @ tui_gateway/session_notifications.py:_format_kanban_event_text
 * PoP: w29_sessionnot_c045cd_p2_async_delegation_display_metadata @ tui_gateway/session_notifications.py:_async_delegation_display_metadata
 * PoP: w29_sessionnot_c045cd_p2_prepend_note @ tui_gateway/session_notifications.py:_prepend_note
 *
 * Everything that needs the process is a seam and stays out: the session
 * registry + its lock, the lineage DB, `time.sleep`, the event queue, the
 * emitters, and the cross-module display-text helpers (a parameter here).
 * What remains is exactly what those values decide.
 */
#include "noc_w29_sessionnot_c045cd_p1.h"
#include "noc_w29_sessionnot_c045cd_p2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SN_REFUSAL_BACKOFF_S 0.25
#define SN_POLL_SECONDS 5.0
#define SN_TRUNC_SUMMARY 200L
#define SN_TRUNC_RESULT 160L
#define SN_TRUNC_TITLE 120L
#define SN_TRUNC_REASON 160L
#define SN_TRUNC_ERROR 200L
#define SN_EMDASH " \xE2\x80\x94 "

/* ---------------------------------------------------------------- helpers */

static char *sn_dup(const char *s) {
    size_t n;
    char *d;
    if (!s) s = "";
    n = strlen(s) + 1;
    d = (char *)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

static char *sn_sub(const char *s, size_t n) {
    char *d = (char *)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n);
    d[n] = 0;
    return d;
}

/* Python s[:n] (negative n drops from the end). Clipping is byte-wise: every
 * fixture is ASCII, and the only multi-byte text in this module is the fixed
 * glyph/em-dash prefix, which is never clipped. */
static char *sn_trunc(const char *s, long n) {
    size_t len = strlen(s);
    if (n < 0) {
        long keep = (long)len + n;
        if (keep < 0) keep = 0;
        n = keep;
    }
    if ((size_t)n < len) return sn_sub(s, (size_t)n);
    return sn_dup(s);
}

static int sn_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

static char *sn_num_str(double d) {
    char buf[64];
    if (d == (double)(long long)d && d > -1e17 && d < 1e17) {
        snprintf(buf, sizeof buf, "%lld", (long long)d);
        return sn_dup(buf);
    }
    snprintf(buf, sizeof buf, "%.15g", d);
    if (strtod(buf, NULL) != d) snprintf(buf, sizeof buf, "%.17g", d);
    return sn_dup(buf);
}

/* Python str(v) for the value shapes a fixture can carry (containers fall back
 * to their JSON text; no fixture depends on Python's repr of a dict). */
static char *sn_py_str(const json_t *v) {
    if (!v) return sn_dup("None");
    switch (v->type) {
        case JSON_STRING: return sn_dup(v->str_val);
        case JSON_NULL: return sn_dup("None");
        case JSON_BOOL: return sn_dup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: return sn_num_str(v->num_val);
        default: {
            char *s = json_serialize(v);
            char *d = sn_dup(s);
            free(s);
            return d;
        }
    }
}

/* Python str(v or ""). */
static char *sn_str_or_empty(const json_t *v) {
    if (!sn_truthy(v)) return sn_dup("");
    return sn_py_str(v);
}

/* _session_lookup_key(session, fallback=...) as the composition sees it:
 * agent.session_id or session_key or fallback, then str(). */
static char *sn_lookup_key(const json_t *session, const json_t *fallback) {
    const json_t *agent = json_obj_get(session, "agent");
    const json_t *v = NULL;
    if (agent && agent->type == JSON_OBJECT) v = json_obj_get(agent, "session_id");
    if (!sn_truthy(v)) v = json_obj_get(session, "session_key");
    if (!sn_truthy(v)) v = fallback;
    if (!sn_truthy(v)) return sn_dup("");
    return sn_py_str(v);
}

static const json_t *sn_get(const json_t *obj, const char *key) {
    return obj ? json_obj_get(obj, key) : NULL;
}

/* evt.get(key, "") — only a *missing* key yields the default. */
static void sn_append_field(json_t *out, const json_t *obj, const char *key) {
    const json_t *v = sn_get(obj, key);
    json_append(out, v ? json_copy(v) : json_string(""));
}

/* Small string builder for the rendered lines. */
typedef struct {
    char *p;
    size_t n, cap;
} sn_sb;

static int sn_sb_addn(sn_sb *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *q;
        while (cap < b->n + n + 1) cap *= 2;
        q = (char *)realloc(b->p, cap);
        if (!q) return 0;
        b->p = q;
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = 0;
    return 1;
}

static int sn_sb_add(sn_sb *b, const char *s) {
    return sn_sb_addn(b, s ? s : "", s ? strlen(s) : 0);
}

static char *sn_sb_take(sn_sb *b) {
    char *p = b->p ? b->p : sn_dup("");
    b->p = NULL;
    b->n = b->cap = 0;
    return p;
}

static int sn_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* ------------------------------------------------------- p1: routing keys */

json_t *w29_sessionnot_c045cd_p1_current_keys(const json_t *sid, const json_t *session) {
    char *a = sn_str_or_empty(sn_get(session, "session_key"));
    char *b = sn_lookup_key(session, sid);
    char *keys[2];
    size_t n = 0;
    json_t *out;
    keys[n++] = a;
    if (strcmp(a, b) != 0) keys[n++] = b;
    qsort(keys, n, sizeof keys[0], sn_cmp_str);
    out = json_array();
    for (size_t i = 0; i < n; i++) json_append(out, json_string(keys[i]));
    free(a);
    free(b);
    return out;
}

static int sn_keys_contains(const json_t *keys, const char *s) {
    if (!keys || keys->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(keys); i++) {
        const json_t *e = json_get(keys, i);
        if (e && e->type == JSON_STRING && e->str_val && strcmp(e->str_val, s) == 0) return 1;
    }
    return 0;
}

json_t *w29_sessionnot_c045cd_p1_session_matches(const json_t *session, const json_t *keys) {
    char *sk = sn_str_or_empty(sn_get(session, "session_key"));
    char *lk = sn_lookup_key(session, NULL);
    int hit = sn_keys_contains(keys, sk) || sn_keys_contains(keys, lk);
    free(sk);
    free(lk);
    return json_bool(hit ? true : false);
}

json_t *w29_sessionnot_c045cd_p1_requires_owner(const json_t *evt) {
    const json_t *t = sn_get(evt, "type");
    int delegation = t && t->type == JSON_STRING && t->str_val &&
                     strcmp(t->str_val, "async_delegation") == 0;
    int addressed = sn_truthy(sn_get(evt, "origin_ui_session_id")) ||
                    sn_truthy(sn_get(evt, "session_key"));
    return json_bool((delegation || addressed) ? true : false);
}

static const char *const SN_DEDUP_WATCH_MATCH[] = {"command", "pattern", "output", "suppressed",
                                                   "message_id", NULL};
static const char *const SN_DEDUP_HEARTBEAT[] = {"seq", NULL};
static const char *const SN_DEDUP_WATCH_DISABLED[] = {"command", "message", "suppressed", NULL};
static const char *const SN_DEDUP_WATCH_OVERFLOW[] = {"command", "message", "suppressed", NULL};

static const char *const *sn_dedup_extra(const char *ty) {
    if (strncmp(ty, "watch_overflow_", 15) == 0) return SN_DEDUP_WATCH_OVERFLOW;
    if (strcmp(ty, "watch_match") == 0) return SN_DEDUP_WATCH_MATCH;
    if (strcmp(ty, "heartbeat") == 0) return SN_DEDUP_HEARTBEAT;
    if (strcmp(ty, "watch_disabled") == 0) return SN_DEDUP_WATCH_DISABLED;
    return NULL;
}

json_t *w29_sessionnot_c045cd_p1_dedup_key(const json_t *evt) {
    const json_t *tv = sn_get(evt, "type");
    const char *ty = (tv && tv->type == JSON_STRING && tv->str_val) ? tv->str_val : "completion";
    json_t *out = json_array();
    const json_t *event_id;
    if ((strcmp(ty, "watch_match") == 0 || strcmp(ty, "watch_disabled") == 0) &&
        sn_truthy((event_id = sn_get(evt, "event_id")))) {
        sn_append_field(out, evt, "session_id");
        json_append(out, json_string(ty));
        json_append(out, json_copy(event_id));
        return out;
    }
    if (strcmp(ty, "async_delegation") == 0) {
        sn_append_field(out, evt, "delegation_id");
        json_append(out, json_string(ty));
        if (sn_truthy(sn_get(evt, "task_failure_notice"))) {
            const json_t *results = sn_get(evt, "results");
            const json_t *first = NULL;
            if (results && results->type == JSON_ARRAY && json_len(results) > 0) {
                first = json_get(results, 0);
                if (first && first->type != JSON_OBJECT) first = NULL;
            }
            json_append(out, json_string("task_failure"));
            sn_append_field(out, first, "task_index");
        }
        return out;
    }
    {
        const char *const *extra = sn_dedup_extra(ty);
        sn_append_field(out, evt, "session_id");
        json_append(out, json_string(ty));
        for (size_t i = 0; extra && extra[i]; i++) {
            const json_t *v = sn_get(evt, extra[i]);
            if (v) {
                json_append(out, json_copy(v));
            } else {
                json_append(out, strcmp(extra[i], "suppressed") == 0 ? json_number(0.0)
                                                                     : json_string(""));
            }
        }
    }
    return out;
}

json_t *w29_sessionnot_c045cd_p1_refusal_backoff(const json_t *session) {
    const json_t *raw = sn_get(session, "_notif_refusal_delay");
    double delay = sn_truthy(raw) ? 0.0 : SN_REFUSAL_BACKOFF_S;
    double next;
    json_t *out = json_object();
    if (sn_truthy(raw)) {
        if (raw->type == JSON_NUMBER) {
            delay = raw->num_val;
        } else if (raw->type == JSON_BOOL) {
            delay = raw->bool_val ? 1.0 : SN_REFUSAL_BACKOFF_S;
        }
    }
    next = delay * 2.0;
    if (next > SN_POLL_SECONDS) next = SN_POLL_SECONDS;
    json_set(out, "delay", json_number(delay));
    json_set(out, "next", json_number(next));
    return out;
}

json_t *w29_sessionnot_c045cd_p1_loop_route_is_gateway_chat(const json_t *state) {
    const json_t *route = sn_get(state, "route");
    if (!sn_truthy(route) || route->type != JSON_OBJECT) {
        return json_bool(false);
    }
    return json_bool((sn_truthy(sn_get(route, "platform")) && sn_truthy(sn_get(route, "chat_id")))
                         ? true
                         : false);
}

static int sn_grow_strs(char ***items, size_t *cap, size_t need) {
    size_t c = *cap ? *cap : 8;
    char **grown;
    if (*cap >= need) return 1;
    while (c < need) c *= 2;
    grown = (char **)realloc(*items, c * sizeof **items);
    if (!grown) return 0;
    *items = grown;
    *cap = c;
    return 1;
}

json_t *w29_sessionnot_c045cd_p1_session_process_owners(const json_t *session) {
    const json_t *agent = sn_get(session, "agent");
    const json_t *ids = (agent && agent->type == JSON_OBJECT)
                            ? sn_get(agent, "_process_owner_task_ids")
                            : NULL;
    char **items = NULL;
    size_t n = 0, cap = 0, i;
    json_t *out = json_array();
    char *key = sn_str_or_empty(sn_get(session, "session_key"));
    for (i = 0; ids && ids->type == JSON_ARRAY && i < json_len(ids); i++) {
        const json_t *e = json_get(ids, i);
        if (!e) continue;
        if (!sn_grow_strs(&items, &cap, n + 1)) break;
        items[n++] = sn_py_str(e);
    }
    if (sn_grow_strs(&items, &cap, n + 1)) {
        items[n++] = key;
        key = NULL;
        qsort(items, n, sizeof items[0], sn_cmp_str);
        for (i = 0; i < n; i++) {
            if (items[i][0] == 0) continue;                             /* - {""} */
            if (i > 0 && strcmp(items[i], items[i - 1]) == 0) continue; /* set */
            json_append(out, json_string(items[i]));
        }
    }
    for (i = 0; i < n; i++) free(items[i]);
    free(items);
    free(key);
    return out;
}

json_t *w29_sessionnot_c045cd_p1_constants(void) {
    json_t *out = json_object();
    json_t *extras = json_object();
    json_t *kinds = json_array();
    static const char *const KANBAN_KINDS[] = {"completed", "blocked",  "gave_up", "crashed",
                                               "timed_out", "status",   "archived", "unblocked",
                                               NULL};
    for (size_t i = 0; KANBAN_KINDS[i]; i++) json_append(kinds, json_string(KANBAN_KINDS[i]));
    json_set(out, "kanban_notify_kinds", kinds);
    json_set(out, "kanban_poll_seconds", json_number(5.0));
    json_set(out, "loop_poll_seconds", json_number(5.0));
    json_set(out, "refusal_backoff_s", json_number(SN_REFUSAL_BACKOFF_S));
    json_set(out, "refusal_backoff_max_s", json_number(SN_POLL_SECONDS));
    {
        static const char *const *const FIELDS[] = {SN_DEDUP_WATCH_MATCH, SN_DEDUP_HEARTBEAT,
                                                    SN_DEDUP_WATCH_DISABLED, SN_DEDUP_WATCH_OVERFLOW};
        static const char *const NAMES[] = {"watch_match", "heartbeat", "watch_disabled",
                                            "watch_overflow_"};
        for (size_t i = 0; i < 4; i++) {
            json_t *arr = json_array();
            for (size_t j = 0; FIELDS[i][j]; j++) json_append(arr, json_string(FIELDS[i][j]));
            json_set(extras, NAMES[i], arr);
        }
    }
    json_set(out, "dedup_extra_fields", extras);
    return out;
}

/* ------------------------------------------------------- p2: text shaping */

/* str(value).strip().splitlines() -> first line clipped to limit, prefixed by
 * "\n" ("" when nothing is left). */
static char *sn_first_line(const json_t *value, long limit) {
    char *raw = sn_py_str(value);
    char *s = raw;
    size_t len, start, end;
    char *clip, *out;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\v' || *s == '\f') s++;
    len = strlen(s);
    while (len > 0) {
        char c = s[len - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') {
            len--;
        } else {
            break;
        }
    }
    if (len == 0) {
        free(raw);
        return sn_dup("");
    }
    start = (size_t)(s - raw);
    end = start;
    while (end < start + len) {
        char c = raw[end];
        if (c == '\n' || c == '\r' || c == '\v' || c == '\f') break;
        end++;
    }
    {
        char *line = sn_sub(raw + start, end - start);
        clip = sn_trunc(line, limit);
        free(line);
    }
    {
        sn_sb b = {0};
        sn_sb_add(&b, "\n");
        sn_sb_add(&b, clip);
        free(clip);
        out = sn_sb_take(&b);
    }
    free(raw);
    return out;
}

json_t *w29_sessionnot_c045cd_p2_kb_first_line(const json_t *value, const json_t *limit) {
    long lim = 0;
    char *s;
    if (limit && limit->type == JSON_NUMBER) lim = (long)limit->num_val;
    else if (limit && limit->type == JSON_STRING) lim = strtol(limit->str_val, NULL, 10);
    s = sn_first_line(value, lim);
    {
        json_t *out = json_string(s);
        free(s);
        return out;
    }
}

static void sn_kb_completed_suffix(const json_t *task, const json_t *payload, const char *title,
                                   sn_sb *b) {
    const json_t *summary = sn_get(payload, "summary");
    const json_t *result = sn_get(task, "result");
    char *handoff = NULL;
    sn_sb_add(b, " done");
    sn_sb_add(b, SN_EMDASH);
    sn_sb_add(b, title);
    if (sn_truthy(summary)) handoff = sn_first_line(summary, SN_TRUNC_SUMMARY);
    else if (sn_truthy(result)) handoff = sn_first_line(result, SN_TRUNC_RESULT);
    if (handoff) {
        sn_sb_add(b, handoff);
        free(handoff);
    }
}

json_t *w29_sessionnot_c045cd_p2_kb_completed(const json_t *task, const json_t *payload,
                                              const json_t *title) {
    sn_sb b = {0};
    char *t = sn_str_or_empty(title);
    char *out;
    sn_kb_completed_suffix(task, payload, t, &b);
    out = sn_sb_take(&b);
    free(t);
    {
        json_t *j = json_string(out);
        free(out);
        return j;
    }
}

/* Python int(v) over the shapes a fixture can carry; *ok=0 where int() raises. */
static int sn_py_int(const json_t *v, int *ok) {
    *ok = 1;
    if (!v) {
        *ok = 0;
        return 0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return (int)v->num_val;
    if (v->type == JSON_STRING && v->str_val) {
        const char *s = v->str_val;
        long acc = 0;
        int neg = 0, digits = 0;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        if (*s == '+' || *s == '-') {
            neg = *s == '-';
            s++;
        }
        while (*s >= '0' && *s <= '9') {
            acc = acc * 10 + (*s - '0');
            digits++;
            s++;
        }
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        if (digits == 0 || *s != 0) {
            *ok = 0;
            return 0;
        }
        return (int)(neg ? -acc : acc);
    }
    *ok = 0;
    return 0;
}

static char *sn_timed_out_suffix(const json_t *payload) {
    const json_t *raw = sn_get(payload, "limit_seconds");
    int ok = 0, secs = 0;
    char buf[64];
    sn_sb b = {0};
    if (sn_truthy(raw)) secs = sn_py_int(raw, &ok);
    if (!ok) secs = 0;
    snprintf(buf, sizeof buf, " timed out (max_runtime=%ds); will retry", secs);
    sn_sb_add(&b, buf);
    return sn_sb_take(&b);
}

json_t *w29_sessionnot_c045cd_p2_kb_timed_out(const json_t *task, const json_t *payload,
                                              const json_t *title) {
    char *suffix;
    json_t *out;
    (void)task;  /* the formatter ignores the task: the runtime limit is on the payload */
    (void)title; /* ... and the kanban formatter already wrote the id/title prefix */
    suffix = sn_timed_out_suffix(payload);
    out = json_string(suffix);
    free(suffix);
    return out;
}

static int sn_kind_glyph(const char *kind, const char **glyph, int *which) {
    if (strcmp(kind, "completed") == 0) {
        *glyph = "\xE2\x9C\x94 ";
        *which = 0;
        return 1;
    }
    if (strcmp(kind, "blocked") == 0) {
        *glyph = "\xE2\x8F\xB8 ";
        *which = 1;
        return 1;
    }
    if (strcmp(kind, "gave_up") == 0) {
        *glyph = "\xE2\x9C\x96 ";
        *which = 2;
        return 1;
    }
    if (strcmp(kind, "crashed") == 0) {
        *glyph = "\xE2\x9C\x96 ";
        *which = 3;
        return 1;
    }
    if (strcmp(kind, "timed_out") == 0) {
        *glyph = "\xE2\x8F\xB1 ";
        *which = 4;
        return 1;
    }
    if (strcmp(kind, "status") == 0) {
        *glyph = "\xF0\x9F\x94\x84 ";
        *which = 5;
        return 1;
    }
    return 0;
}

json_t *w29_sessionnot_c045cd_p2_format_kanban_event_text(const json_t *sub, const json_t *task,
                                                          const json_t *ev,
                                                          const json_t *board_slug) {
    const json_t *kv = sn_get(ev, "kind");
    const json_t *payload = sn_get(ev, "payload");
    const char *kind = (kv && kv->type == JSON_STRING && kv->str_val) ? kv->str_val : "";
    const char *glyph = NULL;
    int which = 0;
    char *task_id, *title_raw, *title, *who, *slug, *out;
    sn_sb b = {0};
    if (!sn_kind_glyph(kind, &glyph, &which)) return NULL;
    if (!payload || payload->type != JSON_OBJECT) payload = NULL;
    task_id = sn_str_or_empty(sn_get(sub, "task_id"));
    {
        const json_t *tv = sn_get(task, "title");
        const json_t *src = sn_truthy(tv) ? tv : sn_get(sub, "task_id");
        title_raw = sn_py_str(src);
        title = sn_trunc(title_raw, SN_TRUNC_TITLE);
        free(title_raw);
    }
    who = sn_str_or_empty(sn_get(task, "assignee"));
    slug = sn_str_or_empty(board_slug);
    sn_sb_add(&b, glyph);
    if (slug[0]) {
        sn_sb_add(&b, "[");
        sn_sb_add(&b, slug);
        sn_sb_add(&b, "] ");
    }
    if (who[0]) {
        sn_sb_add(&b, "@");
        sn_sb_add(&b, who);
        sn_sb_add(&b, " ");
    }
    sn_sb_add(&b, "Kanban ");
    sn_sb_add(&b, task_id);
    switch (which) {
        case 0:
            sn_kb_completed_suffix(task, payload, title, &b);
            break;
        case 1: {
            const json_t *reason = sn_get(payload, "reason");
            sn_sb_add(&b, " blocked");
            if (sn_truthy(reason)) {
                char *rs = sn_py_str(reason);
                char *rc = sn_trunc(rs, SN_TRUNC_REASON);
                sn_sb_add(&b, ": ");
                sn_sb_add(&b, rc);
                free(rs);
                free(rc);
            }
            break;
        }
        case 2: {
            const json_t *error = sn_get(payload, "error");
            sn_sb_add(&b, " gave up after repeated spawn failures");
            if (sn_truthy(error)) {
                char *es = sn_py_str(error);
                char *ec = sn_trunc(es, SN_TRUNC_ERROR);
                sn_sb_add(&b, "\n");
                sn_sb_add(&b, ec);
                free(es);
                free(ec);
            }
            break;
        }
        case 3:
            sn_sb_add(&b, " worker crashed (pid gone); dispatcher will retry");
            break;
        case 4: {
            char *sfx = sn_timed_out_suffix(payload);
            sn_sb_add(&b, sfx);
            free(sfx);
            break;
        }
        default: {
            char *st = sn_str_or_empty(sn_get(payload, "status"));
            sn_sb_add(&b, " \xE2\x86\x92 ");
            sn_sb_add(&b, st);
            free(st);
            break;
        }
    }
    out = sn_sb_take(&b);
    free(task_id);
    free(title);
    free(who);
    free(slug);
    {
        json_t *j = json_string(out);
        free(out);
        return j;
    }
}

json_t *w29_sessionnot_c045cd_p2_async_delegation_display_metadata(const json_t *evt,
                                                                   const json_t *display_text) {
    const json_t *raw = sn_get(evt, "results");
    long task_count = 0, completed = 0, failed = 0;
    json_t *out = json_object();
    char *delegation_id = sn_str_or_empty(sn_get(evt, "delegation_id"));
    if (raw && raw->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(raw); i++) {
            const json_t *r = json_get(raw, i);
            const json_t *sv;
            if (!r || r->type != JSON_OBJECT) continue;
            task_count++;
            sv = sn_get(r, "status");
            if (sv && sv->type == JSON_STRING && sv->str_val) {
                if (strcmp(sv->str_val, "completed") == 0 || strcmp(sv->str_val, "success") == 0)
                    completed++;
                else if (strcmp(sv->str_val, "failed") == 0 || strcmp(sv->str_val, "error") == 0)
                    failed++;
            }
        }
    }
    if (task_count == 0) task_count = 1;
    json_set(out, "display_text", display_text ? json_copy(display_text) : json_string(""));
    json_set(out, "delegation_id", json_string(delegation_id));
    json_set(out, "task_count", json_number((double)task_count));
    json_set(out, "completed_count", json_number((double)(completed ? completed : task_count - failed)));
    json_set(out, "failed_count", json_number((double)failed));
    {
        const json_t *duration = sn_get(evt, "total_duration_seconds");
        if (!sn_truthy(duration)) duration = sn_get(evt, "duration_seconds");
        if (duration && duration->type == JSON_NUMBER) {
            json_set(out, "duration_seconds", json_copy(duration));
        }
    }
    free(delegation_id);
    return out;
}

json_t *w29_sessionnot_c045cd_p2_prepend_note(const json_t *run_message, const json_t *note) {
    if (!sn_truthy(note)) return json_copy(run_message);
    if (run_message && run_message->type == JSON_STRING) {
        char *n = sn_py_str(note);
        sn_sb b = {0};
        char *out;
        json_t *j;
        sn_sb_add(&b, n);
        sn_sb_add(&b, "\n\n");
        sn_sb_add(&b, run_message->str_val);
        free(n);
        out = sn_sb_take(&b);
        j = json_string(out);
        free(out);
        return j;
    }
    if (run_message && run_message->type == JSON_ARRAY) {
        json_t *out = json_array();
        json_t *head = json_object();
        char *n = sn_py_str(note);
        json_set(head, "type", json_string("text"));
        json_set(head, "text", json_string(n));
        free(n);
        json_append(out, head);
        for (size_t i = 0; i < json_len(run_message); i++)
            json_append(out, json_copy(json_get(run_message, i)));
        return out;
    }
    return json_copy(run_message);
}
