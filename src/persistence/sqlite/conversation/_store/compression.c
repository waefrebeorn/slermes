/*
 * compression.c - C11 port of upstream Hermes
 * persistence/sqlite/conversation/_store/compression.py
 * (gates w31_compressio_220b6d_p1 / w31_compressio_220b6d_p2).
 *
 * PoP: w31_compressio_220b6d_p1_cooldown_row @ persistence/sqlite/conversation/_store/compression.py:_cooldown_row
 * PoP: w31_compressio_220b6d_p1_walk_chain @ persistence/sqlite/conversation/_store/compression.py:_walk_compression_chain
 * PoP: w31_compressio_220b6d_p1_chain_tip @ persistence/sqlite/conversation/_store/compression.py:get_compression_tip
 * PoP: w31_compressio_220b6d_p1_unset_created @ persistence/sqlite/conversation/_store/compression.py:_UNSET_CREATED_SOURCES
 * PoP: w31_compressio_220b6d_p1_fork_child @ persistence/sqlite/conversation/_store/messages.py:_is_explicit_fork_child_row
 * PoP: w31_compressio_220b6d_p1_compression_child @ persistence/sqlite/conversation/_store/compression.py:_is_compression_child_row
 * PoP: w31_compressio_220b6d_p1_lineage @ persistence/sqlite/conversation/_store/compression.py:get_compression_lineage
 * PoP: w31_compressio_220b6d_p1_tool_root @ persistence/sqlite/conversation/_store/compression.py:get_tool_rotation_root
 * PoP: w31_compressio_220b6d_p1_lease_key @ persistence/sqlite/conversation/_store/compression.py:_session_turn_lease_key_on_conn
 * PoP: w31_compressio_220b6d_p1_cooldown_active @ persistence/sqlite/conversation/_store/compression.py:get_compression_failure_cooldown
 * PoP: w31_compressio_220b6d_p1_stamp_ancestors @ persistence/sqlite/conversation/_store/compression.py:stamp_compression_ancestors
 * PoP: w31_compressio_220b6d_p1_child_row @ persistence/sqlite/conversation/_store/compression.py:_publish_child_session_row
 * PoP: w31_compressio_220b6d_p2_chain_step_sql @ persistence/sqlite/conversation/_store/compression.py:_CHAIN_STEP_SQL
 * PoP: w31_compressio_220b6d_p2_unset_ancestor_sql @ persistence/sqlite/conversation/_store/compression.py:_UNSET_ANCESTOR_SQL
 * PoP: w31_compressio_220b6d_p2_unstamped_edge_sql @ persistence/sqlite/conversation/_store/compression.py:_UNSTAMPED_EDGE_CHILD_SQL
 * PoP: w31_compressio_220b6d_p2_record_cooldown @ persistence/sqlite/conversation/_store/compression.py:record_compression_failure_cooldown
 * PoP: w31_compressio_220b6d_p2_clear_cooldown @ persistence/sqlite/conversation/_store/compression.py:clear_compression_failure_cooldown
 * PoP: w31_compressio_220b6d_p2_finalize_orphans @ persistence/sqlite/conversation/_store/compression.py:finalize_orphaned_compression_sessions
 * PoP: w31_compressio_220b6d_p2_read_number @ persistence/sqlite/conversation/_store/compression.py:_read_session_number
 * PoP: w31_compressio_220b6d_p2_streak_write @ persistence/sqlite/conversation/_store/compression.py:set_compression_fallback_streak
 * PoP: w31_compressio_220b6d_p2_deadline_write @ persistence/sqlite/conversation/_store/compression.py:set_compression_recovery_deadline
 * PoP: w31_compressio_220b6d_p2_overload_bump @ persistence/sqlite/conversation/_store/compression.py:increment_compression_overload_streak
 * PoP: w31_compressio_220b6d_p2_parent_gate @ persistence/sqlite/conversation/_store/compression.py:_publish_compression_child_on_conn
 *
 * Seams (answers arrive as inputs, never as I/O here): the read connection and
 * its rows (get_session / _read_one / _read_all / _read_ctx), the write
 * connection and its rowcounts, every UPDATE's bound values,
 * created_surface(), _parse_model_config(), _own_profile_name(),
 * _store_system_prompt(), is_automatic_end_reason() and the clock time.time().
 *
 * This is the single port body for the module; the two gate headers
 * noc_w31_compressio_220b6d_p1.h / _p2.h declare its two halves.
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"
#include "noc_w31_compressio_220b6d_p1.h"
#include "noc_w31_compressio_220b6d_p2.h"

/* ------------------------------------------------------------------ values */

