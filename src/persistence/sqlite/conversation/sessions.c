/*
 * sessions.c - C11 port of upstream persistence/sqlite/conversation/sessions.py
 * (gate family w29_sessions_2114c9_p1..p4).
 *
 * PoP: w29_sessions_2114c9_p1_stripped            @ persistence/sqlite/conversation/sessions.py:_stripped
 * PoP: w29_sessions_2114c9_p1_json_value          @ persistence/sqlite/conversation/sessions.py:_json_value
 * PoP: w29_sessions_2114c9_p1_tool_pin            @ persistence/sqlite/conversation/sessions.py:_tool_pin
 * PoP: w29_sessions_2114c9_p1_ids                 @ persistence/sqlite/conversation/sessions.py:_ids
 * PoP: w29_sessions_2114c9_p1_removed             @ persistence/sqlite/conversation/sessions.py:_removed
 * PoP: w29_sessions_2114c9_p1_listed              @ persistence/sqlite/conversation/sessions.py:_listed
 * PoP: w29_sessions_2114c9_p1_optional_list       @ persistence/sqlite/conversation/sessions.py:_optional_list
 * PoP: w29_sessions_2114c9_p1_sql_limit           @ persistence/sqlite/conversation/sessions.py:_sql_limit
 * PoP: w29_sessions_2114c9_p1_last_active         @ persistence/sqlite/conversation/sessions.py:_last_active
 * PoP: w29_sessions_2114c9_p1_parse_model_config  @ persistence/sqlite/conversation/_store/sessions.py:_parse_model_config
 * PoP: w29_sessions_2114c9_p2_session_record      @ persistence/sqlite/conversation/sessions.py:session_record
 * PoP: w29_sessions_2114c9_p2_constants           @ persistence/sqlite/conversation/sessions.py:<_MARKERS,_TITLE_SOURCES,_TITLE_HELD,_CANONICAL_TITLE_LOCKED>
 * PoP: w29_sessions_2114c9_p3_sql_literal         @ persistence/sqlite/_schema_ddl.py:_sql_literal
 * PoP: w29_sessions_2114c9_p3_json_extract        @ persistence/sqlite/_schema_ddl.py:_sql_json_extract
 * PoP: w29_sessions_2114c9_p3_legacy_reset_child  @ persistence/sqlite/_schema_ddl.py:_legacy_reset_child_sql
 * PoP: w29_sessions_2114c9_p3_reset_child         @ persistence/sqlite/_schema_ddl.py:_RESET_CHILD_SQL
 * PoP: w29_sessions_2114c9_p3_descendants         @ persistence/sqlite/conversation/sessions.py:_DESCENDANTS_SQL
 * PoP: w29_sessions_2114c9_p3_session_last_active @ persistence/sqlite/conversation/_store/sql_common.py:_sql_session_last_active
 * PoP: w29_sessions_2114c9_p3_local_chat          @ persistence/sqlite/conversation/sessions.py:_LOCAL_CHAT
 * PoP: w29_sessions_2114c9_p3_owner_filter        @ persistence/sqlite/conversation/sessions.py:_read_bot_sessions.owner_filter
 * PoP: w29_sessions_2114c9_p3_roster_sqls         @ persistence/sqlite/conversation/sessions.py:_read_bot_sessions.<canonical|latest|worker>
 * PoP: w29_sessions_2114c9_p3_last_active_select  @ persistence/sqlite/conversation/sessions.py:SQLiteSessionRepository.last_active
 * PoP: w29_sessions_2114c9_p3_title_refusal       @ persistence/sqlite/conversation/sessions.py:_title_refusals
 * PoP: w29_sessions_2114c9_p3_fragments           @ persistence/sqlite/conversation/sessions.py:<_LOCAL_CHAT,INTERNAL_LISTING_SOURCES,LEGACY_PREFIX>
 * PoP: w29_sessions_2114c9_p4_list_entry          @ persistence/sqlite/conversation/sessions.py:_list_entry
 * PoP: w29_sessions_2114c9_p4_recency_entry       @ persistence/sqlite/conversation/sessions.py:_recency_entry
 * PoP: w29_sessions_2114c9_p4_roster_session      @ persistence/sqlite/conversation/sessions.py:_roster_session
 * PoP: w29_sessions_2114c9_p4_roster_summary      @ persistence/sqlite/conversation/sessions.py:_read_bot_sessions.summary
 * PoP: w29_sessions_2114c9_p4_roster_last_session @ persistence/sqlite/conversation/sessions.py:_read_bot_sessions.fields[last_session]
 * PoP: w29_sessions_2114c9_p4_list_row            @ persistence/sqlite/conversation/sessions.py:SQLiteSessionRepository.list_row
 * PoP: w29_sessions_2114c9_p4_live_listing_row    @ persistence/sqlite/conversation/sessions.py:SQLiteSessionRepository.live_listing_facts
 * PoP: w29_sessions_2114c9_p4_pending_by_session  @ persistence/sqlite/conversation/sessions.py:SQLiteSessionRepository.live_listing_facts.pending
 * PoP: w29_sessions_2114c9_p4_recent_entry        @ persistence/sqlite/conversation/sessions.py:SQLiteSessionRepository.list_recent_sessions
 * PoP: w29_sessions_2114c9_p4_worker_session      @ persistence/sqlite/conversation/sessions.py:SQLiteSessionRepository.roster_sessions.worker
 *
 * SEAMS (not ported here, fixed by the caller):
 *   - SQLiteStore / StoreHolder: every connection, read context, writer and
 *     the refusals() wrapper.  The repository methods that own them stay a
 *     fixture seam; what is ported is the value-level work those methods do.
 *   - _shape_preview (picker segments) and sanitize_title (create_exclusive)
 *     live in other modules and drag the skill-description registry; they stay
 *     imports on the Python side and are unused by these gates.
 *   - session_unread is imported from _store.sessions; its three-line rule is
 *     restated in sr_session_unread because _list_entry consumes it.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w29_sessions_2114c9_p1.h"
#include "noc_w29_sessions_2114c9_p2.h"
#include "noc_w29_sessions_2114c9_p3.h"
#include "noc_w29_sessions_2114c9_p4.h"

/* ── shared value helpers ─────────────────────────────────────────────────── */

static int sr_is_str(const json_t *v) {
    return v && v->type == JSON_STRING && v->str_val;
}

