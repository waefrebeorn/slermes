/*
 * hosted_room_policy.c - C11 port of upstream
 * persistence/sqlite/gateway/hosted_room_policy.py.
 *
 * Two differential gates share this one file:
 *   w32_hostedroom_be7d8d_p1  the bounded projection decisions (budget, capacity,
 *                             transcript bound, oldest thread, rebuild verdict,
 *                             sync advance, retained sources, receipt ids)
 *   w32_hostedroom_be7d8d_p2  the statement text, kind dispatch, text coercion,
 *                             predicates and retention cutoff
 *
 * PoP: w32_hostedroom_be7d8d_p1_budget @ persistence/sqlite/gateway/hosted_room_policy.py:SQLiteHostedRoomPolicyRepository._prepare
 * PoP: w32_hostedroom_be7d8d_p1_capacity @ persistence/sqlite/gateway/hosted_room_policy.py:_initialize_schema
 * PoP: w32_hostedroom_be7d8d_p1_transcript_bound @ persistence/sqlite/gateway/hosted_room_policy.py:_discussion_events
 * PoP: w32_hostedroom_be7d8d_p1_snapshot_thread @ persistence/sqlite/gateway/hosted_room_policy.py:SQLiteHostedRoomPolicyRepository.snapshot
 * PoP: w32_hostedroom_be7d8d_p1_replay_state @ persistence/sqlite/gateway/hosted_room_policy.py:_ensure_cursor_and_transcript
 * PoP: w32_hostedroom_be7d8d_p1_sync_step @ persistence/sqlite/gateway/hosted_room_policy.py:SQLiteHostedRoomPolicyRepository._sync
 * PoP: w32_hostedroom_be7d8d_p1_settled_sources @ persistence/sqlite/gateway/hosted_room_policy.py:_settled_source_users
 * PoP: w32_hostedroom_be7d8d_p1_task_event_ids @ persistence/sqlite/gateway/hosted_room_policy.py:_published_task_events
 * PoP: w32_hostedroom_be7d8d_p2_text @ persistence/sqlite/gateway/hosted_room_policy.py:_text
 * PoP: w32_hostedroom_be7d8d_p2_apply_handler @ persistence/sqlite/gateway/hosted_room_policy.py:_apply_event
 * PoP: w32_hostedroom_be7d8d_p2_named_sql @ persistence/sqlite/gateway/hosted_room_policy.py:<statement constants>
 * PoP: w32_hostedroom_be7d8d_p2_schema_ddl @ persistence/sqlite/gateway/hosted_room_policy.py:_SCHEMA_DDL
 * PoP: w32_hostedroom_be7d8d_p2_reopened @ persistence/sqlite/gateway/hosted_room_policy.py:_REOPENED_KINDS
 * PoP: w32_hostedroom_be7d8d_p2_publication_query @ persistence/sqlite/gateway/hosted_room_policy.py:SQLiteHostedRoomPolicyRepository.publication_exists
 * PoP: w32_hostedroom_be7d8d_p2_event_seqs @ persistence/sqlite/gateway/hosted_room_policy.py:SQLiteHostedRoomPolicyRepository.events_for_task
 * PoP: w32_hostedroom_be7d8d_p2_settled_check @ persistence/sqlite/gateway/hosted_room_policy.py:_settled_message
 * PoP: w32_hostedroom_be7d8d_p2_receipt @ persistence/sqlite/gateway/hosted_room_policy.py:_apply_discussion_event
 * PoP: w32_hostedroom_be7d8d_p2_transcript_retention @ persistence/sqlite/gateway/hosted_room_policy.py:_store_transcript_event
 *
 * The SQLite handle is a seam: one connection per verb, the file handles, the
 * transaction framing, the clock (``updated_at`` from the applied row's
 * ``created_at``) and the ``_PREPARED`` guard all stay in the caller.  What is
 * ported here is what those values decide -- which budget a caller gets, which
 * thread the snapshot names, which page a sync step applies, which statement a
 * receipt publishes -- evaluated over fixture rows.
 *
 * Note on number transport: JSON carries no int/float distinction, so a gate
 * that needs Python's ``isinstance(x, int)`` or ``str(x)`` for a number takes
 * the Python type alongside the value (``value_type``: "int" or "float").
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_hostedroom_be7d8d_p1.h"
#include "noc_w32_hostedroom_be7d8d_p2.h"

/* ------------------------------------------------------------------ */
/* shared value helpers                                                */
/* ------------------------------------------------------------------ */