/* Python truthiness of a JSON value. */
static int zc_truthy(const json_t *v) {
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

static const char *zc_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

/* row[key] when it is a string, else "" (what a sqlite TEXT column reads as). */
static const char *zc_field(const json_t *obj, const char *key) {
    const char *s = zc_str(json_obj_get(obj, key));
    return s ? s : "";
}

/* Python's numeric view: int, float or bool; None is none of those. */
static int zc_num(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

static int zc_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int zc_is_blank(const char *s) {
    if (!s) return 1;
    while (*s) {
        if (!zc_space(*s)) return 0;
        s++;
    }
    return 1;
}

/* Python int(str): surrounding whitespace, optional sign, digits with single
 * underscores between them.  Returns 0 where int() would raise ValueError. */
static int zc_int_str(const char *s, long long *out) {
    const char *p = s;
    long long v = 0;
    int neg = 0, digits = 0, prev_us = 0;
    if (!p) return 0;
    while (zc_space(*p)) p++;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); digits++; prev_us = 0; }
        else if (*p == '_' && digits && !prev_us) prev_us = 1;
        else break;
    }
    if (prev_us || !digits) return 0;
    while (zc_space(*p)) p++;
    if (*p) return 0;
    *out = neg ? -v : v;
    return 1;
}

/* Python float(value): numbers/bools as-is, strings via strtod after trimming
 * (underscores between digits allowed, inf/nan spellings accepted). */
static int zc_float_of(const json_t *v, double *out) {
    char buf[512], low[64];
    char *end = NULL, *t;
    size_t i = 0, j = 0, k = 0;
    if (zc_num(v, out)) return 1;
    if (!v || v->type != JSON_STRING || !v->str_val) return 0;
    while (v->str_val[i] && j + 1 < sizeof buf) {
        char c = v->str_val[i];
        if (c == '_' && j > 0 && v->str_val[i + 1]) { i++; continue; }
        buf[j++] = c;
        i++;
    }
    buf[j] = '\0';
    t = buf;
    while (zc_space(*t)) t++;
    if (zc_is_blank(t)) return 0;
    *out = strtod(t, &end);
    if (end == t) {
        while (t[k] && k + 1 < sizeof low) { low[k] = (char)((t[k] >= 'A' && t[k] <= 'Z') ? t[k] + 32 : t[k]); k++; }
        low[k] = '\0';
        if (!strcmp(low, "inf") || !strcmp(low, "+inf") || !strcmp(low, "infinity") || !strcmp(low, "+infinity"))
            { *out = 1.0 / 0.0; return 1; }
        if (!strcmp(low, "-inf") || !strcmp(low, "-infinity")) { *out = -1.0 / 0.0; return 1; }
        if (!strcmp(low, "nan") || !strcmp(low, "+nan") || !strcmp(low, "-nan")) { *out = 0.0 / 0.0; return 1; }
        return 0;
    }
    while (zc_space(*end)) end++;
    if (*end) return 0;
    return 1;
}

static int zc_int_of(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_STRING) return zc_int_str(v->str_val, out);
    return 0;
}

/* Python '==' over the shapes a lineage marker can hold. */
static int zc_eq(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (zc_num(a, &x) && zc_num(b, &y)) return x == y;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return 0;
}

static char *zc_dup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static int zc_seen(char **seen, size_t n, const char *id) {
    size_t i;
    if (!id) return 0;
    for (i = 0; i < n; i++)
        if (seen[i] && !strcmp(seen[i], id)) return 1;
    return 0;
}

static void zc_seen_free(char **seen, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) free(seen[i]);
    free(seen);
}

static int zc_grow(char ***seen, size_t *cap) {
    size_t ncap = (*cap) * 2;
    char **grown = (char **)realloc(*seen, ncap * sizeof(char *));
    if (!grown) return 0;
    *seen = grown;
    *cap = ncap;
    return 1;
}

/* ``_UNSET_CREATED_SOURCES``: None, "", "unknown", "local". */
static int zc_unset_source(const json_t *v) {
    const char *s;
    if (!v || v->type == JSON_NULL) return 1;
    s = zc_str(v);
    if (!s) return 0;
    return !strcmp(s, "") || !strcmp(s, "unknown") || !strcmp(s, "local");
}

/* Tolerant model_config decode (_parse_model_config): JSON text or object ->
 * object copy, anything else -> empty object. */
static json_t *zc_parse_cfg(const char *raw) {
    json_t *parsed, *out;
    size_t i;
    if (raw == NULL || zc_is_blank(raw)) return json_object();
    parsed = json_parse(raw, NULL);
    if (!parsed || parsed->type != JSON_OBJECT) {
        if (parsed) json_free(parsed);
        return json_object();
    }
    out = json_object();
    for (i = 0; i < json_object_size(parsed); i++) {
        const char *k = json_object_get_key_at(parsed, i);
        if (k) json_set(out, k, json_copy(json_object_get_at(parsed, i)));
    }
    json_free(parsed);
    return out;
}

static json_t *zc_cfg_of(const json_t *raw) {
    if (!raw || raw->type == JSON_NULL) return json_object();
    if (raw->type == JSON_STRING) return zc_parse_cfg(raw->str_val);
    if (raw->type == JSON_OBJECT) return json_copy(raw);
    return json_object();
}