/* Python truthiness of a decoded JSON value. */
static int sr_truthy(const json_t *v) {
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

static int sr_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* str.strip() bounds: [*b, *e) after trimming; returns 1 when anything is left. */
static int sr_strip_bounds(const char *s, size_t *b, size_t *e) {
    size_t i = 0, j;
    if (!s) { *b = *e = 0; return 0; }
    j = strlen(s);
    while (i < j && sr_ws(s[i])) i++;
    while (j > i && sr_ws(s[j - 1])) j--;
    *b = i;
    *e = j;
    return j > i;
}

static json_t *sr_str_n(const char *s, size_t n) {
    char *tmp = (char *)malloc(n + 1);
    json_t *out;
    if (!tmp) return json_string("");
    if (n) memcpy(tmp, s, n);
    tmp[n] = '\0';
    out = json_string(tmp);
    free(tmp);
    return out;
}

static json_t *sr_str_or_null(const char *s, size_t n) {
    char *tmp;
    json_t *out;
    if (n == 0) return json_null();
    tmp = (char *)malloc(n + 1);
    if (!tmp) return json_null();
    memcpy(tmp, s, n);
    tmp[n] = '\0';
    out = json_string(tmp);
    free(tmp);
    return out;
}

/* Python float(value) over the shapes a stored cell holds; 0 on failure. */
static int sr_float_of(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_STRING && v->str_val) {
        char *end = NULL;
        double d = strtod(v->str_val, &end);
        if (end && end != v->str_val) {
            while (end && *end && sr_ws(*end)) end++;
            if (end && *end == '\0') { *out = d; return 1; }
        }
    }
    return 0;
}

/* int(x or 0): falsey -> 0, else truncate toward zero. */
static double sr_int_of(const json_t *v) {
    double d = 0.0;
    if (!sr_truthy(v)) return 0.0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_NUMBER) return trunc(v->num_val);
    if (v->type == JSON_STRING && sr_float_of(v, &d)) return trunc(d);
    return 0.0;
}

/* bool(cell) as the record constructors spell it. */
static int sr_bool_of(const json_t *v) {
    return sr_truthy(v);
}

static json_t *sr_copy(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

static const json_t *sr_row(const json_t *req, const char *key) {
    return req ? json_obj_get(req, key) : NULL;
}

/* json.dumps of a value is enough as a stable dedupe/identity key. */
/* Python str() of a value used as a mapping key: SessionId(None) keys the bucket
 * "None", not nothing. */
static char *sr_pystr_key(const json_t *v) {
    char buf[64];
    if (!v) return NULL;
    switch (v->type) {
    case JSON_NULL:   return strdup("None");
    case JSON_STRING: return v->str_val ? strdup(v->str_val) : NULL;
    case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return strdup(buf);
    default:          return json_dumps((json_t *)v, 0);
    }
}

static char *sr_key(const json_t *v) {
    return v ? json_dumps(v, 0) : NULL;
}

/* ── growable text buffer ─────────────────────────────────────────────────── */

typedef struct {
    char  *p;
    size_t n;
    size_t cap;
} sr_sb;

static void sb_init(sr_sb *b) {
    b->cap = 256;
    b->n = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
    else b->cap = 0;
}

static void sb_need(sr_sb *b, size_t extra) {
    if (!b->p) return;
    if (b->n + extra + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *np;
        while (b->n + extra + 1 > cap) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) return;
        b->p = np;
        b->cap = cap;
    }
}