/* Python truthiness of a JSON value. */
static int hpd_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python's numeric view: an int, a float or a bool; None is none of those. */
static int hpd_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

static const char *hpd_str(const json_t *v) {
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return "";
}

/* A growable string, for the Python repr work below. */
typedef struct {
    char *p;
    size_t n, cap;
    int failed;
} hpd_buf;

static void hpd_put(hpd_buf *b, const char *s, size_t n) {
    if (b->failed) return;
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *p;
        while (cap < b->n + n + 1) cap *= 2;
        p = (char *)realloc(b->p, cap);
        if (!p) { b->failed = 1; return; }
        b->p = p;
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void hpd_puts(hpd_buf *b, const char *s) { hpd_put(b, s, strlen(s)); }

/* Python str() of a float: shortest round-tripping form, ".0" kept for
 * integral values below 1e16, exponent form above it. */
static void hpd_float_str(double v, char *out, size_t cap) {
    char tmp[64];
    int prec;
    if (v != v) { snprintf(out, cap, "nan"); return; }
    if (v == (double)(long long)v && v < 1e16 && v > -1e16) {
        snprintf(out, cap, "%lld.0", (long long)v);
        return;
    }
    snprintf(tmp, sizeof tmp, "%.17g", v);
    for (prec = 1; prec < 17; prec++) {
        char t[64];
        snprintf(t, sizeof t, "%.*g", prec, v);
        if (strtod(t, NULL) == v) { snprintf(tmp, sizeof tmp, "%s", t); break; }
    }
    snprintf(out, cap, "%s", tmp);
}

/* Python str() of a number whose Python type the caller names. */
static void hpd_number_str(double v, const char *value_type, char *out, size_t cap) {
    if (value_type && !strcmp(value_type, "float")) {
        hpd_float_str(v, out, cap);
        return;
    }
    if (v == (double)(long long)v && v < 9.0e18 && v > -9.0e18) {
        snprintf(out, cap, "%lld", (long long)v);
        return;
    }
    hpd_float_str(v, out, cap);
}

static void hpd_repr(hpd_buf *b, const json_t *v, const char *value_type);

static void hpd_repr_scalar(hpd_buf *b, const json_t *v, const char *value_type) {
    char num[64];
    switch (v->type) {
    case JSON_NULL:
        hpd_puts(b, "None");
        return;
    case JSON_BOOL:
        hpd_puts(b, v->bool_val ? "True" : "False");
        return;
    case JSON_NUMBER:
        hpd_number_str(v->num_val, value_type, num, sizeof num);
        hpd_puts(b, num);
        return;
    case JSON_STRING: {
        const char *s = v->str_val ? v->str_val : "";
        size_t i;
        hpd_puts(b, "'");
        for (i = 0; s[i]; i++) {
            if (s[i] == '\'' || s[i] == '\\') hpd_put(b, "\\", 1);
            else if (s[i] == '\n') { hpd_puts(b, "\\n"); continue; }
            else if (s[i] == '\t') { hpd_puts(b, "\\t"); continue; }
            hpd_put(b, &s[i], 1);
        }
        hpd_puts(b, "'");
        return;
    }
    default:
        hpd_puts(b, "");
        return;
    }
}

static void hpd_repr(hpd_buf *b, const json_t *v, const char *value_type) {
    size_t i, n;
    if (!v) { hpd_puts(b, "None"); return; }
    if (v->type == JSON_ARRAY) {
        n = json_len(v);
        hpd_puts(b, "[");
        for (i = 0; i < n; i++) {
            if (i) hpd_puts(b, ", ");
            hpd_repr(b, json_get(v, i), value_type);
        }
        hpd_puts(b, "]");
        return;
    }
    if (v->type == JSON_OBJECT) {
        n = json_object_size(v);
        hpd_puts(b, "{");
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(v, i);
            json_t *key = json_string(k ? k : "");
            if (i) hpd_puts(b, ", ");
            hpd_repr_scalar(b, key, value_type);
            hpd_puts(b, ": ");
            hpd_repr(b, json_object_get_at(v, i), value_type);
            json_free(key);
        }
        hpd_puts(b, "}");
        return;
    }
    hpd_repr_scalar(b, v, value_type);
}

/* str(mapping.get(key) or "") -- the _text coercion, including its falsiness. */
static json_t *hpd_text(const json_t *mapping, const char *key, const char *value_type) {
    const json_t *v = NULL;
    hpd_buf b;
    if (mapping && mapping->type == JSON_OBJECT && key) v = json_obj_get(mapping, key);
    if (!hpd_truthy(v)) return json_string("");
    if (v->type == JSON_STRING) return json_string(v->str_val ? v->str_val : "");  /* str(str) is itself */
    b.p = NULL; b.n = 0; b.cap = 0; b.failed = 0;
    hpd_repr(&b, v, value_type);
    if (b.failed || !b.p) { free(b.p); return json_string(""); }
    {
        json_t *out = json_string(b.p);
        free(b.p);
        return out;
    }
}