static const json_t *zc_row_at(const json_t *rows, const char *id) {
    if (!rows || rows->type != JSON_OBJECT || !id) return NULL;
    return json_obj_get(rows, id);
}

static const char *zc_row_id(const json_t *row) {
    return (row && row->type == JSON_OBJECT) ? zc_str(json_obj_get(row, "id")) : NULL;
}

static int zc_is_compression_end(const json_t *row) {
    return (row && row->type == JSON_OBJECT) && !strcmp(zc_field(row, "end_reason"), "compression");
}

/* ------------------------------------------------------------- predicates */

/* ``_is_explicit_fork_child_row``. */
static int zc_fork_child(const json_t *session, int include_reset) {
    json_t *cfg;
    const json_t *markers[3];
    const char *parent_id;
    size_t n = 0, i;
    int hit = 0;

    if (!session || session->type != JSON_OBJECT) return 0;
    if (!strcmp(zc_field(session, "source"), "tool")) return 1;
    cfg = zc_cfg_of(json_obj_get(session, "model_config"));
    markers[n++] = json_obj_get(cfg, "_branched_from");
    markers[n++] = json_obj_get(cfg, "_delegate_from");
    if (include_reset) markers[n++] = json_obj_get(cfg, "_reset_from");
    parent_id = zc_str(json_obj_get(session, "parent_session_id"));
    if (parent_id && *parent_id) {
        json_t *pid = json_string(parent_id);
        for (i = 0; i < n; i++)
            if (!hit && zc_eq(markers[i], pid)) hit = 1;
        json_free(pid);
    } else {
        for (i = 0; i < n; i++)
            if (markers[i] && markers[i]->type != JSON_NULL) hit = 1;
    }
    json_free(cfg);
    return hit;
}

/* ``_is_compression_child_row``. */
static int zc_compression_child(const json_t *child, const json_t *sessions) {
    const char *parent_id;
    const json_t *parent;
    if (!child || child->type != JSON_OBJECT) return 0;
    parent_id = zc_str(json_obj_get(child, "parent_session_id"));
    if (!parent_id || !*parent_id) return 0;
    if (zc_fork_child(child, 1)) return 0;
    parent = zc_row_at(sessions, parent_id);
    return zc_is_compression_end(parent);
}

/* ``_walk_compression_chain``. */
static json_t *zc_walk(const char *session_id, const json_t *steps, int cap) {
    json_t *chain = json_array();
    size_t capseen, nseen = 0;
    char **seen;
    const char *cur = session_id;
    int i;

    capseen = (size_t)(cap > 0 ? cap : 0) + 2;
    seen = (char **)calloc(capseen, sizeof(char *));
    if (!seen) return chain;
    if (session_id && *session_id) {
        seen[nseen] = zc_dup(session_id);
        if (seen[nseen]) nseen++;
        json_append(chain, json_string(session_id));
    }
    for (i = 0; i < cap; i++) {
        const json_t *row = (steps && steps->type == JSON_OBJECT && cur) ? json_obj_get(steps, cur) : NULL;
        const char *child = zc_str(row);
        if (!child || !*child) break;
        if (zc_seen(seen, nseen, child)) break;
        if (nseen < capseen) {
            seen[nseen] = zc_dup(child);
            if (seen[nseen]) nseen++;
        }
        json_append(chain, json_string(child));
        cur = child;
    }
    zc_seen_free(seen, nseen);
    return chain;
}

/* ------------------------------------------------------------- p1 exports */

json_t *w31_compressio_220b6d_p1_cooldown_row(int session_exists,
                                              const json_t *cooldown_until,
                                              const json_t *error) {
    json_t *out = json_object();
    double d = 0.0;
    json_set(out, "session_exists", json_bool(session_exists != 0));
    if (cooldown_until && cooldown_until->type != JSON_NULL && zc_float_of(cooldown_until, &d))
        json_set(out, "cooldown_until", json_number(d));
    else
        json_set(out, "cooldown_until", json_null());
    json_set(out, "error", error ? json_copy(error) : json_null());
    return out;
}

json_t *w31_compressio_220b6d_p1_walk_chain(const char *session_id,
                                            const json_t *steps, int cap) {
    return zc_walk(session_id ? session_id : "", steps, cap);
}

json_t *w31_compressio_220b6d_p1_chain_tip(const char *session_id,
                                           const json_t *steps, int cap) {
    json_t *chain = zc_walk(session_id ? session_id : "", steps, cap);
    size_t n = json_len(chain);
    json_t *out = (n == 0) ? json_string(session_id ? session_id : "")
                           : json_copy(json_get(chain, n - 1));
    json_free(chain);
    return out;
}

json_t *w31_compressio_220b6d_p1_unset_created(const json_t *value) {
    return json_bool(zc_unset_source(value) != 0);
}