static void sb_addn(sr_sb *b, const char *s, size_t n) {
    sb_need(b, n);
    if (!b->p) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void sb_add(sr_sb *b, const char *s) {
    if (s) sb_addn(b, s, strlen(s));
}

static void sb_addc(sr_sb *b, char c) {
    sb_addn(b, &c, 1);
}

static json_t *sb_take(sr_sb *b) {
    json_t *out;
    if (!b->p) return json_string("");
    out = json_string(b->p);
    free(b->p);
    b->p = NULL;
    b->n = b->cap = 0;
    return out;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Gate p1 - coercion, id keys, filter/pagination arithmetic
 * ══════════════════════════════════════════════════════════════════════════ */

/* _stripped(value): value.strip() if isinstance(value, str) and value.strip() else None */
json_t *w29_sessions_2114c9_p1_stripped(const json_t *req) {
    const json_t *v = sr_row(req, "value");
    size_t b, e;
    if (!sr_is_str(v)) return json_null();
    if (!sr_strip_bounds(v->str_val, &b, &e)) return json_null();
    return sr_str_or_null(v->str_val + b, e - b);
}

/* _json_value(text): decoded JSON column, None when unset or unreadable. */
json_t *w29_sessions_2114c9_p1_json_value(const json_t *req) {
    const json_t *v = sr_row(req, "text");
    json_t *parsed;
    if (!sr_is_str(v) || v->str_val[0] == '\0') return json_null();
    parsed = json_parse(v->str_val, NULL);
    return parsed ? parsed : json_null();
}

/* _tool_pin(text): None for an unresolved content hash, else the decoded value. */
json_t *w29_sessions_2114c9_p1_tool_pin(const json_t *req) {
    const json_t *v = sr_row(req, "text");
    if (sr_is_str(v)) {
        size_t i, n = strlen(v->str_val);
        if (n == 64) {
            int hex = 1;
            for (i = 0; i < n; i++) {
                char c = v->str_val[i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) { hex = 0; break; }
            }
            if (hex) return json_null();
        }
    }
    return w29_sessions_2114c9_p1_json_value(req);
}

/* _ids(values): tuple(SessionId(value) for value in values) */
json_t *w29_sessions_2114c9_p1_ids(const json_t *req) {
    const json_t *values = sr_row(req, "values");
    json_t *out = json_array();
    size_t i;
    if (!values || values->type != JSON_ARRAY) return out;
    for (i = 0; i < json_len(values); i++) json_append(out, sr_copy(json_get(values, i)));
    return out;
}

/* _removed(values): dict.fromkeys - each id once, first occurrence order kept. */
json_t *w29_sessions_2114c9_p1_removed(const json_t *req) {
    const json_t *values = sr_row(req, "values");
    json_t *out = json_array();
    size_t i, j;
    if (!values || values->type != JSON_ARRAY) return out;
    for (i = 0; i < json_len(values); i++) {
        const json_t *v = json_get(values, i);
        char *k = sr_key(v);
        int dup = 0;
        for (j = 0; j < json_len(out); j++) {
            char *kk = sr_key(json_get(out, j));
            if (k && kk && !strcmp(k, kk)) dup = 1;
            free(kk);
            if (dup) break;
        }
        free(k);
        if (!dup) json_append(out, sr_copy(v));
    }
    return out;
}

/* list(values) of a string is its characters. */
static json_t *sr_chars(const char *s) {
    json_t *out = json_array();
    size_t i, n = s ? strlen(s) : 0;
    for (i = 0; i < n; i++) {
        char one[2];
        one[0] = s[i];
        one[1] = '\0';
        json_append(out, json_string(one));
    }
    return out;
}

/* _listed(values): list(values) if values else None */
json_t *w29_sessions_2114c9_p1_listed(const json_t *req) {
    const json_t *values = sr_row(req, "values");
    if (!values || values->type == JSON_NULL) return json_null();
    if (values->type == JSON_ARRAY) return json_len(values) ? json_copy(values) : json_null();
    if (values->type == JSON_STRING) {
        if (!values->str_val || values->str_val[0] == '\0') return json_null();
        return sr_chars(values->str_val);
    }
    return json_null();
}

/* _optional_list(values): list(values) if values is not None else None */
json_t *w29_sessions_2114c9_p1_optional_list(const json_t *req) {
    const json_t *values = sr_row(req, "values");
    if (!values || values->type == JSON_NULL) return json_null();
    if (values->type == JSON_ARRAY) return json_copy(values);
    if (values->type == JSON_STRING) return sr_chars(values->str_val);
    if (values->type == JSON_OBJECT) {
        /* list(a_mapping) is its KEYS: {} is [], not None */
        json_t *out = json_array();
        size_t i, n = json_object_size(values);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(values, i);
            if (k) json_append(out, json_string(k));
        }
        return out;
    }
    return json_null();
}

/* _sql_limit(window): -1 (no limit) when window.limit is None, else the limit. */
json_t *w29_sessions_2114c9_p1_sql_limit(const json_t *req) {
    const json_t *window = sr_row(req, "window");
    const json_t *limit;
    if (!window || window->type != JSON_OBJECT) return json_number(20);
    limit = json_obj_get(window, "limit");
    if (!limit) return json_number(20);              /* SessionWindow(limit=20) default */
    if (limit->type == JSON_NULL) return json_number(-1);
    if (limit->type == JSON_NUMBER) return json_number(trunc(limit->num_val));
    if (limit->type == JSON_BOOL) return json_number(limit->bool_val ? 1 : 0);
    return json_number(trunc(sr_int_of(limit)));
}

/* _last_active(row): None if the cell is unset, else float(cell). */
json_t *w29_sessions_2114c9_p1_last_active(const json_t *req) {
    const json_t *row = sr_row(req, "row");
    const json_t *v = row ? json_obj_get(row, "last_active") : NULL;
    double d;
    if (!v || v->type == JSON_NULL) return json_null();
    if (!sr_float_of(v, &d)) return json_null();
    return json_number(d);
}

/* _parse_model_config(raw): tolerant model_config decode -> dict copy, else {}. */
static json_t *sr_parse_model_config(const json_t *raw) {
    if (sr_is_str(raw)) {
        size_t b, e;
        if (sr_strip_bounds(raw->str_val, &b, &e)) {
            json_t *decoded = json_parse(raw->str_val, NULL);
            if (decoded) {
                if (decoded->type == JSON_OBJECT) return decoded;
                json_free(decoded);
            }
            return json_object();
        }
    }
    if (raw && raw->type == JSON_OBJECT) return json_copy(raw);
    return json_object();
}

json_t *w29_sessions_2114c9_p1_parse_model_config(const json_t *req) {
    return sr_parse_model_config(sr_row(req, "raw"));
}

/* ══════════════════════════════════════════════════════════════════════════
 * Gate p2 - the legacy row -> SessionRecord envelope
 * ══════════════════════════════════════════════════════════════════════════ */

static json_t *sr_stripped_json(const json_t *v) {
    size_t b, e;
    if (!sr_is_str(v)) return json_null();
    if (!sr_strip_bounds(v->str_val, &b, &e)) return json_null();
    return sr_str_or_null(v->str_val + b, e - b);
}

/* is_corrupt_model_config(raw): non-blank text that does not decode as JSON. */
static int sr_model_config_corrupt(const json_t *raw) {
    size_t b, e;
    json_t *parsed;
    if (!sr_is_str(raw)) return 0;
    if (!sr_strip_bounds(raw->str_val, &b, &e)) return 0;
    parsed = json_parse(raw->str_val, NULL);
    if (!parsed) return 1;
    json_free(parsed);
    return 0;
}

/* stored_instant(cell) or UNTRUSTED_INSTANT: trusted epoch or the window floor. */
static json_t *sr_instant_or_floor(const json_t *v) {
    double d;
    if (!v || v->type == JSON_NULL) return json_number(0.0);
    if (sr_is_str(v) && v->str_val[0] == '\0') return json_number(0.0);
    if (!sr_float_of(v, &d)) return json_number(0.0);
    if (!(d >= 0.0 && d <= 4200000000.0)) return json_number(0.0);
    return json_number(d);
}

static json_t *sr_instant_or_null(const json_t *v) {
    double d;
    if (!v || v->type == JSON_NULL) return json_null();
    if (sr_is_str(v) && v->str_val[0] == '\0') return json_null();
    if (!sr_float_of(v, &d)) return json_null();
    if (!(d >= 0.0 && d <= 4200000000.0)) return json_null();
    return json_number(d);
}

static void sr_pass(json_t *out, const char *field, const json_t *v) {
    json_set(out, field, sr_copy(v));
}

static void sr_pass_or(json_t *out, const char *field, const json_t *v, const char *empty) {
    json_set(out, field, sr_truthy(v) ? json_copy(v) : json_string(empty));
}

/* row.get(field, default): the default only when the key is absent. */
static void sr_pass_default(json_t *out, const char *field, const json_t *row, double def) {
    const json_t *v = row ? json_obj_get(row, field) : NULL;
    json_set(out, field, v ? json_copy(v) : json_number(def));
}

/* int(row.get(field) or 0) */
static void sr_pass_int(json_t *out, const char *field, const json_t *row) {
    const json_t *v = row ? json_obj_get(row, field) : NULL;
    json_set(out, field, json_number(sr_int_of(v)));
}

/* _MARKERS, in the order the first non-blank one names the kind. */
static const char *const SR_MARKER_KEYS[3] = {"_delegate_from", "_branched_from", "_reset_from"};
static const char *const SR_MARKER_KINDS[3] = {"delegate", "branch", "reset"};

json_t *w29_sessions_2114c9_p2_session_record(const json_t *req) {
    const json_t *row = sr_row(req, "row");
    const json_t *origin_raw, *tool_names;
    json_t *out, *origin = NULL, *config;
    const char *kind = "none";
    const json_t *origin_id = NULL;
    size_t i;

    config = sr_parse_model_config(row ? json_obj_get(row, "model_config") : NULL);
    out = json_object();

    sr_pass(out, "session_id", row ? json_obj_get(row, "id") : NULL);
    sr_pass_or(out, "source", row ? json_obj_get(row, "source") : NULL, "");
    sr_pass(out, "created_source", row ? json_obj_get(row, "created_source") : NULL);
    sr_pass(out, "user_id", row ? json_obj_get(row, "user_id") : NULL);
    sr_pass(out, "session_key", row ? json_obj_get(row, "session_key") : NULL);
    sr_pass(out, "chat_id", row ? json_obj_get(row, "chat_id") : NULL);
    sr_pass(out, "chat_type", row ? json_obj_get(row, "chat_type") : NULL);
    sr_pass(out, "thread_id", row ? json_obj_get(row, "thread_id") : NULL);
    sr_pass(out, "display_name", row ? json_obj_get(row, "display_name") : NULL);

    origin_raw = row ? json_obj_get(row, "origin_json") : NULL;
    if (sr_is_str(origin_raw) && origin_raw->str_val[0]) {
        json_t *parsed = json_parse(origin_raw->str_val, NULL);
        if (parsed) {
            if (parsed->type == JSON_OBJECT) origin = parsed;
            else json_free(parsed);
        }
    }
    json_set(out, "origin", origin ? origin : json_null());

    json_set(out, "expiry_finalized",
             json_bool(sr_bool_of(row ? json_obj_get(row, "expiry_finalized") : NULL)));
    sr_pass(out, "model", row ? json_obj_get(row, "model") : NULL);
    json_set(out, "model_config", config);
    sr_pass(out, "system_prompt", row ? json_obj_get(row, "system_prompt") : NULL);
    sr_pass(out, "system_prompt_hash", row ? json_obj_get(row, "system_prompt_hash") : NULL);
    sr_pass(out, "parent_session_id", row ? json_obj_get(row, "parent_session_id") : NULL);

    for (i = 0; i < 3; i++) {
        const json_t *cand = json_obj_get(config, SR_MARKER_KEYS[i]);
        size_t b, e;
        if (sr_is_str(cand) && sr_strip_bounds(cand->str_val, &b, &e)) {
            kind = SR_MARKER_KINDS[i];
            origin_id = cand;
            break;
        }
    }
    json_set(out, "lineage_kind", json_string(kind));
    json_set(out, "reset_from", sr_stripped_json(json_obj_get(config, "_reset_from")));
    json_set(out, "branched_from", sr_stripped_json(json_obj_get(config, "_branched_from")));

    json_set(out, "started_at",
             sr_instant_or_floor(row ? json_obj_get(row, "started_at") : NULL));
    json_set(out, "ended_at", sr_instant_or_null(row ? json_obj_get(row, "ended_at") : NULL));
    sr_pass(out, "end_reason", row ? json_obj_get(row, "end_reason") : NULL);

    sr_pass_default(out, "message_count", row, 0);
    sr_pass_default(out, "tool_call_count", row, 0);
    sr_pass_default(out, "input_tokens", row, 0);
    sr_pass_default(out, "output_tokens", row, 0);
    sr_pass_default(out, "cache_read_tokens", row, 0);
    sr_pass_default(out, "cache_write_tokens", row, 0);
    sr_pass_default(out, "reasoning_tokens", row, 0);
    sr_pass_default(out, "api_call_count", row, 0);

    sr_pass(out, "estimated_cost_usd", row ? json_obj_get(row, "estimated_cost_usd") : NULL);
    sr_pass(out, "actual_cost_usd", row ? json_obj_get(row, "actual_cost_usd") : NULL);
    sr_pass(out, "billing_provider", row ? json_obj_get(row, "billing_provider") : NULL);
    sr_pass(out, "billing_base_url", row ? json_obj_get(row, "billing_base_url") : NULL);
    sr_pass(out, "billing_mode", row ? json_obj_get(row, "billing_mode") : NULL);
    sr_pass(out, "cost_status", row ? json_obj_get(row, "cost_status") : NULL);
    sr_pass(out, "cost_source", row ? json_obj_get(row, "cost_source") : NULL);
    sr_pass(out, "pricing_version", row ? json_obj_get(row, "pricing_version") : NULL);
    sr_pass(out, "cwd", row ? json_obj_get(row, "cwd") : NULL);
    sr_pass(out, "git_branch", row ? json_obj_get(row, "git_branch") : NULL);
    sr_pass(out, "git_repo_root", row ? json_obj_get(row, "git_repo_root") : NULL);
    sr_pass_int(out, "git_metadata_generation", row);

    sr_pass(out, "title", row ? json_obj_get(row, "title") : NULL);
    {
        const json_t *ts = row ? json_obj_get(row, "title_source") : NULL;
        const char *mapped = NULL;
        if (sr_is_str(ts)) {
            if (!strcmp(ts->str_val, "derived")) mapped = "derived";
            else if (!strcmp(ts->str_val, "llm")) mapped = "llm";
            else if (!strcmp(ts->str_val, "user")) mapped = "user";
        }
        json_set(out, "title_source", mapped ? json_string(mapped) : json_null());
    }
    sr_pass(out, "last_activity_at", row ? json_obj_get(row, "last_activity_at") : NULL);
    sr_pass(out, "last_activity_description",
            row ? json_obj_get(row, "last_activity_description") : NULL);
    sr_pass(out, "last_activity_provenance",
            row ? json_obj_get(row, "last_activity_provenance") : NULL);
    sr_pass(out, "handoff_state", row ? json_obj_get(row, "handoff_state") : NULL);
    sr_pass(out, "handoff_platform", row ? json_obj_get(row, "handoff_platform") : NULL);
    sr_pass(out, "handoff_error", row ? json_obj_get(row, "handoff_error") : NULL);
    sr_pass(out, "compression_failure_cooldown_until",
            row ? json_obj_get(row, "compression_failure_cooldown_until") : NULL);
    sr_pass(out, "compression_failure_error",
            row ? json_obj_get(row, "compression_failure_error") : NULL);
    sr_pass_int(out, "compression_fallback_streak", row);
    sr_pass_int(out, "compression_ineffective_count", row);
    sr_pass(out, "compression_recovery_deadline",
            row ? json_obj_get(row, "compression_recovery_deadline") : NULL);
    sr_pass_int(out, "compression_overload_streak", row);
    sr_pass(out, "profile_name", row ? json_obj_get(row, "profile_name") : NULL);
    sr_pass(out, "transport_profile", row ? json_obj_get(row, "transport_profile") : NULL);
    sr_pass_int(out, "rewind_count", row);

    json_set(out, "archived", json_bool(sr_bool_of(row ? json_obj_get(row, "archived") : NULL)));
    json_set(out, "auto_archived",
             json_bool(sr_bool_of(row ? json_obj_get(row, "auto_archived") : NULL)));
    json_set(out, "pinned", json_bool(sr_bool_of(row ? json_obj_get(row, "pinned") : NULL)));
    json_set(out, "hidden", json_bool(sr_bool_of(row ? json_obj_get(row, "hidden") : NULL)));
    sr_pass(out, "last_read_at", row ? json_obj_get(row, "last_read_at") : NULL);

    tool_names = row ? json_obj_get(row, "tool_names") : NULL;
    {
        json_t *pin_req = json_object();
        json_set(pin_req, "text", sr_copy(tool_names));
        json_set(out, "tool_pin", w29_sessions_2114c9_p1_tool_pin(pin_req));
        json_free(pin_req);
    }

    sr_pass_int(out, "transcript_revision", row);
    sr_pass_int(out, "runtime_revision", row);
    sr_pass_int(out, "runtime_generation", row);
    json_set(out, "lineage_origin_id", origin_id ? json_copy(origin_id) : json_null());
    json_set(out, "model_config_corrupt",
             json_bool(sr_model_config_corrupt(row ? json_obj_get(row, "model_config") : NULL)));
    sr_pass(out, "model_config_json", row ? json_obj_get(row, "model_config") : NULL);
    sr_pass(out, "origin_json", origin_raw);
    sr_pass(out, "tool_names", tool_names);
    sr_pass(out, "prompt_cache_clock", row ? json_obj_get(row, "prompt_cache_clock") : NULL);
    return out;
}

json_t *w29_sessions_2114c9_p2_constants(const json_t *req) {
    json_t *out = json_object();
    json_t *markers = json_array();
    json_t *sources = json_array();
    size_t i;
    (void)req;
    for (i = 0; i < 3; i++) {
        json_t *pair = json_array();
        json_append(pair, json_string(SR_MARKER_KEYS[i]));
        json_append(pair, json_string(SR_MARKER_KINDS[i]));
        json_append(markers, pair);
    }
    json_append(sources, json_string("derived"));
    json_append(sources, json_string("llm"));
    json_append(sources, json_string("user"));
    json_set(out, "markers", markers);
    json_set(out, "title_sources", sources);
    json_set(out, "canonical_title_locked",
             json_string("This is the bot's canonical Bot Chat"));
    json_set(out, "prompt_hash_pattern", json_string("[0-9a-f]{64}"));
    json_set(out, "untrusted_instant", json_number(0.0));
    json_set(out, "epoch_min", json_number(0.0));
    json_set(out, "epoch_max", json_number(4200000000.0));
    return out;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Gate p3 - statement / SQL fragment builders and the title refusal predicate
 * ══════════════════════════════════════════════════════════════════════════ */

static const char *const SR_RESET_END_REASONS[6] = {
    "session_reset", "session_switch", "idle", "daily", "suspended", "resume_pending_expired",
};

/* INTERNAL_LISTING_SOURCES, in declaration order. */
static const char *const SR_INTERNAL_SOURCES[3] = {"kanban", "tool", "oneshot"};

#define SR_LOCAL_CHAT \
    "(chat_id LIKE 'local-%' OR EXISTS (SELECT 1 FROM state_meta WHERE key = ? || sessions.chat_id))"

#define SR_LEGACY_PREFIX "gateway.local_legacy.v1:"

static void sb_literal(sr_sb *b, const char *text) {
    const char *p;
    sb_addc(b, '\'');
    for (p = text; p && *p; p++) {
        if (*p == '\'') sb_addc(b, '\'');
        sb_addc(b, *p);
    }
    sb_addc(b, '\'');
}

static void sb_json_extract(sr_sb *b, const char *expr, const char *path) {
    sb_add(b, "json_extract((CASE WHEN json_valid(");
    sb_add(b, expr);
    sb_add(b, ") THEN ");
    sb_add(b, expr);
    sb_add(b, " ELSE json_object() END), ");
    sb_literal(b, path);
    sb_addc(b, ')');
}

static void sb_legacy_reset_child(sr_sb *b, const char *alias, const char *reasons_sql) {
    sb_add(b, "EXISTS (SELECT 1 FROM sessions p            WHERE p.id = ");
    sb_add(b, alias);
    sb_add(b, ".parent_session_id            AND p.end_reason IN (");
    sb_add(b, reasons_sql);
    sb_add(b, ")            AND ");
    sb_add(b, alias);
    sb_add(b, ".session_key IS NOT NULL            AND ");
    sb_add(b, alias);
    sb_add(b, ".session_key != ''            AND ");
    sb_add(b, alias);
    sb_add(b, ".session_key = p.session_key)");
}

static void sb_reasons_sql(sr_sb *b) {
    size_t i;
    for (i = 0; i < 6; i++) {
        if (i) sb_add(b, ", ");
        sb_literal(b, SR_RESET_END_REASONS[i]);
    }
}

static void sb_reset_child(sr_sb *b, const char *alias) {
    char expr[160];
    sr_sb reasons;
    snprintf(expr, sizeof expr, "%s.model_config", alias);
    sb_json_extract(b, expr, "$._reset_from");
    sb_add(b, " IS NOT NULL OR ");
    sb_init(&reasons);
    sb_reasons_sql(&reasons);
    sb_legacy_reset_child(b, alias, reasons.p ? reasons.p : "");
    free(reasons.p);
}

static void sb_hidden_sources(sr_sb *b) {
    size_t i;
    for (i = 0; i < 3; i++) {
        if (i) sb_addc(b, ',');
        sb_addc(b, '?');
    }
}

static void sb_freshest_of(sr_sb *b, const char *activity, const char *session_id_expr,
                           const char *started) {
    sb_add(b, "COALESCE((SELECT MAX(_act_v.v) FROM (SELECT ");
    sb_add(b, activity);
    sb_add(b, " AS v UNION ALL SELECT (SELECT MAX(_act_m.timestamp) FROM messages _act_m "
              "INDEXED BY idx_messages_session WHERE _act_m.session_id = ");
    sb_add(b, session_id_expr);
    sb_add(b, " AND _act_m.timestamp BETWEEN 0.0 AND 4200000000.0)) _act_v WHERE "
              "_act_v.v BETWEEN 0.0 AND 4200000000.0), (SELECT _win.v FROM (SELECT ");
    sb_add(b, started);
    sb_add(b, " AS v) _win WHERE _win.v BETWEEN 0.0 AND 4200000000.0))");
}

static void sb_session_last_active(sr_sb *b, const char *alias) {
    char act[160], sid[160], started[160];
    snprintf(act, sizeof act, "%s.last_activity_at", alias);
    snprintf(sid, sizeof sid, "%s.id", alias);
    snprintf(started, sizeof started, "%s.started_at", alias);
    sb_freshest_of(b, act, sid, started);
}

static const char *sr_arg_str(const json_t *req, const char *key, const char *def) {
    const json_t *v = sr_row(req, key);
    return sr_is_str(v) ? v->str_val : def;
}

json_t *w29_sessions_2114c9_p3_sql_literal(const json_t *req) {
    sr_sb b;
    sb_init(&b);
    sb_literal(&b, sr_arg_str(req, "text", ""));
    return sb_take(&b);
}

json_t *w29_sessions_2114c9_p3_json_extract(const json_t *req) {
    sr_sb b;
    sb_init(&b);
    sb_json_extract(&b, sr_arg_str(req, "expression", ""), sr_arg_str(req, "path", ""));
    return sb_take(&b);
}

json_t *w29_sessions_2114c9_p3_legacy_reset_child(const json_t *req) {
    sr_sb b;
    sb_init(&b);
    sb_legacy_reset_child(&b, sr_arg_str(req, "alias", ""), sr_arg_str(req, "reasons_sql", ""));
    return sb_take(&b);
}

json_t *w29_sessions_2114c9_p3_reset_child(const json_t *req) {
    sr_sb b;
    sb_init(&b);
    sb_reset_child(&b, sr_arg_str(req, "alias", "s"));
    return sb_take(&b);
}

json_t *w29_sessions_2114c9_p3_descendants(const json_t *req) {
    sr_sb b;
    (void)req;
    sb_init(&b);
    sb_add(&b,
           "\n            WITH RECURSIVE descendants(id, parent_session_id, started_at) AS (\n"
           "                SELECT id, parent_session_id, started_at FROM sessions WHERE id = ?\n"
           "                UNION\n"
           "                SELECT s.id, s.parent_session_id, s.started_at\n"
           "                FROM sessions s\n"
           "                JOIN descendants d ON s.parent_session_id = d.id\n"
           "                -- Continuation edges only (same predicate as the session list's chain CTE): a subagent run,\n"
           "                -- a /branch fork, a /new reset child or a tool-owned row is its own conversation, and resuming\n"
           "                -- INTO one parks the user's chat in a row the sidebar never lists (#115092).\n"
           "                WHERE ");
    sb_json_extract(&b, "s.model_config", "$._delegate_from");
    sb_add(&b, " IS NULL\n                  AND ");
    sb_json_extract(&b, "s.model_config", "$._branched_from");
    sb_add(&b, " IS NULL\n                  AND NOT (");
    sb_reset_child(&b, "s");
    sb_add(&b, ")\n                  AND COALESCE(s.source, '') != 'tool'\n"
               "            )\n"
               "            SELECT id, parent_session_id, started_at FROM descendants\n"
               "            ");
    return sb_take(&b);
}

json_t *w29_sessions_2114c9_p3_session_last_active(const json_t *req) {
    sr_sb b;
    sb_init(&b);
    sb_session_last_active(&b, sr_arg_str(req, "alias", "s"));
    return sb_take(&b);
}

json_t *w29_sessions_2114c9_p3_local_chat(const json_t *req) {
    (void)req;
    return json_string(SR_LOCAL_CHAT);
}

json_t *w29_sessions_2114c9_p3_owner_filter(const json_t *req) {
    json_t *out = json_object();
    json_t *params = json_array();
    if (sr_truthy(sr_row(req, "operator"))) {
        json_set(out, "sql", json_string(""));
    } else {
        json_set(out, "sql", json_string("user_id=? AND "));
        json_append(params, sr_copy(sr_row(req, "subject")));
    }
    json_set(out, "params", params);
    return out;
}

json_t *w29_sessions_2114c9_p3_roster_sqls(const json_t *req) {
    json_t *out = json_object();
    json_t *filt = w29_sessions_2114c9_p3_owner_filter(req);
    const char *sql = json_get_str(filt, "sql", "");
    sr_sb b;

    sb_init(&b);
    sb_add(&b, "SELECT * FROM sessions WHERE title=? AND ");
    sb_add(&b, SR_LOCAL_CHAT);
    json_set(out, "canonical", sb_take(&b));

    sb_init(&b);
    sb_add(&b, "SELECT * FROM sessions WHERE ");
    sb_add(&b, sql);
    sb_add(&b, SR_LOCAL_CHAT);
    sb_add(&b, " AND archived=0 AND hidden=0 AND COALESCE(source,'') NOT IN (");
    sb_hidden_sources(&b);
    sb_add(&b, ") ORDER BY COALESCE(last_activity_at,started_at) DESC LIMIT 1");
    json_set(out, "latest", sb_take(&b));

    sb_init(&b);
    sb_add(&b, "SELECT id, source, title, COALESCE(last_activity_at, started_at) AS last_active "
               "FROM sessions WHERE COALESCE(source,'') IN (");
    sb_hidden_sources(&b);
    sb_add(&b, ") AND archived=0 AND hidden=0 ORDER BY last_active DESC LIMIT 1");
    json_set(out, "worker", sb_take(&b));

    json_free(filt);
    return out;
}

json_t *w29_sessions_2114c9_p3_last_active_select(const json_t *req) {
    sr_sb b;
    (void)req;
    sb_init(&b);
    sb_add(&b, "SELECT ");
    sb_session_last_active(&b, "s");
    sb_add(&b, " FROM sessions s WHERE s.id = ?");
    return sb_take(&b);
}

/* _title_refusals: the store's ValueError text -> conflict / canonical / pass. */
json_t *w29_sessions_2114c9_p3_title_refusal(const json_t *req) {
    const json_t *textv = sr_row(req, "text");
    const char *text = sr_is_str(textv) ? textv->str_val : NULL;
    static const char HEAD[] = "Title '";
    static const char DELIM[] = "' is already in use by session ";
    static const char LOCKED[] = "This is the bot's canonical Bot Chat";
    const size_t hlen = sizeof HEAD - 1, dlen = sizeof DELIM - 1;
    json_t *out;
    size_t n, idx, best;
    if (!text) return json_string("pass");
    n = strlen(text);
    if (strncmp(text, HEAD, hlen)) {
        if (n >= sizeof LOCKED - 1 && !memcmp(text, LOCKED, sizeof LOCKED - 1))
            return json_string("canonical");
        return json_string("pass");
    }
    /* DOTALL greedy title: the LAST delimiter that leaves a non-empty holder. */
    best = (size_t)-1;
    if (n >= dlen + hlen + 1) {
        for (idx = n - dlen - 1; ; idx--) {
            if (!memcmp(text + idx, DELIM, dlen)) {
                best = idx;
                break;
            }
            if (idx == hlen) break;
        }
    }
    if (best == (size_t)-1) return json_string("pass");
    out = json_object();
    json_set(out, "kind", json_string("conflict"));
    json_set(out, "title", sr_str_n(text + hlen, best - hlen));
    json_set(out, "holder", sr_str_n(text + best + dlen, n - best - dlen));
    return out;
}

json_t *w29_sessions_2114c9_p3_fragments(const json_t *req) {
    json_t *out = json_object();
    json_t *reasons = json_array();
    json_t *sources = json_array();
    json_t *window = json_array();
    sr_sb b;
    size_t i;
    (void)req;
    sb_init(&b);
    sb_hidden_sources(&b);
    json_set(out, "hidden_sources", sb_take(&b));
    for (i = 0; i < 6; i++) json_append(reasons, json_string(SR_RESET_END_REASONS[i]));
    json_set(out, "reset_end_reasons", reasons);
    for (i = 0; i < 3; i++) json_append(sources, json_string(SR_INTERNAL_SOURCES[i]));
    json_set(out, "internal_sources", sources);
    json_set(out, "legacy_prefix", json_string(SR_LEGACY_PREFIX));
    json_append(window, json_number(0.0));
    json_append(window, json_number(4200000000.0));
    json_set(out, "epoch_window", window);
    return out;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Gate p4 - listing / recency / roster envelopes
 * ══════════════════════════════════════════════════════════════════════════ */

/* session_unread(row): activity postdates the last_read_at watermark (NULL = read). */
static int sr_session_unread(const json_t *row) {
    const json_t *last_read = row ? json_obj_get(row, "last_read_at") : NULL;
    const json_t *act;
    double lr = 0.0, la = 0.0;
    if (!last_read || last_read->type == JSON_NULL) return 0;
    if (!sr_float_of(last_read, &lr)) return 0;
    act = json_obj_get(row, "last_active");
    if (!sr_truthy(act)) act = json_obj_get(row, "started_at");
    if (sr_truthy(act) && !sr_float_of(act, &la)) la = 0.0;
    return la > lr;
}

json_t *w29_sessions_2114c9_p4_list_entry(const json_t *req) {
    const json_t *row = sr_row(req, "row");
    const json_t *lineage = row ? json_obj_get(row, "_lineage_ids") : NULL;
    const json_t *unread = row ? json_obj_get(row, "unread") : NULL;
    json_t *out = json_object();
    json_t *ids = json_array();
    size_t i;
    json_set(out, "session", w29_sessions_2114c9_p2_session_record(req));
    sr_pass_or(out, "preview", row ? json_obj_get(row, "preview") : NULL, "");
    if (unread) json_set(out, "unread", json_bool(sr_bool_of(unread)));
    else json_set(out, "unread", json_bool(sr_session_unread(row)));
    json_set(out, "last_active", w29_sessions_2114c9_p1_last_active(req));
    sr_pass(out, "lineage_root_id", row ? json_obj_get(row, "_lineage_root_id") : NULL);
    if (lineage && lineage->type == JSON_ARRAY) {
        for (i = 0; i < json_len(lineage); i++)
            json_append(ids, sr_copy(json_get(lineage, i)));
    }
    json_set(out, "lineage_ids", ids);
    sr_pass(out, "continuation_kind", row ? json_obj_get(row, "continuation_kind") : NULL);
    json_set(out, "compact", json_bool(sr_truthy(sr_row(req, "compact"))));
    return out;
}

json_t *w29_sessions_2114c9_p4_recency_entry(const json_t *req) {
    json_t *out = json_object();
    json_set(out, "session", w29_sessions_2114c9_p2_session_record(req));
    json_set(out, "last_active", w29_sessions_2114c9_p1_last_active(req));
    return out;
}

/* _roster_session(fields): RosterSession(**fields, id/resolved_id as SessionId). */
json_t *w29_sessions_2114c9_p4_roster_session(const json_t *req) {
    const json_t *fields = sr_row(req, "fields");
    static const char *const ORDER[8] = {"id", "resolved_id", "title", "root_title",
                                         "preview", "started_at", "last_active", "message_count"};
    json_t *out;
    size_t i;
    if (!fields || fields->type == JSON_NULL) return json_null();
    out = json_object();
    for (i = 0; i < 8; i++) sr_pass(out, ORDER[i], json_obj_get(fields, ORDER[i]));
    return out;
}

/* ' '.join(str(text or '').split()) with the 80-char preview cut. */
static json_t *sr_joined_text(const json_t *textv) {
    const char *s = sr_is_str(textv) ? textv->str_val : NULL;
    sr_sb b;
    size_t i = 0, n, cut;
    char *tmp;
    json_t *out;
    sb_init(&b);
    if (s) {
        while (s[i]) {
            size_t start;
            while (s[i] && sr_ws(s[i])) i++;
            if (!s[i]) break;
            start = i;
            while (s[i] && !sr_ws(s[i])) i++;
            if (b.n) sb_addc(&b, ' ');
            sb_addn(&b, s + start, i - start);
        }
    }
    n = b.p ? b.n : 0;
    cut = n > 80 ? 80 : n;
    tmp = (char *)malloc(cut + 4);
    if (!tmp) { free(b.p); return json_string(""); }
    if (n) memcpy(tmp, b.p, cut);
    if (n > 80) { memcpy(tmp + cut, "...", 3); tmp[cut + 3] = '\0'; }
    else tmp[cut] = '\0';
    out = json_string(tmp);
    free(tmp);
    free(b.p);
    return out;
}

/* _read_bot_sessions.summary(root, target, tip, text) */
json_t *w29_sessions_2114c9_p4_roster_summary(const json_t *req) {
    const json_t *root = sr_row(req, "root");
    const json_t *target = sr_row(req, "target");
    const json_t *started = target ? json_obj_get(target, "started_at") : NULL;
    const json_t *activity = target ? json_obj_get(target, "last_activity_at") : NULL;
    const json_t *count = target ? json_obj_get(target, "message_count") : NULL;
    json_t *out = json_object();
    sr_pass(out, "id", root ? json_obj_get(root, "id") : NULL);
    sr_pass(out, "resolved_id", sr_row(req, "tip"));
    sr_pass_or(out, "title", target ? json_obj_get(target, "title") : NULL, "");
    sr_pass_or(out, "root_title", root ? json_obj_get(root, "title") : NULL, "");
    json_set(out, "preview", sr_joined_text(sr_row(req, "text")));
    json_set(out, "started_at", sr_truthy(started) ? json_copy(started) : json_number(0));
    if (sr_truthy(activity)) json_set(out, "last_active", json_copy(activity));
    else if (sr_truthy(started)) json_set(out, "last_active", json_copy(started));
    else json_set(out, "last_active", json_number(0));
    json_set(out, "message_count", sr_truthy(count) ? json_copy(count) : json_number(0));
    return out;
}

/* fields['last_session']: the canonical copy when the newest row IS the canonical one. */
json_t *w29_sessions_2114c9_p4_roster_last_session(const json_t *req) {
    const json_t *canonical = sr_row(req, "canonical");
    const json_t *latest = sr_row(req, "latest");
    int has_canonical = canonical && canonical->type == JSON_OBJECT;
    int has_latest = latest && latest->type == JSON_OBJECT;
    if (has_latest && has_canonical) {
        char *ka = sr_key(json_obj_get(canonical, "id"));
        char *kb = sr_key(json_obj_get(latest, "id"));
        int same = ka && kb && !strcmp(ka, kb);
        free(ka);
        free(kb);
        if (same) return json_copy(canonical);
    }
    if (has_latest) return json_copy(latest);
    return json_null();
}

/* SessionListRow shaping of get_session_list_row's row. */
json_t *w29_sessions_2114c9_p4_list_row(const json_t *req) {
    const json_t *row = sr_row(req, "row");
    const json_t *started = row ? json_obj_get(row, "started_at") : NULL;
    json_t *out = json_object();
    double d = 0.0;
    sr_pass(out, "session_id", row ? json_obj_get(row, "id") : NULL);
    sr_pass_or(out, "source", row ? json_obj_get(row, "source") : NULL, "");
    sr_pass(out, "title", row ? json_obj_get(row, "title") : NULL);
    if (sr_truthy(started) && sr_float_of(started, &d))
        json_set(out, "started_at", json_number(d));
    else
        json_set(out, "started_at", json_number(0.0));
    sr_pass(out, "ended_at", row ? json_obj_get(row, "ended_at") : NULL);
    sr_pass(out, "message_count", row ? json_obj_get(row, "message_count") : NULL);
    sr_pass(out, "model", row ? json_obj_get(row, "model") : NULL);
    json_set(out, "hidden", json_bool(sr_bool_of(row ? json_obj_get(row, "hidden") : NULL)));
    json_set(out, "archived", json_bool(sr_bool_of(row ? json_obj_get(row, "archived") : NULL)));
    return out;
}

/* LiveListingRow shaping of the live_listing_facts row. */
json_t *w29_sessions_2114c9_p4_live_listing_row(const json_t *req) {
    const json_t *row = sr_row(req, "row");
    const json_t *started = row ? json_obj_get(row, "started_at") : NULL;
    json_t *out = json_object();
    double d = 0.0;
    sr_pass(out, "id", row ? json_obj_get(row, "id") : NULL);
    sr_pass_or(out, "source", row ? json_obj_get(row, "source") : NULL, "");
    sr_pass(out, "title", row ? json_obj_get(row, "title") : NULL);
    if (sr_truthy(started) && sr_float_of(started, &d))
        json_set(out, "started_at", json_number(d));
    else
        json_set(out, "started_at", json_number(0.0));
    sr_pass(out, "ended_at", row ? json_obj_get(row, "ended_at") : NULL);
    sr_pass(out, "message_count", row ? json_obj_get(row, "message_count") : NULL);
    sr_pass(out, "model", row ? json_obj_get(row, "model") : NULL);
    json_set(out, "hidden", json_bool(sr_bool_of(row ? json_obj_get(row, "hidden") : NULL)));
    json_set(out, "archived", json_bool(sr_bool_of(row ? json_obj_get(row, "archived") : NULL)));
    json_set(out, "runtime_revision",
             json_number(sr_int_of(row ? json_obj_get(row, "runtime_revision") : NULL)));
    json_set(out, "runtime_generation",
             json_number(sr_int_of(row ? json_obj_get(row, "runtime_generation") : NULL)));
    sr_pass(out, "target_ended_at", row ? json_obj_get(row, "target_ended_at") : NULL);
    return out;
}

/* pending admission rows grouped under their target session, seq order kept. */
json_t *w29_sessions_2114c9_p4_pending_by_session(const json_t *req) {
    const json_t *rows = sr_row(req, "rows");
    json_t *out = json_object();
    size_t i;
    if (!rows || rows->type != JSON_ARRAY) return out;
    for (i = 0; i < json_len(rows); i++) {
        const json_t *r = json_get(rows, i);
        const json_t *sid = r ? json_obj_get(r, "target_session_id") : NULL;
        json_t *bucket, *ref;
        char *key = sr_pystr_key(sid);   /* SessionId(None) -> "None" */
        if (!key) continue;
        bucket = json_obj_get(out, key);
        if (!bucket) {
            json_t *fresh = json_array();
            json_set(out, key, fresh);
            bucket = json_obj_get(out, key);
        }
        free(key);
        ref = json_object();
        sr_pass(ref, "admission_id", r ? json_obj_get(r, "admission_id") : NULL);
        json_set(ref, "seq", json_number(sr_int_of(r ? json_obj_get(r, "seq") : NULL)));
        sr_pass(ref, "status", r ? json_obj_get(r, "status") : NULL);
        json_append(bucket, ref);
    }
    return out;
}

/* RecentSessionEntry shaping of list_recent_sessions_bounded's row. */
json_t *w29_sessions_2114c9_p4_recent_entry(const json_t *req) {
    const json_t *row = sr_row(req, "row");
    const json_t *started = row ? json_obj_get(row, "started_at") : NULL;
    json_t *out = json_object();
    double d = 0.0;
    sr_pass(out, "session_id", row ? json_obj_get(row, "id") : NULL);
    sr_pass_or(out, "source", row ? json_obj_get(row, "source") : NULL, "");
    sr_pass(out, "model", row ? json_obj_get(row, "model") : NULL);
    sr_pass(out, "title", row ? json_obj_get(row, "title") : NULL);
    if (sr_truthy(started) && sr_float_of(started, &d))
        json_set(out, "started_at", json_number(d));
    else
        json_set(out, "started_at", json_number(0.0));
    sr_pass(out, "ended_at", row ? json_obj_get(row, "ended_at") : NULL);
    sr_pass(out, "end_reason", row ? json_obj_get(row, "end_reason") : NULL);
    sr_pass(out, "message_count", row ? json_obj_get(row, "message_count") : NULL);
    sr_pass(out, "tool_call_count", row ? json_obj_get(row, "tool_call_count") : NULL);
    json_set(out, "last_active", w29_sessions_2114c9_p1_last_active(req));
    sr_pass_or(out, "preview", row ? json_obj_get(row, "preview") : NULL, "");
    sr_pass(out, "lineage_root_id", row ? json_obj_get(row, "_lineage_root_id") : NULL);
    json_set(out, "unread", json_bool(sr_truthy(row ? json_obj_get(row, "unread") : NULL)));
    return out;
}

/* RosterWorker shaping of the freshest internal-source row. */
json_t *w29_sessions_2114c9_p4_worker_session(const json_t *req) {
    const json_t *row = sr_row(req, "row");
    const json_t *src = row ? json_obj_get(row, "source") : NULL;
    const json_t *la = row ? json_obj_get(row, "last_active") : NULL;
    json_t *out = json_object();
    json_set(out, "id", sr_copy(row ? json_obj_get(row, "id") : NULL));
    if (sr_is_str(src)) {
        size_t b, e;
        if (sr_strip_bounds(src->str_val, &b, &e)) {
            char *tmp = (char *)malloc(e - b + 1);
            size_t i;
            if (!tmp) return json_null();
            for (i = b; i < e; i++) {
                char c = src->str_val[i];
                tmp[i - b] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
            }
            tmp[e - b] = '\0';
            json_set(out, "source", json_string(tmp));
            free(tmp);
        } else {
            json_set(out, "source", json_string(""));
        }
    } else {
        json_set(out, "source", json_string(""));
    }
    sr_pass_or(out, "title", row ? json_obj_get(row, "title") : NULL, "");
    json_set(out, "last_active", sr_truthy(la) ? json_copy(la) : json_number(0));
    return out;
}