/* "?,?,?..." for n bound parameters. */
static char *hpd_placeholders(size_t n) {
    char *s;
    size_t i;
    if (n == 0) return (char *)calloc(1, 1);
    s = (char *)malloc(2 * n);
    if (!s) return NULL;
    for (i = 0; i < n; i++) {
        s[2 * i] = '?';
        s[2 * i + 1] = (i + 1 < n) ? ',' : '\0';
    }
    return s;
}

static int hpd_cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static int hpd_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* ------------------------------------------------------------------ */
/* w32_hostedroom_be7d8d_p1: the bounded projection decisions          */
/* ------------------------------------------------------------------ */

/* _prepare: reject what is not a positive int, then clamp the active-event
 * budget up to the configured floor and inside SQLite's integer range. */
json_t *w32_hostedroom_be7d8d_p1_budget(const json_t *value, const char *value_type) {
    json_t *out = json_object();
    double v = 0.0, budget;
    int ok = 1;

    if (!value_type) value_type = "int";
    if (!strcmp(value_type, "bool") || !strcmp(value_type, "float") ||
        !strcmp(value_type, "str") || !strcmp(value_type, "null")) {
        ok = 0;
    } else {
        if (!hpd_number(value, &v)) ok = 0;
        else {
            v = (double)(long long)v;  /* int(12.9) == 12 */
            if (v < 1.0) ok = 0;
        }
    }
    if (!ok) {
        json_set(out, "ok", json_bool(0));
        json_set(out, "error", json_string("max_active_events must be a positive integer"));
        json_set(out, "budget", json_null());
        return out;
    }
    budget = v > (double)W32_HOSTEDROOM_BE7D8D_MAX_ACTIVE_POLICY_EVENTS
                 ? v : (double)W32_HOSTEDROOM_BE7D8D_MAX_ACTIVE_POLICY_EVENTS;
    if (budget > W32_HOSTEDROOM_BE7D8D_SQLITE_INT_MAX) budget = W32_HOSTEDROOM_BE7D8D_SQLITE_INT_MAX;
    json_set(out, "ok", json_bool(1));
    json_set(out, "error", json_null());
    json_set(out, "budget", json_number(budget));
    return out;
}

/* _prepare's _PREPARED cache plus _initialize_schema's
 * MAX(max_active_events, excluded.max_active_events) retention. */
json_t *w32_hostedroom_be7d8d_p1_capacity(const json_t *cached, const json_t *stored, double budget) {
    json_t *out = json_object();
    double c = 0.0, capacity;
    int has_cached = hpd_number(cached, &c);

    if (has_cached && c >= budget) {
        json_set(out, "reinitialize", json_bool(0));
        json_set(out, "capacity", json_number(c));
        return out;
    }
    capacity = budget;
    if (stored) {
        double s = 0.0;
        if (hpd_number(stored, &s) && s > capacity) capacity = s;
    }
    json_set(out, "reinitialize", json_bool(1));
    json_set(out, "capacity", json_number(capacity));
    return out;
}

/* _discussion_events: the transcript bound, one overflow sentinel included. */
json_t *w32_hostedroom_be7d8d_p1_transcript_bound(double max_active_events) {
    double v = (double)W32_HOSTEDROOM_BE7D8D_MAX_THREAD_TRANSCRIPT_EVENTS *
               (max_active_events + (double)W32_HOSTEDROOM_BE7D8D_MAX_ACTIVE_RETRY_RECEIPTS + 2.0);
    if (v > W32_HOSTEDROOM_BE7D8D_SQLITE_INT_MAX) v = W32_HOSTEDROOM_BE7D8D_SQLITE_INT_MAX;
    return json_number(v);
}

/* snapshot: the oldest unfinished thread above the stop watermark,
 * ORDER BY latest_user_seq, thread_id LIMIT 1. */