json_t *w31_compressio_220b6d_p1_fork_child(const json_t *session, int include_reset) {
    return json_bool(zc_fork_child(session, include_reset) != 0);
}

json_t *w31_compressio_220b6d_p1_compression_child(const json_t *child, const json_t *sessions) {
    return json_bool(zc_compression_child(child, sessions) != 0);
}

json_t *w31_compressio_220b6d_p1_lineage(const char *session_id,
                                         const json_t *sessions,
                                         const json_t *children) {
    const char *sid = session_id ? session_id : "";
    json_t *out = json_array();
    const json_t *root, *current;
    char **seen;
    size_t nseen = 0, capseen = 16;
    int guard = 0, in_lineage = 0;
    size_t i;

    root = zc_row_at(sessions, sid);
    if (!root || root->type != JSON_OBJECT) return out;
    if (zc_fork_child(root, 0)) {
        json_append(out, json_string(sid));
        return out;
    }
    /* climb to the lineage root through compression parents */
    seen = (char **)calloc(capseen, sizeof(char *));
    if (!seen) return out;
    if (zc_row_id(root)) seen[nseen++] = zc_dup(zc_row_id(root));
    while (zc_compression_child(root, sessions) && guard++ < W31_COMPRESSIO_220B6D_CHAIN_CAP) {
        const json_t *parent = zc_row_at(sessions, zc_str(json_obj_get(root, "parent_session_id")));
        if (!parent || !zc_row_id(parent) || zc_seen(seen, nseen, zc_row_id(parent))) break;
        if (nseen >= capseen && !zc_grow(&seen, &capseen)) break;
        seen[nseen++] = zc_dup(zc_row_id(parent));
        root = parent;
    }
    /* walk forward from the root through preferred continuations */
    zc_seen_free(seen, nseen);
    seen = (char **)calloc(capseen, sizeof(char *));
    nseen = 0;
    if (!seen) return out;
    json_append(out, json_string(zc_row_id(root) ? zc_row_id(root) : ""));
    seen[nseen++] = zc_dup(zc_row_id(root) ? zc_row_id(root) : "");
    current = root;
    guard = 0;
    while (zc_is_compression_end(current) && guard++ < W31_COMPRESSIO_220B6D_CHAIN_CAP) {
        const json_t *kids = zc_row_at(children, zc_row_id(current));
        const json_t *next = NULL;
        size_t k;
        if (kids && kids->type == JSON_ARRAY) {
            for (k = 0; k < json_len(kids); k++) {
                const json_t *kid = json_get(kids, k);
                if (zc_compression_child(kid, sessions)) { next = kid; break; }
            }
        }
        if (!next || !zc_row_id(next) || zc_seen(seen, nseen, zc_row_id(next))) break;
        if (nseen >= capseen && !zc_grow(&seen, &capseen)) break;
        json_append(out, json_string(zc_row_id(next)));
        seen[nseen++] = zc_dup(zc_row_id(next));
        current = next;
    }
    for (i = 0; i < json_len(out); i++) {
        const char *v = zc_str(json_get(out, i));
        if (v && !strcmp(v, sid)) in_lineage = 1;
    }
    if (!in_lineage) {
        json_free(out);
        out = json_array();
        json_append(out, json_string(sid));
    }
    zc_seen_free(seen, nseen);
    return out;
}

json_t *w31_compressio_220b6d_p1_tool_root(const char *session_id,
                                           const json_t *sessions) {
    const char *sid = session_id ? session_id : "";
    const json_t *row = zc_row_at(sessions, sid);
    char **seen;
    size_t nseen = 0, capseen = 16;
    int guard = 0;
    json_t *out;

    seen = (char **)calloc(capseen, sizeof(char *));
    if (!seen) return json_string(sid);
    seen[nseen++] = zc_dup(sid);
    while (row && row->type == JSON_OBJECT && guard++ < W31_COMPRESSIO_220B6D_CHAIN_CAP) {
        const char *parent_id;
        const json_t *parent, *muted;
        if (strcmp(zc_field(row, "source"), "tool")) break;
        parent_id = zc_str(json_obj_get(row, "parent_session_id"));
        if (!parent_id || !*parent_id || zc_seen(seen, nseen, parent_id)) break;
        parent = zc_row_at(sessions, parent_id);
        if (!parent || strcmp(zc_field(parent, "source"), "tool")) break;
        /* ``_is_compression_child_row(holder, {**row, "source": None})`` */
        muted = json_copy(row);
        json_obj_del((json_t *)muted, "source");
        if (!zc_compression_child(muted, sessions)) { json_free((json_t *)muted); break; }
        json_free((json_t *)muted);
        if (nseen >= capseen && !zc_grow(&seen, &capseen)) break;
        seen[nseen++] = zc_dup(parent_id);
        row = parent;
    }
    out = json_string(zc_row_id(row) ? zc_row_id(row) : sid);
    zc_seen_free(seen, nseen);
    return out;
}

json_t *w31_compressio_220b6d_p1_lease_key(const char *session_id,
                                           const json_t *sessions) {
    const char *sid = session_id ? session_id : "";
    const json_t *current;
    char **seen;
    size_t nseen = 0, capseen = 16;
    int guard = 0;
    json_t *out;

    if (!*sid) return json_string(sid);
    seen = (char **)calloc(capseen, sizeof(char *));
    if (!seen) return json_string(sid);
    seen[nseen++] = zc_dup(sid);
    current = zc_row_at(sessions, sid);
    while (current && current->type == JSON_OBJECT && guard++ < W31_COMPRESSIO_220B6D_CHAIN_CAP) {
        const char *parent_id = zc_str(json_obj_get(current, "parent_session_id"));
        const json_t *parent;
        if (!parent_id || !*parent_id || zc_seen(seen, nseen, parent_id) || zc_fork_child(current, 1)) break;
        parent = zc_row_at(sessions, parent_id);
        if (!parent || !zc_is_compression_end(parent)) break;
        if (nseen >= capseen && !zc_grow(&seen, &capseen)) break;
        seen[nseen++] = zc_dup(parent_id);
        current = parent;
    }
    if (current && zc_row_id(current) && *zc_row_id(current)) out = json_string(zc_row_id(current));
    else out = json_string(sid);
    zc_seen_free(seen, nseen);
    return out;
}

json_t *w31_compressio_220b6d_p1_cooldown_active(const char *session_id,
                                                 const json_t *row, double now) {
    json_t *out;
    double until = 0.0;
    const json_t *error;
    if (!session_id || !*session_id) return json_null();
    if (!row || row->type != JSON_ARRAY || json_len(row) < 1) return json_null();
    if (!zc_float_of(json_get(row, 0), &until)) return json_null();
    if (until <= now) return json_null();
    out = json_object();
    json_set(out, "cooldown_until", json_number(until));
    json_set(out, "remaining_seconds", json_number(until - now));
    error = json_len(row) > 1 ? json_get(row, 1) : NULL;
    json_set(out, "error", error ? json_copy(error) : json_null());
    return out;
}

json_t *w31_compressio_220b6d_p1_stamp_ancestors(const char *session_id,
                                                 const json_t *created,
                                                 const json_t *steps,
                                                 int unset_ancestor, int cap) {
    json_t *out = json_array();
    const json_t *row;
    json_t *surface;
    const char *current, *parent_id;
    int i;

    if (!session_id) return out;
    row = zc_row_at(created, session_id);
    surface = json_copy((row && row->type == JSON_OBJECT) ? json_obj_get(row, "created_source") : NULL);
    if (!surface) surface = json_null();
    if (zc_unset_source(surface) || !unset_ancestor) {
        json_free(surface);
        return out;
    }
    current = session_id;
    parent_id = zc_str(json_obj_get(row, "parent_session_id"));
    for (i = 0; i < cap; i++) {
        const json_t *parent = (parent_id && *parent_id) ? zc_row_at(created, parent_id) : NULL;
        json_t *pcs;
        const json_t *step;
        if (!parent || parent->type != JSON_OBJECT) break;
        pcs = json_copy(json_obj_get(parent, "created_source"));
        if (!pcs) pcs = json_null();
        if (!zc_unset_source(pcs) && !zc_eq(pcs, surface)) { json_free(pcs); break; }
        step = (steps && steps->type == JSON_OBJECT) ? json_obj_get(steps, parent_id) : NULL;
        if (!step || !zc_str(step) || strcmp(zc_str(step), current)) { json_free(pcs); break; }
        if (!zc_eq(pcs, surface)) json_append(out, json_string(parent_id));
        json_free(pcs);
        current = parent_id;
        parent_id = zc_str(json_obj_get(parent, "parent_session_id"));
    }
    json_free(surface);
    return out;
}