json_t *w32_hostedroom_be7d8d_p1_snapshot_thread(const json_t *req) {
    const json_t *threads = req ? json_obj_get(req, "threads") : NULL;
    double stopped = 0.0, best_seq = 0.0;
    const json_t *best = NULL;
    const char *best_id = NULL;
    size_t i, n;

    if (req) hpd_number(json_obj_get(req, "stopped_through_seq"), &stopped);
    if (!threads || threads->type != JSON_ARRAY) return json_null();
    n = json_len(threads);
    for (i = 0; i < n; i++) {
        const json_t *t = json_get(threads, i);
        double completed = 0.0, latest = 0.0;
        const char *tid;
        if (!t || t->type != JSON_OBJECT) continue;
        hpd_number(json_obj_get(t, "completed"), &completed);
        if (completed != 0.0) continue;
        hpd_number(json_obj_get(t, "latest_user_seq"), &latest);
        if (!(latest > stopped)) continue;
        tid = hpd_str(json_obj_get(t, "thread_id"));
        if (!best) { best = t; best_seq = latest; best_id = tid; continue; }
        if (latest > best_seq) continue;
        if (latest == best_seq && strcmp(tid, best_id) >= 0) continue;
        best = t; best_seq = latest; best_id = tid;
    }
    if (!best) return json_null();
    {
        json_t *out = json_object();
        json_set(out, "thread_id", json_string(best_id));
        json_set(out, "discussion_event_id", json_string(hpd_str(json_obj_get(best, "discussion_event_id"))));
        return out;
    }
}

/* _ensure_cursor_and_transcript: rebuild derived caches when the recorded
 * transcript schema is absent or older than the current version. */
json_t *w32_hostedroom_be7d8d_p1_replay_state(const json_t *req) {
    static const char *const TABLES[4] = {
        "hosted_room_policy_events", "hosted_room_policy_threads",
        "hosted_room_policy_watermarks", "hosted_room_policy_transcript",
    };
    json_t *out = json_object();
    json_t *tables = json_array();
    const json_t *state = NULL;
    double version = 0.0;
    int rebuild, has_state;
    size_t i;

    has_state = req && hpd_truthy(json_obj_get(req, "has_state"));
    if (has_state && req) state = json_obj_get(req, "schema_version");
    rebuild = !state || state->type == JSON_NULL || !hpd_number(state, &version) ||
              version < (double)W32_HOSTEDROOM_BE7D8D_TRANSCRIPT_SCHEMA_VERSION;
    if (rebuild)
        for (i = 0; i < 4; i++) json_append(tables, json_string(TABLES[i]));
    json_set(out, "rebuild", json_bool(rebuild));
    json_set(out, "write_version",
             rebuild ? json_number((double)W32_HOSTEDROOM_BE7D8D_TRANSCRIPT_SCHEMA_VERSION) : json_null());
    json_set(out, "tables", tables);
    json_set(out, "cursor_reset", json_bool(rebuild));
    return out;
}

/* _sync: one page of the replay loop -- done, refused for not advancing, taken
 * over by another worker, or applied. */
json_t *w32_hostedroom_be7d8d_p1_sync_step(const json_t *req) {
    json_t *out = json_object();
    double cursor = 0.0, latest = 0.0, rows = 0.0, next_cursor = 0.0, through = 0.0;
    const char *action = "done";
    const char *error = NULL;
    double result;

    if (req) {
        hpd_number(json_obj_get(req, "cursor"), &cursor);
        hpd_number(json_obj_get(req, "latest_seq"), &latest);
    }
    result = cursor;
    if (cursor < latest) {
        if (!hpd_number(json_obj_get(req, "rows"), &rows)) rows = 0.0;
        if (!hpd_number(json_obj_get(req, "next_cursor"), &next_cursor) || next_cursor == 0.0)
            next_cursor = cursor;  /* int(page.get("cursor") or cursor) */
        if (rows <= 0.0 || next_cursor <= cursor) {
            action = "error";
            error = "hosted room policy cursor did not advance";
        } else if (!hpd_number(json_obj_get(req, "through_seq"), &through)) {
            action = "error";
            error = "room policy cursor disappeared during replay";
        } else if (through != cursor) {
            action = "stale";
            result = through;
        } else {
            action = "apply";
            result = next_cursor;
        }
    }
    json_set(out, "action", json_string(action));
    json_set(out, "cursor", json_number(result));
    json_set(out, "error", error ? json_string(error) : json_null());
    return out;
}

/* _settled_source_users: the canonical user sources retained non-pass
 * settlements still need, minus the events already in the projection. */