json_t *w31_compressio_220b6d_p1_child_row(const json_t *parent,
                                           const json_t *model_config,
                                           const char *source,
                                           const char *model,
                                           const char *parent_session_id,
                                           const char *child_id,
                                           const char *cwd,
                                           const char *profile_name,
                                           const char *prompt_hash,
                                           const char *own_profile,
                                           const json_t *surface_parent,
                                           const json_t *surface_source,
                                           double now) {
    static const char *const carried_keys[] = {"onboarding_run", "prompt_refresh_epoch", "_chat_adopted"};
    static const char *const fork_keys[] = {"_branched_from", "_reset_from"};
    json_t *pc, *mc, *merged, *out;
    const char *p_profile;
    int continuation = 1;
    size_t i;

    if (!parent || parent->type != JSON_OBJECT) return json_null();
    pc = zc_cfg_of(json_obj_get(parent, "model_config"));
    mc = zc_cfg_of(model_config);
    for (i = 0; i < sizeof fork_keys / sizeof fork_keys[0]; i++)
        if (json_obj_get(mc, fork_keys[i])) continuation = 0;

    /* ``{**carried, **(model_config or {})}``: parent-config key order first,
     * then any novel caller key; a colliding key keeps its carried position. */
    merged = json_object();
    if (continuation) {
        size_t c;
        for (c = 0; c < json_object_size(pc); c++) {
            const char *k = json_object_get_key_at(pc, c);
            size_t j;
            int wanted = 0;
            if (!k) continue;
            for (j = 0; j < sizeof carried_keys / sizeof carried_keys[0]; j++)
                if (!strcmp(k, carried_keys[j])) wanted = 1;
            if (wanted) json_set(merged, k, json_copy(json_object_get_at(pc, c)));
        }
    }
    for (i = 0; i < json_object_size(mc); i++) {
        const char *k = json_object_get_key_at(mc, i);
        if (k) json_set(merged, k, json_copy(json_object_get_at(mc, i)));
    }

    out = json_object();
    json_set(out, "id", json_string(child_id ? child_id : ""));
    json_set(out, "source", source ? json_string(source) : json_null());
    if (zc_truthy(surface_parent)) json_set(out, "created_source", json_copy(surface_parent));
    else if (zc_truthy(surface_source)) json_set(out, "created_source", json_copy(surface_source));
    else json_set(out, "created_source", json_null());
    json_set(out, "model", model ? json_string(model) : json_null());
    if (json_object_size(merged) > 0) json_set(out, "model_config", json_copy(merged));
    else json_set(out, "model_config", json_null());
    json_set(out, "system_prompt_hash", prompt_hash ? json_string(prompt_hash) : json_null());
    json_set(out, "tool_names", json_copy(json_obj_get(parent, "tool_names")));
    json_set(out, "parent_session_id", parent_session_id ? json_string(parent_session_id) : json_null());
    if (cwd && *cwd) json_set(out, "cwd", json_string(cwd));
    else json_set(out, "cwd", json_copy(json_obj_get(parent, "cwd")));
    json_set(out, "git_branch", json_copy(json_obj_get(parent, "git_branch")));
    json_set(out, "git_repo_root", json_copy(json_obj_get(parent, "git_repo_root")));
    p_profile = zc_str(json_obj_get(parent, "profile_name"));
    if (profile_name && *profile_name) json_set(out, "profile_name", json_string(profile_name));
    else if (p_profile && *p_profile) json_set(out, "profile_name", json_string(p_profile));
    else json_set(out, "profile_name", own_profile ? json_string(own_profile) : json_null());
    json_set(out, "user_id", json_copy(json_obj_get(parent, "user_id")));
    json_set(out, "session_key", json_copy(json_obj_get(parent, "session_key")));
    json_set(out, "chat_id", json_copy(json_obj_get(parent, "chat_id")));
    json_set(out, "chat_type", json_copy(json_obj_get(parent, "chat_type")));
    json_set(out, "thread_id", json_copy(json_obj_get(parent, "thread_id")));
    json_set(out, "display_name", json_copy(json_obj_get(parent, "display_name")));
    json_set(out, "origin_json", json_copy(json_obj_get(parent, "origin_json")));
    {
        const json_t *pinned = json_obj_get(parent, "pinned");
        long long pv = 0;
        if (continuation && zc_truthy(pinned) && !zc_int_of(pinned, &pv)) pv = 0;
        json_set(out, "pinned", json_number((double)pv));
    }
    json_set(out, "started_at", json_number(now));
    {
        const json_t *arch = json_obj_get(parent, "archived");
        const json_t *auto_arch = json_obj_get(parent, "auto_archived");
        if (continuation && zc_truthy(arch)) json_set(out, "archived", json_copy(arch));
        else json_set(out, "archived", json_number(0));
        if (continuation && zc_truthy(auto_arch)) json_set(out, "auto_archived", json_copy(auto_arch));
        else json_set(out, "auto_archived", json_number(0));
    }
    json_free(merged);
    json_free(mc);
    json_free(pc);
    return out;
}

/* ------------------------------------------------------- statement bodies */

static const char *const P2_CHAIN_PARTS[5] = {
    "\n                    SELECT child.id\n                    FROM sessions parent\n                    JOIN sessions child ON child.parent_session_id = parent.id\n                    WHERE parent.id = ?\n                      AND parent.end_reason = 'compression'\n                      AND ",
    " IS NULL\n                      AND ",
    " IS NULL\n                      AND NOT (",
    ")\n                      AND COALESCE(child.source, '') != 'tool'\n                    ORDER BY\n                      CASE\n                        WHEN child.end_reason = 'compression' THEN 0\n                        WHEN child.ended_at IS NULL THEN 1\n                        ELSE 2\n                      END,\n                      ",
    " DESC,\n                      child.started_at DESC,\n                      child.id DESC\n                    LIMIT 1\n                    ",
};

static const char *const P2_UNSET_PARTS[2] = {
    "\n    WITH RECURSIVE up(pid, n) AS (\n        SELECT parent_session_id, 1 FROM sessions WHERE id = ?\n        UNION ALL\n        SELECT s.parent_session_id, up.n + 1 FROM up JOIN sessions s ON s.id = up.pid\n        WHERE s.end_reason = 'compression' AND s.created_source = ? AND up.n < ?\n    )\n    SELECT 1 FROM up CROSS JOIN sessions s ON s.id = up.pid\n    WHERE s.end_reason = 'compression' AND ",
    "\n    LIMIT 1\n",
};

static const char *const P2_EDGE_PARTS[3] = {
    "\n    SELECT child.id FROM sessions parent JOIN sessions child ON child.parent_session_id = parent.id\n    WHERE parent.end_reason = 'compression' AND ",
    "\n      AND NOT ",
    "\n",
};

static const char *const P2_RECORD_SQL =
    "UPDATE sessions SET compression_failure_cooldown_until = CASE "
    "WHEN compression_failure_cooldown_until IS NOT NULL  AND compression_failure_cooldown_until > ? "
    "THEN compression_failure_cooldown_until ELSE ? END, compression_failure_error = ? WHERE id = ?";

static const char *const P2_CLEAR_SQL =
    "UPDATE sessions SET compression_failure_cooldown_until = NULL, "
    "compression_failure_error = NULL WHERE id = ?";

static const char *const P2_FINALIZE_SQL =
    "\n                UPDATE sessions\n                SET ended_at = ?,\n"
    "                    end_reason = 'orphaned_compression'\n"
    "                WHERE api_call_count = 0\n"
    "                  AND end_reason IS NULL\n"
    "                  AND ended_at IS NULL\n"
    "                  AND started_at < ?\n"
    "                  AND parent_session_id IS NOT NULL\n"
    "                  AND EXISTS (\n"
    "                      SELECT 1 FROM sessions p\n"
    "                      WHERE p.id = sessions.parent_session_id\n"
    "                        AND p.end_reason = 'compression'\n"
    "                        AND p.ended_at IS NOT NULL\n"
    "                  )\n"
    "                  AND EXISTS (\n"
    "                      SELECT 1 FROM messages m\n"
    "                      WHERE m.session_id = sessions.id\n"
    "                  )\n"
    "                ";

static char *zc_cat5(const char *a, const char *b, const char *c, const char *d, const char *e) {
    size_t na = a ? strlen(a) : 0, nb = b ? strlen(b) : 0, nc = c ? strlen(c) : 0;
    size_t nd = d ? strlen(d) : 0, ne = e ? strlen(e) : 0;
    char *out = (char *)malloc(na + nb + nc + nd + ne + 1);
    char *p;
    if (!out) return NULL;
    p = out;
    if (na) { memcpy(p, a, na); p += na; }
    if (nb) { memcpy(p, b, nb); p += nb; }
    if (nc) { memcpy(p, c, nc); p += nc; }
    if (nd) { memcpy(p, d, nd); p += nd; }
    if (ne) { memcpy(p, e, ne); p += ne; }
    *p = '\0';
    return out;
}

static char *zc_cat3(const char *a, const char *b, const char *c) {
    return zc_cat5(a, b, c, NULL, NULL);
}

static json_t *zc_sql_out(char *sql) {
    json_t *out;
    if (!sql) return json_null();
    out = json_string(sql);
    free(sql);
    return out;
}

/* ------------------------------------------------------------- p2 exports */

json_t *w31_compressio_220b6d_p2_chain_step_sql(const char *jx_branched,
                                                const char *jx_delegate,
                                                const char *reset_child,
                                                const char *last_active) {
    char *a = zc_cat5(P2_CHAIN_PARTS[0], jx_branched ? jx_branched : "",
                      P2_CHAIN_PARTS[1], jx_delegate ? jx_delegate : "", NULL);
    char *b, *c;
    if (!a) return json_null();
    b = zc_cat5(a, P2_CHAIN_PARTS[2], reset_child ? reset_child : "", P2_CHAIN_PARTS[3], NULL);
    free(a);
    if (!b) return json_null();
    c = zc_cat3(b, last_active ? last_active : "", P2_CHAIN_PARTS[4]);
    free(b);
    return zc_sql_out(c);
}

json_t *w31_compressio_220b6d_p2_unset_ancestor_sql(const char *unset_sql) {
    return zc_sql_out(zc_cat3(P2_UNSET_PARTS[0], unset_sql ? unset_sql : "", P2_UNSET_PARTS[1]));
}