json_t *w32_hostedroom_be7d8d_p1_settled_sources(const json_t *events) {
    json_t *out = json_array();
    const char **ids = NULL;
    const char **all_ids = NULL;
    size_t n = 0, cap = 0, count = 0, all_count = 0, i, j;

    if (!events || events->type != JSON_ARRAY) return out;
    cap = json_len(events) * 2 + 1;
    ids = (const char **)calloc(cap, sizeof(*ids));
    all_ids = (const char **)calloc(cap, sizeof(*all_ids));
    if (!ids || !all_ids) { free(ids); free(all_ids); return out; }

    for (i = 0; i < json_len(events); i++) {
        const json_t *e = json_get(events, i);
        const char *eid;
        if (!e || e->type != JSON_OBJECT) continue;
        eid = hpd_str(json_obj_get(e, "event_id"));
        all_ids[all_count++] = eid;
        if (strcmp(hpd_str(json_obj_get(e, "kind")), "turn.settled") != 0) continue;
        {
            const json_t *payload = json_obj_get(e, "payload");
            const json_t *passed = payload && payload->type == JSON_OBJECT ? json_obj_get(payload, "passed") : NULL;
            if (hpd_truthy(passed)) continue;
            ids[count++] = hpd_str(payload && payload->type == JSON_OBJECT
                                       ? json_obj_get(payload, "discussion_event_id") : NULL);
        }
    }
    /* source_ids.difference_update(event ids) */
    j = 0;
    for (i = 0; i < count; i++) {
        size_t k;
        int found = 0;
        for (k = 0; k < all_count; k++)
            if (strcmp(ids[i], all_ids[k]) == 0) { found = 1; break; }
        if (!found) ids[j++] = ids[i];
    }
    n = j;
    qsort(ids, n, sizeof(*ids), hpd_cmp_str);
    for (i = 0; i < n; i++) {
        if (i > 0 && strcmp(ids[i], ids[i - 1]) == 0) continue;
        json_append(out, json_string(ids[i]));
    }
    free(ids);
    free(all_ids);
    return out;
}

/* _published_task_events: the two room event ids a task's receipts name. */
json_t *w32_hostedroom_be7d8d_p1_task_event_ids(const json_t *task_id) {
    json_t *out = json_object();
    json_t *ids = json_array();
    const char *tid = hpd_str(task_id);

    if (!hpd_truthy(task_id)) {
        json_set(out, "digest", json_null());
        json_set(out, "event_ids", ids);
        return out;
    }
    if (!strncmp(tid, "dtask:", 6)) tid += 6;
    json_set(out, "digest", json_string(tid));
    {
        size_t need = strlen(tid) + 16;
        char *s = (char *)malloc(need);
        if (s) {
            snprintf(s, need, "dmessage:%s", tid);
            json_append(ids, json_string(s));
            snprintf(s, need, "dterminal:%s", tid);
            json_append(ids, json_string(s));
            free(s);
        }
    }
    json_set(out, "event_ids", ids);
    return out;
}

/* ------------------------------------------------------------------ */
/* w32_hostedroom_be7d8d_p2: statement text, dispatch and coercions    */
/* ------------------------------------------------------------------ */

json_t *w32_hostedroom_be7d8d_p2_text(const json_t *mapping, const char *key, const char *value_type) {
    return hpd_text(mapping, key, value_type ? value_type : "int");
}

/* _APPLY_BY_KIND: the handler each kind dispatches to. */
json_t *w32_hostedroom_be7d8d_p2_apply_handler(const char *kind) {
    const char *handler = NULL;
    if (!kind) return json_null();
    if (!strcmp(kind, "message.user")) handler = "_apply_user_message";
    else if (!strcmp(kind, "message.member")) handler = "_apply_discussion_event";
    else if (!strcmp(kind, "turn.settled")) handler = "_apply_discussion_event";
    else if (!strcmp(kind, "turn.failed")) handler = "_apply_discussion_event";
    else if (!strcmp(kind, "turn.cancelled")) handler = "_apply_discussion_event";
    else if (!strcmp(kind, "turn.deferred")) handler = "_apply_discussion_event";
    else if (!strcmp(kind, "room.activity")) handler = "_apply_room_activity";
    else if (!strcmp(kind, "room.assignment")) handler = "_apply_discussion_event";
    else if (!strcmp(kind, "room.routed")) handler = "_apply_discussion_event";
    else if (!strcmp(kind, "room.continued")) handler = "_apply_continued";
    else if (!strcmp(kind, "room.stop_requested")) handler = "_apply_stop_requested";
    if (!handler) return json_null();
    return json_string(handler);
}

/* _ROOM_EVENT_COLUMNS */
static const char *const HPD_ROOM_EVENT_COLUMNS =
    "room_id, seq, event_id, kind, actor_json, authority_epoch, payload_json, created_at";