json_t *w31_compressio_220b6d_p2_unstamped_edge_sql(const char *unset_parent,
                                                    const char *unset_child,
                                                    const char *non_continuation) {
    char *a = zc_cat5(P2_EDGE_PARTS[0], unset_parent ? unset_parent : "",
                      P2_EDGE_PARTS[1], unset_child ? unset_child : "", NULL);
    char *b;
    if (!a) return json_null();
    b = zc_cat3(a, P2_EDGE_PARTS[2], non_continuation ? non_continuation : "");
    free(a);
    return zc_sql_out(b);
}

json_t *w31_compressio_220b6d_p2_record_cooldown(const char *session_id,
                                                 const json_t *cooldown_until,
                                                 const json_t *error) {
    json_t *out, *params;
    if (!session_id || !*session_id) return json_null();
    out = json_object();
    json_set(out, "sql", json_string(P2_RECORD_SQL));
    params = json_array();
    json_append(params, cooldown_until ? json_copy(cooldown_until) : json_null());
    json_append(params, cooldown_until ? json_copy(cooldown_until) : json_null());
    json_append(params, error ? json_copy(error) : json_null());
    json_append(params, json_string(session_id));
    json_set(out, "params", params);
    return out;
}

json_t *w31_compressio_220b6d_p2_clear_cooldown(const char *session_id) {
    json_t *out, *params;
    if (!session_id || !*session_id) return json_null();
    out = json_object();
    json_set(out, "sql", json_string(P2_CLEAR_SQL));
    params = json_array();
    json_append(params, json_string(session_id));
    json_set(out, "params", params);
    return out;
}

json_t *w31_compressio_220b6d_p2_finalize_orphans(double now) {
    json_t *out = json_object(), *params = json_array();
    json_set(out, "sql", json_string(P2_FINALIZE_SQL));
    json_append(params, json_number(now));
    json_append(params, json_number(now - 604800.0));
    json_set(out, "params", params);
    return out;
}

json_t *w31_compressio_220b6d_p2_read_number(const json_t *value,
                                             const char *kind,
                                             const json_t *zero) {
    double z = 0.0, v = 0.0;
    const json_t *raw;
    int is_int;

    if (zero) { if (!zc_num(zero, &z)) z = 0.0; }
    is_int = (kind && !strcmp(kind, "float")) ? 0 : 1;
    raw = (value && value->type != JSON_NULL) ? value : NULL;
    if (raw && !zc_truthy(raw)) raw = NULL;   /* ``row[0] or zero`` */
    if (!raw) return json_number(z);
    if (is_int) {
        long long iv;
        if (!zc_int_of(raw, &iv)) return json_number(z);
        v = (double)iv;
    } else if (!zc_float_of(raw, &v)) {
        return json_number(z);
    }
    return json_number(v > z ? v : z);
}

json_t *w31_compressio_220b6d_p2_streak_write(const char *column,
                                              const char *session_id,
                                              const json_t *streak) {
    json_t *out, *params;
    long long iv;
    char sql[160];
    if (!session_id || !*session_id) return json_null();
    if (!zc_int_of(streak, &iv)) return json_string("TypeError");
    if (iv < 0) iv = 0;
    snprintf(sql, sizeof sql, "UPDATE sessions SET %s = ? WHERE id = ?", column ? column : "");
    out = json_object();
    json_set(out, "sql", json_string(sql));
    params = json_array();
    json_append(params, json_number((double)iv));
    json_append(params, json_string(session_id));
    json_set(out, "params", params);
    return out;
}

json_t *w31_compressio_220b6d_p2_deadline_write(const char *session_id,
                                                const json_t *deadline) {
    json_t *out, *params;
    double d = 0.0;
    if (!session_id || !*session_id) return json_null();
    if (zc_truthy(deadline) && !zc_float_of(deadline, &d)) d = 0.0;
    if (!(d > 0.0)) d = 0.0;
    out = json_object();
    json_set(out, "sql", json_string("UPDATE sessions SET compression_recovery_deadline = ? WHERE id = ?"));
    params = json_array();
    json_append(params, d != 0.0 ? json_number(d) : json_null());
    json_append(params, json_string(session_id));
    json_set(out, "params", params);
    return out;
}

json_t *w31_compressio_220b6d_p2_overload_bump(const char *session_id,
                                               const json_t *row) {
    long long iv;
    const json_t *cell;
    if (!session_id || !*session_id) return json_null();
    if (!row || row->type == JSON_NULL) return json_null();
    cell = (row->type == JSON_ARRAY) ? (json_len(row) > 0 ? json_get(row, 0) : NULL) : row;
    if (!cell || !zc_int_of(cell, &iv)) return json_null();
    return json_number((double)iv);
}

json_t *w31_compressio_220b6d_p2_parent_gate(int parent_exists, int ended_at_set,
                                             int automatic_reason) {
    if (!parent_exists) return json_string("missing");
    if (!ended_at_set) return json_string("proceed");
    if (!automatic_reason) return json_string("already_ended");
    return json_string("clear_stamp");
}