/* _DELETE_ACTIVE_EVENTS_SQL */
static const char *const HPD_DELETE_ACTIVE_EVENTS_SQL =
    "DELETE FROM hosted_room_policy_events WHERE room_id=? AND discussion_event_id=?";

/* _TRANSCRIPT_EVENTS_SQL */
static const char *const HPD_TRANSCRIPT_EVENTS_SQL =
    "WITH transcript_events(seq) AS (\n"
    "        SELECT seq FROM hosted_room_policy_transcript WHERE room_id=? AND thread_id=?\n"
    "        UNION ALL\n"
    "        SELECT settled_seq FROM hosted_room_policy_transcript\n"
    "         WHERE room_id=? AND thread_id=? AND settled_seq IS NOT NULL)\n"
    "    SELECT events.room_id, events.seq, events.event_id, events.kind, events.actor_json, events.authority_epoch, events.payload_json, events.created_at\n"
    "    FROM transcript_events\n"
    "    JOIN hosted_room_events AS events ON events.room_id=? AND events.seq=transcript_events.seq\n"
    "    ORDER BY events.seq LIMIT ?";

json_t *w32_hostedroom_be7d8d_p2_named_sql(const char *name) {
    if (!name) return json_null();
    if (!strcmp(name, "delete_active")) return json_string(HPD_DELETE_ACTIVE_EVENTS_SQL);
    if (!strcmp(name, "transcript")) return json_string(HPD_TRANSCRIPT_EVENTS_SQL);
    if (!strcmp(name, "room_columns")) return json_string(HPD_ROOM_EVENT_COLUMNS);
    return json_null();
}

/* _SCHEMA_DDL, in creation order. */
static const char *const HPD_SCHEMA_DDL[11] = {
    "CREATE TABLE IF NOT EXISTS hosted_room_policy_capacity (\n"
    "        singleton INTEGER PRIMARY KEY CHECK(singleton=1), max_active_events INTEGER NOT NULL)",
    "CREATE TABLE IF NOT EXISTS hosted_room_policy_cursors (\n"
    "        room_id TEXT PRIMARY KEY, through_seq INTEGER NOT NULL DEFAULT 0,\n"
    "        stopped_through_seq INTEGER NOT NULL DEFAULT 0, updated_at REAL NOT NULL DEFAULT 0)",
    "CREATE TABLE IF NOT EXISTS hosted_room_policy_threads (\n"
    "        room_id TEXT NOT NULL, thread_id TEXT NOT NULL, discussion_event_id TEXT NOT NULL,\n"
    "        latest_user_seq INTEGER NOT NULL, completed INTEGER NOT NULL DEFAULT 0,\n"
    "        PRIMARY KEY(room_id, thread_id))",
    "CREATE INDEX IF NOT EXISTS idx_hosted_room_policy_pending\n"
    "        ON hosted_room_policy_threads(room_id, completed, latest_user_seq, thread_id)",
    "CREATE TABLE IF NOT EXISTS hosted_room_policy_events (\n"
    "        room_id TEXT NOT NULL, thread_id TEXT NOT NULL, discussion_event_id TEXT NOT NULL,\n"
    "        seq INTEGER NOT NULL, event_json TEXT NOT NULL, PRIMARY KEY(room_id, seq))",
    "CREATE INDEX IF NOT EXISTS idx_hosted_room_policy_events_active\n"
    "        ON hosted_room_policy_events(room_id, discussion_event_id, seq)",
    "CREATE TABLE IF NOT EXISTS hosted_room_policy_watermarks (\n"
    "        room_id TEXT NOT NULL, thread_id TEXT NOT NULL, member_id TEXT NOT NULL,\n"
    "        seen_through_seq INTEGER NOT NULL, PRIMARY KEY(room_id, thread_id, member_id))",
    "CREATE TABLE IF NOT EXISTS hosted_room_policy_publications (\n"
    "        room_id TEXT NOT NULL, task_id TEXT NOT NULL, kind TEXT NOT NULL,\n"
    "        execution_generation INTEGER NOT NULL DEFAULT 0, seq INTEGER NOT NULL,\n"
    "        PRIMARY KEY(room_id, task_id, kind, execution_generation))",
    "CREATE TABLE IF NOT EXISTS hosted_room_policy_transcript (\n"
    "        room_id TEXT NOT NULL, thread_id TEXT NOT NULL, seq INTEGER NOT NULL,\n"
    "        kind TEXT NOT NULL, settled_seq INTEGER, PRIMARY KEY(room_id, thread_id, seq))",
    "CREATE TABLE IF NOT EXISTS hosted_room_policy_transcript_state (\n"
    "        room_id TEXT PRIMARY KEY, schema_version INTEGER NOT NULL)",
    "CREATE INDEX IF NOT EXISTS idx_hosted_room_user_thread\n"
    "       ON hosted_room_events(room_id, json_extract(payload_json, '$.thread_id'), seq)\n"
    "       WHERE kind='message.user'",
};

json_t *w32_hostedroom_be7d8d_p2_schema_ddl(void) {
    json_t *out = json_array();
    size_t i;
    for (i = 0; i < 11; i++) json_append(out, json_string(HPD_SCHEMA_DDL[i]));
    return out;
}

/* _REOPENED_KINDS: what a Continue restores into a reopened discussion. */
static const char *const HPD_REOPENED_KINDS[8] = {
    "turn.cancelled", "turn.deferred", "turn.failed", "turn.settled",
    "message.member", "room.assignment", "room.routed", "room.continued",
};

json_t *w32_hostedroom_be7d8d_p2_reopened(void) {
    json_t *out = json_object();
    json_t *kinds = json_array();
    char *ph = hpd_placeholders(8);
    size_t i;
    for (i = 0; i < 8; i++) json_append(kinds, json_string(HPD_REOPENED_KINDS[i]));
    json_set(out, "kinds", kinds);
    json_set(out, "placeholders", json_string(ph ? ph : ""));
    json_set(out, "count", json_number(8.0));
    free(ph);
    return out;
}

/* publication_exists: the deferred branch pins one exact driver outcome, every
 * other status accepts any terminal outcome. */
json_t *w32_hostedroom_be7d8d_p2_publication_query(const char *status) {
    static const char *const EXACT =
        "SELECT 1 FROM hosted_room_policy_publications\n"
        "                     WHERE room_id=? AND task_id=? AND kind=? AND execution_generation=?";
    static const char *const TERMINAL =
        "SELECT 1 FROM hosted_room_policy_publications\n"
        "                     WHERE room_id=? AND task_id=? AND kind IN ('turn.settled', 'turn.failed', 'turn.cancelled')";
    static const char *const TERMINAL_KINDS[3] = {"turn.settled", "turn.failed", "turn.cancelled"};
    json_t *out = json_object();
    json_t *kinds = json_array();

    if (status && !strcmp(status, "deferred")) {
        char buf[32];
        snprintf(buf, sizeof buf, "turn.%s", status);
        json_append(kinds, json_string(buf));
        json_set(out, "sql", json_string(EXACT));
        json_set(out, "kinds", kinds);
        json_set(out, "uses_generation", json_bool(1));
        return out;
    }
    json_append(kinds, json_string(TERMINAL_KINDS[0]));
    json_append(kinds, json_string(TERMINAL_KINDS[1]));
    json_append(kinds, json_string(TERMINAL_KINDS[2]));
    json_set(out, "sql", json_string(TERMINAL));
    json_set(out, "kinds", kinds);
    json_set(out, "uses_generation", json_bool(0));
    return out;
}

/* events_for_task: sorted({source_event_seq, *context["event_seqs"]}) and the
 * placeholders its IN(...) list binds. */
json_t *w32_hostedroom_be7d8d_p2_event_seqs(const json_t *source_event_seq, const json_t *event_seqs) {
    json_t *out = json_object();
    json_t *seqs = json_array();
    double *values;
    size_t cap, n = 0, i, j;
    char *ph;

    cap = 1 + (event_seqs && event_seqs->type == JSON_ARRAY ? json_len(event_seqs) : 0);
    values = (double *)calloc(cap, sizeof(*values));
    if (!values) {
        json_set(out, "seqs", seqs);
        json_set(out, "placeholders", json_string(""));
        json_set(out, "count", json_number(0.0));
        return out;
    }
    {
        double v = 0.0;
        if (hpd_number(source_event_seq, &v)) values[n++] = v;
    }
    if (event_seqs && event_seqs->type == JSON_ARRAY) {
        for (i = 0; i < json_len(event_seqs); i++) {
            double v = 0.0;
            if (hpd_number(json_get(event_seqs, i), &v)) values[n++] = v;
        }
    }
    qsort(values, n, sizeof(*values), hpd_cmp_double);
    j = 0;
    for (i = 0; i < n; i++) {
        if (j > 0 && values[i] == values[j - 1]) continue;
        values[j++] = values[i];
    }
    n = j;
    for (i = 0; i < n; i++) json_append(seqs, json_number(values[i]));
    ph = hpd_placeholders(n);
    json_set(out, "seqs", seqs);
    json_set(out, "placeholders", json_string(ph ? ph : ""));
    json_set(out, "count", json_number((double)n));
    free(ph);
    free(values);
    return out;
}

/* _settled_message: the message counts only when it is a member message of the
 * same discussion. */
json_t *w32_hostedroom_be7d8d_p2_settled_check(const json_t *row, const char *discussion_event_id) {
    const json_t *payload;
    if (!row || row->type != JSON_OBJECT) return json_bool(0);
    if (strcmp(hpd_str(json_obj_get(row, "kind")), "message.member") != 0) return json_bool(0);
    payload = json_obj_get(row, "payload");
    if (!payload || payload->type != JSON_OBJECT) return json_bool(0);
    return json_bool(strcmp(hpd_str(json_obj_get(payload, "discussion_event_id")),
                            discussion_event_id ? discussion_event_id : "") == 0);
}

/* _apply_discussion_event: the publication receipt, watermark and transcript
 * path one terminal (or member) event commits. */
json_t *w32_hostedroom_be7d8d_p2_receipt(const json_t *kind, const json_t *payload) {
    json_t *out = json_object();
    const char *k = hpd_str(kind);
    const json_t *p = payload && payload->type == JSON_OBJECT ? payload : NULL;
    const char *task_id = hpd_str(p ? json_obj_get(p, "task_id") : NULL);
    const char *member_id = hpd_str(p ? json_obj_get(p, "member_id") : NULL);
    double gen = 0.0, seen = 0.0;
    int terminal, is_deferred, publish, watermark;
    const char *path = NULL;

    terminal = !strcmp(k, "turn.settled") || !strcmp(k, "turn.failed") ||
               !strcmp(k, "turn.cancelled") || !strcmp(k, "turn.deferred");
    is_deferred = !strcmp(k, "turn.deferred");
    if (is_deferred && p && !hpd_number(json_obj_get(p, "execution_generation"), &gen)) gen = 0.0;
    if (!is_deferred) gen = 0.0;
    gen = (double)(long long)gen;
    if (p) hpd_number(json_obj_get(p, "seen_through_seq"), &seen);
    seen = (double)(long long)seen;
    publish = terminal && task_id[0] != '\0';
    watermark = terminal && member_id[0] != '\0' && seen > 0.0;
    if (!strcmp(k, "turn.settled") && p && hpd_truthy(json_obj_get(p, "message_event_id"))) path = "settled";
    else if (terminal) path = "transcript";

    json_set(out, "terminal", json_bool(terminal));
    json_set(out, "publish", json_bool(publish));
    json_set(out, "task_id", json_string(task_id));
    json_set(out, "execution_generation", json_number(gen));
    json_set(out, "member_id", json_string(member_id));
    json_set(out, "seen_through_seq", json_number(seen));
    json_set(out, "watermark", json_bool(watermark));
    json_set(out, "path", path ? json_string(path) : json_null());
    return out;
}

/* _store_transcript_event: the message retention cutoff, OFFSET
 * MAX_THREAD_TRANSCRIPT_EVENTS-1 over the newest-first message seqs. */
json_t *w32_hostedroom_be7d8d_p2_transcript_retention(const json_t *seqs) {
    json_t *out = json_object();
    double *values;
    size_t n = 0, i, offset = W32_HOSTEDROOM_BE7D8D_MAX_THREAD_TRANSCRIPT_EVENTS - 1;
    int has_cutoff = 0;
    double cutoff = 0.0;

    n = seqs && seqs->type == JSON_ARRAY ? json_len(seqs) : 0;
    values = (double *)calloc(n ? n : 1, sizeof(*values));
    if (values) {
        size_t count = 0;
        for (i = 0; i < n; i++) {
            double v = 0.0;
            if (hpd_number(json_get(seqs, i), &v)) values[count++] = v;
        }
        n = count;
        qsort(values, n, sizeof(*values), hpd_cmp_double);
        {   /* the transcript's primary key makes the seqs distinct */
            size_t j = 0;
            for (i = 0; i < n; i++) {
                if (j > 0 && values[i] == values[j - 1]) continue;
                values[j++] = values[i];
            }
            n = j;
        }
        if (n > offset) { has_cutoff = 1; cutoff = values[n - 1 - offset]; }
        free(values);
    }
    json_set(out, "cutoff", has_cutoff ? json_number(cutoff) : json_null());
    json_set(out, "delete_below", has_cutoff ? json_number(cutoff) : json_null());
    json_set(out, "offset", json_number((double)offset));
    return out;
}
