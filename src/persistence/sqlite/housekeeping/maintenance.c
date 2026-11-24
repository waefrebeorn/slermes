/*
 * maintenance.c - C11 port of upstream
 * persistence/sqlite/housekeeping/maintenance.py, home of gates
 * w33_maintenanc_3e1b75_p1 and w33_maintenanc_3e1b75_p2.
 *
 * PoP: w33_maintenanc_3e1b75_p1_escape_like            @ maintenance.py:_escape_like
 * PoP: w33_maintenanc_3e1b75_p1_like                   @ maintenance.py:_like
 * PoP: w33_maintenanc_3e1b75_p1_placeholders           @ maintenance.py:_placeholders
 * PoP: w33_maintenanc_3e1b75_p1_id_chunks              @ maintenance.py:_id_chunks
 * PoP: w33_maintenanc_3e1b75_p1_cwd_prefix_clause      @ sqlite/conversation/_store/sessions.py:_cwd_prefix_clause
 * PoP: w33_maintenanc_3e1b75_p1_last_active_sql        @ sqlite/gateway/_routes.py:_sql_session_last_active
 * PoP: w33_maintenanc_3e1b75_p1_continuation_edge      @ maintenance.py:_CONTINUATION_EDGE_SQL
 * PoP: w33_maintenanc_3e1b75_p1_continued_ancestors_sql@ maintenance.py:_continued_ancestors_sql
 * PoP: w33_maintenanc_3e1b75_p1_pinned_tail_sql        @ maintenance.py:_PINNED_TAIL_SQL
 * PoP: w33_maintenanc_3e1b75_p1_not_pinned_sql         @ maintenance.py:_not_pinned_sql
 * PoP: w33_maintenanc_3e1b75_p1_row_bytes_sql          @ maintenance.py:_row_bytes_sql
 * PoP: w33_maintenanc_3e1b75_p1_projection_fields      @ maintenance.py:session_projection_digest (field selection)
 * PoP: w33_maintenanc_3e1b75_p1_sweep_scope            @ maintenance.py:sweep_orphaned_sessions (scope plan)
 * PoP: w33_maintenanc_3e1b75_p1_idle_archive_sql       @ maintenance.py:list_idle_archive_candidates
 * PoP: w33_maintenanc_3e1b75_p2_prune_filter_names     @ maintenance.py:_PRUNE_FILTER_NAMES
 * PoP: w33_maintenanc_3e1b75_p2_prune_filter_where     @ maintenance.py:_prune_filter_where
 * PoP: w33_maintenanc_3e1b75_p2_prune_where            @ maintenance.py:_prune_where
 * PoP: w33_maintenanc_3e1b75_p2_count_open_sql         @ maintenance.py:count_open_prune_matches
 * PoP: w33_maintenanc_3e1b75_p2_prune_open_guard       @ maintenance.py:prune_sessions (include_open guard)
 * PoP: w33_maintenanc_3e1b75_p2_reclaim_estimate       @ maintenance.py:prune_reclaim_estimate (report)
 * PoP: w33_maintenanc_3e1b75_p2_export_safe            @ maintenance.py:assert_export_safe
 * PoP: w33_maintenanc_3e1b75_p2_tree_key               @ maintenance.py:_inside_tree.key
 * PoP: w33_maintenanc_3e1b75_p2_inside_tree            @ maintenance.py:_inside_tree.inside
 * PoP: w33_maintenanc_3e1b75_p2_tree_serve_facts       @ maintenance.py:tree_served_sessions (facts)
 * PoP: w33_maintenanc_3e1b75_p2_auto_archive_extra_set @ maintenance.py:auto_archive_lineage (extra SET)
 * PoP: w33_maintenanc_3e1b75_p2_heartbeat_register     @ maintenance.py:register_backend_heartbeat
 * PoP: w33_maintenanc_3e1b75_p2_heartbeat_delete       @ maintenance.py:clear_backend_heartbeat
 * PoP: w33_maintenanc_3e1b75_p2_prune_batches          @ maintenance.py:prune_sessions (batched deletes)
 * PoP: w33_maintenanc_3e1b75_p2_retained_tree_sql      @ maintenance.py:retained_tree_is_live (probe)
 * PoP: w33_maintenanc_3e1b75_p2_served_tree_sql        @ maintenance.py:_SERVED_TREE_SQL
 *
 * Seams, all outside this file: the sqlite connection/holder and its
 * transactions, ``time.time()`` (every entry point takes ``now``),
 * ``Path.resolve()`` (path identity is the caller's spelling), the
 * ``hermes_inside_tree`` SQL UDF, the blake2b projection digest over live rows,
 * and the post-commit delete effects (hook, retired media, transcript files).
 * What is ported is exactly what those values decide: the selection SQL bytes,
 * the bound-parameter order, the guards, and the report shapes.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "noc_w33_maintenanc_3e1b75_p1.h"
#include "noc_w33_maintenanc_3e1b75_p2.h"

/* ── small buffers ─────────────────────────────────────────────────────── */

typedef struct { char *s; size_t n, cap; } sb_t;

static void sb_grow(sb_t *b, size_t need) {
    if (b->n + need + 1 <= b->cap) return;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < b->n + need + 1) cap *= 2;
    char *p = (char *)realloc(b->s, cap);
    if (!p) abort();
    b->s = p;
    b->cap = cap;
}

static void sb_addn(sb_t *b, const char *s, size_t n) {
    sb_grow(b, n);
    memcpy(b->s + b->n, s, n);
    b->n += n;
    b->s[b->n] = '\0';
}

static void sb_add(sb_t *b, const char *s) { sb_addn(b, s, strlen(s)); }
static void sb_addc(sb_t *b, char c) { sb_addn(b, &c, 1); }

static char *sb_take(sb_t *b) {
    if (!b->s) { b->s = (char *)malloc(1); if (!b->s) abort(); b->s[0] = '\0'; }
    return b->s;
}

static json_t *sb_out(sb_t *b) {
    char *s = sb_take(b);
    json_t *j = json_string(s);
    free(s);
    return j;
}

typedef struct { char *v[64]; int n; } cl_t;

static void cl_add(cl_t *c, const char *s) {
    if (c->n >= 64) return;
    size_t len = strlen(s);
    char *p = (char *)malloc(len + 1);
    if (!p) abort();
    memcpy(p, s, len + 1);
    c->v[c->n++] = p;
}

static char *cl_join(cl_t *c) {
    sb_t b = {0};
    for (int i = 0; i < c->n; i++) {
        if (i) sb_add(&b, " AND ");
        sb_add(&b, c->v[i]);
    }
    char *out = sb_take(&b);
    for (int i = 0; i < c->n; i++) free(c->v[i]);
    c->n = 0;
    return out;
}

/* ── python-ish value helpers ──────────────────────────────────────────── */

static int py_truthy(const json_t *v) {
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

/* A numeric view: int, float or bool.  None (and missing) are not numbers. */
static int py_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

static long long py_int(double v) { return (long long)v; }

/* str.lower() over the ASCII range the SQL literals live in. */
static char *py_lower(const char *s) {
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    if (!o) abort();
    for (size_t i = 0; i < n; i++) o[i] = (char)tolower((unsigned char)s[i]);
    o[n] = '\0';
    return o;
}

/* repr() of a str for error messages: quote, escape, short. */
static void sb_repr_str(sb_t *b, const char *s) {
    char quote = '\'';
    if (strchr(s, '\'') && !strchr(s, '"')) quote = '"';
    sb_addc(b, quote);
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == (unsigned char)quote || c == '\\') { sb_addc(b, '\\'); sb_addc(b, (char)c); }
        else if (c == '\n') sb_add(b, "\\n");
        else if (c == '\r') sb_add(b, "\\r");
        else if (c == '\t') sb_add(b, "\\t");
        else if (c < 0x20 || c == 0x7f) {
            char tmp[8];
            snprintf(tmp, sizeof tmp, "\\x%02x", c);
            sb_add(b, tmp);
        } else sb_addc(b, (char)c);
    }
    sb_addc(b, quote);
}

/* repr() of a float: shortest round-tripping form, ".0" on an integral value. */
static void sb_repr_float(sb_t *b, double v) {
    char tmp[64];
    if (v != v) { sb_add(b, "nan"); return; }
    if (isinf(v)) { sb_add(b, v < 0 ? "-inf" : "inf"); return; }
    int done = 0;
    for (int prec = 1; prec <= 17 && !done; prec++) {
        snprintf(tmp, sizeof tmp, "%.*g", prec, v);
        if (strtod(tmp, NULL) == v) done = 1;
    }
    int integral = !strpbrk(tmp, ".eE");
    sb_add(b, tmp);
    if (integral) sb_add(b, ".0");
}

static json_t *err_obj(const char *type, const char *message) {
    json_t *o = json_object();
    json_set(o, "error", json_string(type));
    json_set(o, "message", json_string(message));
    return o;
}

static void arr_add_str(json_t *a, const char *s) { json_append(a, json_string(s ? s : "")); }
static void arr_add_num(json_t *a, double v) { json_append(a, json_number(v)); }

/* The JSON value as the bound parameter it would be (strings stay strings). */
static void arr_add_value(json_t *a, const json_t *v) {
    if (!v) { json_append(a, json_null()); return; }
    json_append(a, json_copy(v));
}

/* ── SQL fragments ─────────────────────────────────────────────────────── */

#define TOKENS_SQL "(COALESCE(s.input_tokens, 0) + COALESCE(s.output_tokens, 0))"
#define COST_SQL   "COALESCE(s.actual_cost_usd, s.estimated_cost_usd, 0)"

/* _non_continuation_child_sql: branch/delegate/reset markers bound to the
 * parent id, then the tool-session arm.  Two leading spaces per line. */
#define CONT_MARKERS(child, parent) \
    "  AND COALESCE(json_extract((CASE WHEN json_valid(" child "model_config) THEN " child "model_config ELSE json_object() END), '$._branched_from'), '') != " parent "\n" \
    "  AND COALESCE(json_extract((CASE WHEN json_valid(" child "model_config) THEN " child "model_config ELSE json_object() END), '$._delegate_from'), '') != " parent "\n" \
    "  AND COALESCE(json_extract((CASE WHEN json_valid(" child "model_config) THEN " child "model_config ELSE json_object() END), '$._reset_from'), '') != " parent "\n" \
    "  AND COALESCE(" child "source, '') != 'tool'\n"

#define CONT_EDGE(child, parent) "p.end_reason = 'compression'\n" CONT_MARKERS(child, parent)

#define PINNED_TAIL_BODY(edge) \
    "WITH RECURSIVE tail(id) AS (" \
    " SELECT c.id FROM sessions c JOIN sessions p ON p.id = c.parent_session_id" \
    " WHERE COALESCE(p.pinned, 0) = 1 AND " edge \
    " UNION" \
    " SELECT c.id FROM tail t JOIN sessions p ON p.id = t.id JOIN sessions c ON c.parent_session_id = p.id" \
    " WHERE " edge \
    ") SELECT id FROM tail"

static void sb_not_pinned(sb_t *b, const char *alias) {
    sb_add(b, "COALESCE(");
    sb_add(b, alias);
    sb_add(b, ".pinned, 0) = 0 AND ");
    sb_add(b, alias);
    sb_add(b, ".id NOT IN (" PINNED_TAIL_BODY(CONT_EDGE("c.", "p.id")) ")");
}

static void sb_last_active(sb_t *b, const char *alias) {
    sb_add(b, "COALESCE((SELECT MAX(_act_v.v) FROM (SELECT ");
    sb_add(b, alias);
    sb_add(b, ".last_activity_at AS v UNION ALL SELECT (SELECT MAX(_act_m.timestamp) FROM messages _act_m INDEXED BY idx_messages_session WHERE _act_m.session_id = ");
    sb_add(b, alias);
    sb_add(b, ".id AND _act_m.timestamp BETWEEN 0.0 AND 4200000000.0)) _act_v WHERE _act_v.v BETWEEN 0.0 AND 4200000000.0), (SELECT _win.v FROM (SELECT ");
    sb_add(b, alias);
    sb_add(b, ".started_at AS v) _win WHERE _win.v BETWEEN 0.0 AND 4200000000.0))");
}

static void sb_continued_ancestors(sb_t *b, const char *where) {
    const char *edge = CONT_EDGE("c.", "p.id");
    sb_add(b, "WITH RECURSIVE kept(id) AS ("
              " SELECT p.id FROM sessions c JOIN sessions p ON p.id = c.parent_session_id"
              " WHERE ");
    sb_add(b, edge);
    sb_add(b, " AND NOT EXISTS (SELECT 1 FROM sessions s WHERE s.id = c.id AND ");
    sb_add(b, where);
    sb_add(b, ") UNION SELECT p.id FROM kept k JOIN sessions c ON c.id = k.id JOIN sessions p ON p.id = c.parent_session_id WHERE ");
    sb_add(b, edge);
    sb_add(b, ") SELECT id FROM kept");
}

static const char *HEARTBEAT_INSERT_SQL =
    "INSERT INTO gateway_heartbeats (backend_id, pid, started_at, last_heartbeat, profile, host)"
    " VALUES (?, ?, ?, ?, ?, ?) ON CONFLICT(backend_id) DO UPDATE SET pid = excluded.pid,"
    " started_at = excluded.started_at, last_heartbeat = excluded.last_heartbeat,"
    " profile = excluded.profile, host = excluded.host";

static const char *HEARTBEAT_DELETE_SQL =
    "DELETE FROM gateway_heartbeats WHERE backend_id = ?";

static const char *SERVED_TREE_SQL =
    "SELECT 1 FROM sessions s WHERE hermes_inside_tree(s.cwd) AND (s.ended_at IS NULL OR EXISTS ("
    "SELECT 1 FROM state_meta m WHERE m.key IN ('loop:'||s.id, 'heartbeat:'||s.id, 'goal:'||s.id) "
    "AND (json_valid(m.value)=0 OR COALESCE(json_extract(m.value,'$.status'),'active')='active'))) LIMIT 1";

static const char *AUTO_ARCHIVE_EXTRA_SET =
    ", auto_archived = CASE WHEN archived = 0 THEN 1 ELSE auto_archived END";

/* ── gate p1: builders ─────────────────────────────────────────────────── */

json_t *w33_maintenanc_3e1b75_p1_escape_like(const char *text) {
    sb_t b = {0};
    for (const char *p = text ? text : ""; *p; p++) {
        if (*p == '\\') sb_add(&b, "\\\\");
        else if (*p == '%') sb_add(&b, "\\%");
        else if (*p == '_') sb_add(&b, "\\_");
        else sb_addc(&b, *p);
    }
    return sb_out(&b);
}

json_t *w33_maintenanc_3e1b75_p1_like(const char *value) {
    char *low = py_lower(value ? value : "");
    sb_t b = {0};
    sb_addc(&b, '%');
    for (const char *p = low; *p; p++) {
        if (*p == '\\') sb_add(&b, "\\\\");
        else if (*p == '%') sb_add(&b, "\\%");
        else if (*p == '_') sb_add(&b, "\\_");
        else sb_addc(&b, *p);
    }
    sb_addc(&b, '%');
    free(low);
    return sb_out(&b);
}

json_t *w33_maintenanc_3e1b75_p1_placeholders(double count) {
    sb_t b = {0};
    long long n = py_int(count);
    for (long long i = 0; i < n; i++) {
        if (i) sb_addc(&b, ',');
        sb_addc(&b, '?');
    }
    return sb_out(&b);
}

json_t *w33_maintenanc_3e1b75_p1_id_chunks(const json_t *ids, double size) {
    json_t *out = json_array();
    long long step = py_int(size);
    size_t n = ids ? json_len(ids) : 0;
    if (step <= 0) return out;
    for (size_t start = 0; start < n; start += (size_t)step) {
        json_t *chunk = json_array();
        for (size_t i = start; i < n && i < start + (size_t)step; i++)
            json_append(chunk, json_copy(json_get(ids, i)));
        json_append(out, chunk);
    }
    return out;
}

json_t *w33_maintenanc_3e1b75_p1_cwd_prefix_clause(const char *prefix_in) {
    const char *src = prefix_in ? prefix_in : "";
    size_t n = strlen(src);
    while (n > 0 && (src[n - 1] == '/' || src[n - 1] == '\\')) n--;
    char *prefix;
    if (n == 0) {
        prefix = (char *)malloc(strlen(src) + 1);
        if (!prefix) abort();
        strcpy(prefix, src);
    } else {
        prefix = (char *)malloc(n + 1);
        if (!prefix) abort();
        memcpy(prefix, src, n);
        prefix[n] = '\0';
    }

    sb_t esc = {0};
    for (const char *p = prefix; *p; p++) {
        if (*p == '\\') sb_add(&esc, "\\\\");
        else if (*p == '%') sb_add(&esc, "\\%");
        else if (*p == '_') sb_add(&esc, "\\_");
        else sb_addc(&esc, *p);
    }
    char *e = sb_take(&esc);

    sb_t slash = {0};
    sb_add(&slash, e); sb_add(&slash, "/%");
    char *p_slash = sb_take(&slash);

    sb_t bs = {0};
    sb_add(&bs, e); sb_add(&bs, "\\\\%");
    char *p_bs = sb_take(&bs);

    json_t *o = json_object();
    json_set(o, "clause", json_string("(s.cwd = ? OR s.cwd LIKE ? ESCAPE '\\' OR s.cwd LIKE ? ESCAPE '\\')"));
    json_t *params = json_array();
    arr_add_str(params, prefix);
    arr_add_str(params, p_slash);
    arr_add_str(params, p_bs);
    json_set(o, "params", params);

    free(prefix); free(e); free(p_slash); free(p_bs);
    return o;
}

json_t *w33_maintenanc_3e1b75_p1_last_active_sql(const char *alias) {
    sb_t b = {0};
    sb_last_active(&b, alias && *alias ? alias : W33_MAINTENANC_3E1B75_P1_DEFAULT_ALIAS);
    return sb_out(&b);
}

json_t *w33_maintenanc_3e1b75_p1_continuation_edge(void) {
    return json_string(CONT_EDGE("c.", "p.id"));
}

json_t *w33_maintenanc_3e1b75_p1_continued_ancestors_sql(const char *candidates_where) {
    sb_t b = {0};
    sb_continued_ancestors(&b, candidates_where ? candidates_where : "");
    return sb_out(&b);
}

json_t *w33_maintenanc_3e1b75_p1_pinned_tail_sql(void) {
    return json_string(PINNED_TAIL_BODY(CONT_EDGE("c.", "p.id")));
}

json_t *w33_maintenanc_3e1b75_p1_not_pinned_sql(const char *alias) {
    sb_t b = {0};
    sb_not_pinned(&b, alias && *alias ? alias : W33_MAINTENANC_3E1B75_P1_DEFAULT_ALIAS);
    return sb_out(&b);
}

json_t *w33_maintenanc_3e1b75_p1_row_bytes_sql(const json_t *columns) {
    sb_t b = {0};
    size_t n = columns ? json_len(columns) : 0;
    int first = 1;
    for (size_t i = 0; i < n; i++) {
        const json_t *row = json_get(columns, i);
        const json_t *name = row ? json_get(row, 1) : NULL;
        const json_t *type = row ? json_get(row, 2) : NULL;
        if (!name) continue;
        const char *tname = name->type == JSON_STRING ? name->str_val : "";
        const char *ttype = (type && type->type == JSON_STRING) ? type->str_val : "";
        char upper[64];
        size_t k = 0;
        for (; ttype[k] && k + 1 < sizeof upper; k++) upper[k] = (char)toupper((unsigned char)ttype[k]);
        upper[k] = '\0';
        if (strcmp(upper, "TEXT") && strcmp(upper, "BLOB") && strcmp(upper, "")) continue;
        if (!first) sb_add(&b, " + ");
        first = 0;
        sb_add(&b, "COALESCE(LENGTH(CAST(m.\"");
        sb_add(&b, tname);
        sb_add(&b, "\" AS BLOB)), 0)");
    }
    if (first) return json_string("0");
    return sb_out(&b);
}

/* SESSION_PROJECTION_FIELDS, in order. */
static const char *const PROJECTION_FIELDS[] = {
    "id", "source", "session_key", "display_name", "model", "parent_session_id",
    "started_at", "ended_at", "end_reason", "message_count", "tool_call_count",
    "cwd", "git_branch", "git_repo_root", "title", "title_source", "profile_name",
    "archived", "pinned", "hidden", "last_read_at", "handoff_state",
};
#define PROJECTION_FIELD_COUNT ((int)(sizeof PROJECTION_FIELDS / sizeof PROJECTION_FIELDS[0]))

json_t *w33_maintenanc_3e1b75_p1_projection_fields(const json_t *available) {
    size_t n = available ? json_len(available) : 0;
    json_t *fields = json_array();
    int has_id = 0;
    for (int i = 0; i < PROJECTION_FIELD_COUNT; i++) {
        for (size_t j = 0; j < n; j++) {
            const json_t *v = json_get(available, j);
            if (v && v->type == JSON_STRING && !strcmp(v->str_val, PROJECTION_FIELDS[i])) {
                json_append(fields, json_string(PROJECTION_FIELDS[i]));
                break;
            }
        }
    }
    for (size_t j = 0; j < n; j++) {
        const json_t *v = json_get(available, j);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, "id")) { has_id = 1; break; }
    }
    if (json_len(fields) == 0) { json_free(fields); return json_null(); }
    json_t *o = json_object();
    json_set(o, "fields", fields);
    json_set(o, "order", json_string(has_id ? " ORDER BY id" : ""));
    return o;
}

json_t *w33_maintenanc_3e1b75_p1_sweep_scope(const json_t *req) {
    double max_idle = 0, now = 0;
    py_number(json_obj_get(req, "max_idle_seconds"), &max_idle);
    py_number(json_obj_get(req, "now"), &now);

    const json_t *sources = json_obj_get(req, "sources");
    size_t ns = sources ? json_len(sources) : 0;
    json_t *srcs = json_array();
    for (size_t i = 0; i < ns; i++) {
        const json_t *s = json_get(sources, i);
        if (s && s->type == JSON_STRING && s->str_val[0]) arr_add_str(srcs, s->str_val);
    }

    if (max_idle <= 0 || json_len(srcs) == 0) {
        json_free(srcs);
        json_t *skip = json_object();
        json_set(skip, "skip", json_bool(1));
        return skip;
    }

    double hb_staleness = 0, hb_grace = 0;
    int have_staleness = py_number(json_obj_get(req, "heartbeat_staleness_seconds"), &hb_staleness);
    int have_grace = py_number(json_obj_get(req, "heartbeat_ownership_grace_seconds"), &hb_grace);
    if (!(have_staleness && hb_staleness > 0)) hb_staleness = max_idle * 2;
    if (!(have_grace && hb_grace >= 0)) hb_grace = hb_staleness;

    double cutoff = now - max_idle;
    int exclude_pinned = py_truthy(json_obj_get(req, "exclude_pinned"));
    int respect_hb = py_truthy(json_obj_get(req, "respect_gateway_heartbeats"));

    sb_t sql = {0};
    sb_add(&sql, "SELECT id FROM sessions WHERE ended_at IS NULL AND source IN (");
    for (size_t i = 0; i < json_len(srcs); i++) {
        if (i) sb_addc(&sql, ',');
        sb_addc(&sql, '?');
    }
    sb_addc(&sql, ')');
    if (exclude_pinned) {
        sb_add(&sql, " AND ");
        sb_not_pinned(&sql, "sessions");
    }
    sb_add(&sql, " AND started_at < ? AND ");
    sb_last_active(&sql, "sessions");
    sb_add(&sql, " < ?");
    if (respect_hb)
        sb_add(&sql, " AND NOT EXISTS (SELECT 1 FROM gateway_heartbeats h WHERE"
                     " h.last_heartbeat >= ? AND h.started_at <= sessions.started_at + ?)");

    json_t *params = json_array();
    for (size_t i = 0; i < json_len(srcs); i++) json_append(params, json_copy(json_get(srcs, i)));
    arr_add_num(params, cutoff);
    arr_add_num(params, cutoff);
    if (respect_hb) {
        arr_add_num(params, now - hb_staleness);
        arr_add_num(params, hb_grace);
    }

    json_t *o = json_object();
    json_set(o, "skip", json_bool(0));
    json_set(o, "select_sql", sb_out(&sql));
    json_set(o, "select_params", params);
    json_free(srcs);
    return o;
}

json_t *w33_maintenanc_3e1b75_p1_idle_archive_sql(const json_t *req) {
    double idle_days = 0, now = 0;
    int have_days = py_number(json_obj_get(req, "idle_days"), &idle_days);
    py_number(json_obj_get(req, "now"), &now);
    if (!have_days || idle_days < 0) {
        json_t *skip = json_object();
        json_set(skip, "skip", json_bool(1));
        return skip;
    }

    double cutoff = now - idle_days * 86400.0;
    int exclude_pinned = py_truthy(json_obj_get(req, "exclude_pinned"));
    const char *title = json_get_str(req, "title", "Bot Chat");

    sb_t sql = {0};
    sb_add(&sql, "\n                SELECT s.id FROM sessions s\n"
                 "                WHERE s.archived = 0\n"
                 "                  AND COALESCE(s.end_reason, '') <> 'compression'\n"
                 "                  ");
    if (exclude_pinned) {
        sb_add(&sql, "AND ");
        sb_not_pinned(&sql, "s");
    }
    sb_add(&sql, "\n                  AND NOT (COALESCE(s.hidden, 0) <> 0 AND COALESCE(s.title, '') = ?)\n"
                 "                  AND ");
    sb_last_active(&sql, "s");
    sb_add(&sql, " < ?\n"
                 "                ORDER BY s.started_at ASC\n"
                 "                ");

    json_t *params = json_array();
    arr_add_str(params, title);
    arr_add_num(params, cutoff);

    json_t *o = json_object();
    json_set(o, "skip", json_bool(0));
    json_set(o, "sql", sb_out(&sql));
    json_set(o, "params", params);
    return o;
}

/* ── gate p2: filter assembly ──────────────────────────────────────────── */

typedef struct { const char *name; int truthy; } pf_t;

/* _PRUNE_FILTERS, in evaluation order. */
static const pf_t PRUNE_FILTERS[] = {
    {"last_active_before", 0}, {"last_active_after", 0},
    {"started_before", 0},     {"started_after", 0},
    {"source", 1},             {"title_like", 1},
    {"end_reason", 1},         {"cwd_prefix", 1},
    {"min_messages", 0},       {"max_messages", 0},
    {"model_like", 1},         {"provider", 1},
    {"user_id", 1},            {"chat_id", 1},
    {"chat_type", 1},          {"branch_like", 1},
    {"min_tokens", 0},         {"max_tokens", 0},
    {"min_cost", 0},           {"max_cost", 0},
    {"min_tool_calls", 0},     {"max_tool_calls", 0},
};
#define PRUNE_FILTER_COUNT ((int)(sizeof PRUNE_FILTERS / sizeof PRUNE_FILTERS[0]))

static const char *const PRUNE_FILTER_NAMES[] = {
    "last_active_before", "last_active_after", "started_before", "started_after",
    "source", "title_like", "end_reason", "cwd_prefix", "min_messages",
    "max_messages", "model_like", "provider", "user_id", "chat_id", "chat_type",
    "branch_like", "min_tokens", "max_tokens", "min_cost", "max_cost",
    "min_tool_calls", "max_tool_calls", "archived", "include_pinned",
    "lineage_tips_only",
};
#define PRUNE_FILTER_NAME_COUNT ((int)(sizeof PRUNE_FILTER_NAMES / sizeof PRUNE_FILTER_NAMES[0]))

json_t *w33_maintenanc_3e1b75_p2_prune_filter_names(void) {
    const char **names = (const char **)malloc(sizeof(char *) * PRUNE_FILTER_NAME_COUNT);
    if (!names) abort();
    for (int i = 0; i < PRUNE_FILTER_NAME_COUNT; i++) names[i] = PRUNE_FILTER_NAMES[i];
    for (int i = 1; i < PRUNE_FILTER_NAME_COUNT; i++) {
        const char *key = names[i];
        int j = i - 1;
        while (j >= 0 && strcmp(names[j], key) > 0) { names[j + 1] = names[j]; j--; }
        names[j + 1] = key;
    }
    json_t *out = json_array();
    for (int i = 0; i < PRUNE_FILTER_NAME_COUNT; i++) arr_add_str(out, names[i]);
    free(names);
    return out;
}

/* The clause builder for one filter value: adds clauses and bound params. */
static void pf_apply(const char *name, const json_t *v, cl_t *clauses, json_t *params) {
    sb_t b = {0};
    if (!strcmp(name, "last_active_before")) {
        sb_last_active(&b, "s");
        sb_add(&b, " < ?");
        cl_add(clauses, sb_take(&b));
        cl_add(clauses, "(COALESCE(s.end_reason, '') != 'startup_orphan_reap' OR s.ended_at < ?)");
        arr_add_value(params, v);
        arr_add_value(params, v);
    } else if (!strcmp(name, "last_active_after")) {
        sb_last_active(&b, "s");
        sb_add(&b, " >= ?");
        cl_add(clauses, sb_take(&b));
        arr_add_value(params, v);
    } else if (!strcmp(name, "started_before")) {
        cl_add(clauses, "s.started_at < ?");
        arr_add_value(params, v);
    } else if (!strcmp(name, "started_after")) {
        cl_add(clauses, "s.started_at >= ?");
        arr_add_value(params, v);
    } else if (!strcmp(name, "source")) {
        cl_add(clauses, "s.source = ?");
        arr_add_value(params, v);
    } else if (!strcmp(name, "title_like") || !strcmp(name, "model_like") || !strcmp(name, "branch_like")) {
        const char *col = !strcmp(name, "title_like") ? "title"
                        : !strcmp(name, "model_like") ? "model" : "git_branch";
        sb_add(&b, "LOWER(COALESCE(s.");
        sb_add(&b, col);
        sb_add(&b, ", '')) LIKE ? ESCAPE '\\'");
        cl_add(clauses, sb_take(&b));
        json_t *pat = w33_maintenanc_3e1b75_p1_like(v && v->type == JSON_STRING ? v->str_val : "");
        json_append(params, pat);
    } else if (!strcmp(name, "end_reason")) {
        cl_add(clauses, "s.end_reason = ?");
        arr_add_value(params, v);
    } else if (!strcmp(name, "cwd_prefix")) {
        json_t *built = w33_maintenanc_3e1b75_p1_cwd_prefix_clause(
            v && v->type == JSON_STRING ? v->str_val : "");
        json_t *clause = json_obj_get(built, "clause");
        json_t *ps = json_obj_get(built, "params");
        if (clause && clause->type == JSON_STRING) cl_add(clauses, clause->str_val);
        for (size_t i = 0; ps && i < json_len(ps); i++)
            json_append(params, json_copy(json_get(ps, i)));
        json_free(built);
    } else if (!strcmp(name, "min_messages")) {
        cl_add(clauses, "s.message_count >= ?");
        arr_add_value(params, v);
    } else if (!strcmp(name, "max_messages")) {
        cl_add(clauses, "s.message_count <= ?");
        arr_add_value(params, v);
    } else if (!strcmp(name, "provider")) {
        cl_add(clauses, "LOWER(COALESCE(s.billing_provider, '')) = ?");
        char *low = py_lower(v && v->type == JSON_STRING ? v->str_val : "");
        arr_add_str(params, low);
        free(low);
    } else if (!strcmp(name, "user_id")) {
        cl_add(clauses, "s.user_id = ?");
        arr_add_value(params, v);
    } else if (!strcmp(name, "chat_id")) {
        cl_add(clauses, "s.chat_id = ?");
        arr_add_value(params, v);
    } else if (!strcmp(name, "chat_type")) {
        cl_add(clauses, "s.chat_type = ?");
        arr_add_value(params, v);
    } else if (!strcmp(name, "min_tokens")) {
        cl_add(clauses, TOKENS_SQL " >= ?");
        arr_add_value(params, v);
    } else if (!strcmp(name, "max_tokens")) {
        cl_add(clauses, TOKENS_SQL " <= ?");
        arr_add_value(params, v);
    } else if (!strcmp(name, "min_cost")) {
        cl_add(clauses, COST_SQL " >= ?");
        arr_add_value(params, v);
    } else if (!strcmp(name, "max_cost")) {
        cl_add(clauses, COST_SQL " <= ?");
        arr_add_value(params, v);
    } else if (!strcmp(name, "min_tool_calls")) {
        cl_add(clauses, "COALESCE(s.tool_call_count, 0) >= ?");
        arr_add_value(params, v);
    } else if (!strcmp(name, "max_tool_calls")) {
        cl_add(clauses, "COALESCE(s.tool_call_count, 0) <= ?");
        arr_add_value(params, v);
    }
    if (b.s) free(b.s);
}

/* _prune_filter_where: the shared WHERE for bulk prune/archive selection.
 * ``source_pos`` is the positional keyword when the caller supplies it; NULL
 * means "read the source filter from ``filters``". */
static json_t *filter_where(const json_t *filters, const char *source_pos, int include_open,
                            int include_pinned, int lineage_tips_only,
                            int has_archived, int archived) {
    if (filters && filters->type != JSON_OBJECT) filters = NULL;

    /* Unknown keyword guard: min(unknown) in repr, before any clause work. */
    const char *min_unknown = NULL;
    size_t nkeys = filters ? json_object_size(filters) : 0;
    for (size_t i = 0; i < nkeys; i++) {
        const char *k = json_object_get_key_at(filters, i);
        if (!k) continue;
        /* These four bind to keyword-only parameters, never to **filters. */
        if (!strcmp(k, "archived") || !strcmp(k, "include_pinned") ||
            !strcmp(k, "lineage_tips_only") || !strcmp(k, "include_open")) continue;
        int known = 0;
        for (int j = 0; j < PRUNE_FILTER_NAME_COUNT; j++)
            if (!strcmp(k, PRUNE_FILTER_NAMES[j])) { known = 1; break; }
        if (!known && (!min_unknown || strcmp(k, min_unknown) < 0)) min_unknown = k;
    }
    if (min_unknown) {
        sb_t m = {0};
        sb_add(&m, "SessionMaintenanceMixin._prune_filter_where() got an unexpected keyword argument ");
        sb_repr_str(&m, min_unknown);
        char *msg = sb_take(&m);
        json_t *o = err_obj("TypeError", msg);
        free(msg);
        return o;
    }

    cl_t clauses = {{0}, 0};
    if (!include_open) cl_add(&clauses, "s.ended_at IS NOT NULL");
    if (lineage_tips_only) {
        cl_add(&clauses, "COALESCE(s.end_reason, '') <> 'compression'");
        cl_add(&clauses, "COALESCE(s.title, '') <> 'Bot Chat'");
    }

    json_t *params = json_array();
    for (int i = 0; i < PRUNE_FILTER_COUNT; i++) {
        const char *name = PRUNE_FILTERS[i].name;
        if (!strcmp(name, "source") && source_pos) {
            json_t *tmp = json_string(source_pos);
            if (py_truthy(tmp)) pf_apply(name, tmp, &clauses, params);
            json_free(tmp);
            continue;
        }
        const json_t *v = filters ? json_obj_get(filters, name) : NULL;
        int applies = PRUNE_FILTERS[i].truthy ? py_truthy(v) : (v && v->type != JSON_NULL);
        if (!applies) continue;
        pf_apply(name, v, &clauses, params);
    }
    if (has_archived) {
        char buf[64];
        snprintf(buf, sizeof buf, "s.archived = %d", archived ? 1 : 0);
        cl_add(&clauses, buf);
    }
    if (!include_pinned) {
        sb_t b = {0};
        sb_not_pinned(&b, "s");
        cl_add(&clauses, sb_take(&b));
        free(b.s);
    }

    char *where = cl_join(&clauses);
    json_t *o = json_object();
    json_set(o, "where", json_string(where));
    json_set(o, "params", params);
    free(where);
    return o;
}

/* Reads archived/include_pinned/lineage_tips_only/include_open out of the
 * filters dict, mirroring the keyword-only signature they bind to. */
static void split_signature_keys(const json_t *filters, int *has_archived, int *archived,
                                 int *include_pinned, int *lineage_tips_only, int *include_open) {
    *has_archived = 0; *archived = 0;
    *include_pinned = 0; *lineage_tips_only = 0; *include_open = 0;
    if (!filters || filters->type != JSON_OBJECT) return;
    const json_t *a = json_obj_get(filters, "archived");
    if (a && a->type == JSON_BOOL) { *has_archived = 1; *archived = a->bool_val; }
    *include_pinned = py_truthy(json_obj_get(filters, "include_pinned"));
    *lineage_tips_only = py_truthy(json_obj_get(filters, "lineage_tips_only"));
    *include_open = py_truthy(json_obj_get(filters, "include_open"));
}

json_t *w33_maintenanc_3e1b75_p2_prune_filter_where(const json_t *req) {
    const json_t *filters = json_obj_get(req, "filters");
    int has_archived, archived, include_pinned, lineage_tips_only, include_open;
    split_signature_keys(filters, &has_archived, &archived, &include_pinned,
                         &lineage_tips_only, &include_open);
    return filter_where(filters, NULL, include_open, include_pinned, lineage_tips_only,
                        has_archived, archived);
}

json_t *w33_maintenanc_3e1b75_p2_prune_where(const json_t *req) {
    const json_t *filters = json_obj_get(req, "filters");
    if (filters && filters->type == JSON_OBJECT && json_obj_get(filters, "source"))
        return err_obj("TypeError",
            "persistence.sqlite.housekeeping.maintenance._prune_filter_where() got multiple values "
            "for keyword argument 'source'");

    double older = 0, now = 0;
    int has_older = py_number(json_obj_get(req, "older_than_days"), &older);
    py_number(json_obj_get(req, "now"), &now);
    int whole_lineages = py_truthy(json_obj_get(req, "whole_lineages"));
    const json_t *source = json_obj_get(req, "source");
    const char *source_pos = (source && source->type == JSON_STRING) ? source->str_val : NULL;

    json_t *work = (filters && filters->type == JSON_OBJECT) ? json_copy(filters) : json_object();

    int has_archived, archived, include_pinned, lineage_tips_only, include_open;
    split_signature_keys(work, &has_archived, &archived, &include_pinned,
                         &lineage_tips_only, &include_open);

    if (has_older && !json_obj_get(work, "last_active_before") && !json_obj_get(work, "started_before")) {
        if (older < 0) {
            sb_t m = {0};
            sb_add(&m, "older_than_days must be >= 0, got ");
            sb_repr_float(&m, older);
            sb_add(&m, ": a negative retention builds a future cutoff that matches every ended session.");
            char *msg = sb_take(&m);
            json_t *o = err_obj("ValueError", msg);
            free(msg);
            json_free(work);
            return o;
        }
        json_set(work, "last_active_before", json_number(now - older * 86400.0));
    }

    json_t *out = filter_where(work, source_pos, include_open, include_pinned,
                               lineage_tips_only, has_archived, archived);
    json_free(work);
    if (json_obj_get(out, "error")) return out;
    if (!whole_lineages) return out;

    const char *where = json_get_str(out, "where", "");
    sb_t b = {0};
    sb_add(&b, where);
    sb_add(&b, " AND s.id NOT IN (");
    sb_continued_ancestors(&b, where);
    sb_add(&b, ")");
    char *joined = sb_take(&b);

    const json_t *params = json_obj_get(out, "params");
    json_t *doubled = json_array();
    for (size_t i = 0; params && i < json_len(params); i++)
        json_append(doubled, json_copy(json_get(params, i)));
    for (size_t i = 0; params && i < json_len(params); i++)
        json_append(doubled, json_copy(json_get(params, i)));

    json_t *res = json_object();
    json_set(res, "where", json_string(joined));
    json_set(res, "params", doubled);
    free(joined);
    json_free(out);
    return res;
}

static const char *COUNT_SQL_PREFIX = "SELECT COUNT(*) FROM sessions s WHERE ";
static const char *ENDED_GUARD = "s.ended_at IS NOT NULL";

json_t *w33_maintenanc_3e1b75_p2_count_open_sql(const json_t *req) {
    const json_t *filters = json_obj_get(req, "filters");
    if (filters && filters->type == JSON_OBJECT && py_truthy(json_obj_get(filters, "include_open"))) {
        json_t *o = json_object();
        json_set(o, "zero", json_bool(1));
        return o;
    }
    json_t *built = w33_maintenanc_3e1b75_p2_prune_where(req);
    if (json_obj_get(built, "error")) return built;

    const char *where = json_get_str(built, "where", "");
    size_t glen = strlen(ENDED_GUARD);
    if (strncmp(where, ENDED_GUARD, glen) != 0) {
        json_free(built);
        return err_obj("RuntimeError", "prune filter lost its ended-session safety guard");
    }
    sb_t b = {0};
    sb_add(&b, COUNT_SQL_PREFIX);
    sb_add(&b, "s.ended_at IS NULL");
    sb_add(&b, where + glen);

    json_t *o = json_object();
    json_set(o, "sql", sb_out(&b));
    json_set(o, "params", json_copy(json_obj_get(built, "params")));
    json_free(built);
    return o;
}

json_t *w33_maintenanc_3e1b75_p2_prune_open_guard(int include_open) {
    json_t *o = json_object();
    if (include_open) {
        json_set(o, "error", json_string("ValueError"));
        json_set(o, "message", json_string(
            "prune_sessions() never deletes open sessions: include_open is archive-only"));
        return o;
    }
    json_set(o, "ok", json_bool(1));
    return o;
}

json_t *w33_maintenanc_3e1b75_p2_reclaim_estimate(const json_t *req) {
    double prunable = 0, total = 0, prunable_bytes = 0, total_bytes = 0;
    py_number(json_obj_get(req, "prunable"), &prunable);
    py_number(json_obj_get(req, "total"), &total);
    int measure = py_truthy(json_obj_get(req, "measure_total_bytes"));
    int have_pb = py_number(json_obj_get(req, "prunable_bytes"), &prunable_bytes);
    int have_tb = py_number(json_obj_get(req, "total_bytes"), &total_bytes);

    long long p = py_int(prunable);
    json_t *o = json_object();
    json_set(o, "prunable", json_number((double)p));
    json_set(o, "total", json_number((double)py_int(total)));
    json_set(o, "prunable_bytes", json_number((double)((p && have_pb) ? py_int(prunable_bytes) : 0)));
    if (p && measure && have_tb) json_set(o, "total_bytes", json_number((double)py_int(total_bytes)));
    else json_set(o, "total_bytes", json_null());
    return o;
}

/* f"{n:,}" */
static void sb_thousands(sb_t *b, long long n) {
    char raw[32];
    snprintf(raw, sizeof raw, "%lld", n);
    size_t len = strlen(raw);
    for (size_t i = 0; i < len; i++) {
        if (i && (len - i) % 3 == 0) sb_addc(b, ',');
        sb_addc(b, raw[i]);
    }
}

json_t *w33_maintenanc_3e1b75_p2_export_safe(const json_t *req) {
    double max_messages = 0, count = 0;
    py_number(json_obj_get(req, "max_messages"), &max_messages);
    py_number(json_obj_get(req, "message_count"), &count);
    const char *sid = json_get_str(req, "session_id", "");

    long long limit = py_int(max_messages);
    if (limit < 0) return err_obj("ValueError", "max_messages must be non-negative");
    json_t *o = json_object();
    if (limit == 0) { json_set(o, "count", json_number(0)); return o; }
    if (py_int(count) > limit) {
        sb_t m = {0};
        sb_add(&m, "Session '");
        sb_add(&m, sid);
        sb_add(&m, "' has more than ");
        sb_thousands(&m, limit);
        sb_add(&m, " exportable messages; the JSON/JSONL backup is built in memory and capped per session. "
                   "Use the dashboard Sessions page's streaming Export action, or set "
                   "sessions.max_export_messages: 0 in config.yaml to disable the guard.");
        char *msg = sb_take(&m);
        json_t *e = err_obj("ExportTooLarge", msg);
        free(msg);
        json_free(o);
        return e;
    }
    json_set(o, "count", json_number((double)py_int(count)));
    return o;
}

/* ── gate p2: predicates and reports ───────────────────────────────────── */

json_t *w33_maintenanc_3e1b75_p2_tree_key(const char *path) {
    const char *src = path ? path : "";
    sb_t b = {0};
    for (const char *p = src; *p; p++) {
        if (*p == '\\' || *p == '/') sb_addc(&b, '/');
        else sb_addc(&b, (char)tolower((unsigned char)*p));
    }
    while (b.n > 0 && b.s[b.n - 1] == '/') b.s[--b.n] = '\0';
    return sb_out(&b);
}

json_t *w33_maintenanc_3e1b75_p2_inside_tree(const json_t *req) {
    const char *wt = json_get_str(req, "worktree_path", "");
    const json_t *cwd_v = json_obj_get(req, "cwd");
    json_t *root = w33_maintenanc_3e1b75_p2_tree_key(wt);
    const char *r = root->str_val ? root->str_val : "";
    int inside = 0;
    if (cwd_v && cwd_v->type == JSON_STRING && cwd_v->str_val[0]) {
        char *ckey = py_lower(cwd_v->str_val);
        sb_t b = {0};
        for (const char *p = ckey; *p; p++) {
            if (*p == '\\' || *p == '/') sb_addc(&b, '/');
            else sb_addc(&b, *p);
        }
        while (b.n > 0 && b.s[b.n - 1] == '/') b.s[--b.n] = '\0';
        const char *c = b.s ? b.s : "";
        if (!strcmp(c, r)) inside = 1;
        else {
            size_t rlen = strlen(r);
            if (rlen > 0 && !strncmp(c, r, rlen) && c[rlen] == '/') inside = 1;
        }
        free(b.s);
        free(ckey);
    }
    json_free(root);
    return json_number(inside);
}

json_t *w33_maintenanc_3e1b75_p2_tree_serve_facts(const json_t *req) {
    int served = py_truthy(json_obj_get(req, "served"));
    json_t *o = json_object();
    json_set(o, "served", json_bool(served));
    if (served) {
        /* TreeServeFacts(served=True) keeps the default empty chat set. */
        json_set(o, "chat_ids", json_array());
        return o;
    }

    const json_t *in = json_obj_get(req, "chat_ids");
    size_t n = in ? json_len(in) : 0;
    char **ids = (char **)malloc(sizeof(char *) * (n ? n : 1));
    if (!ids) abort();
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *v = json_get(in, i);
        if (!v || v->type != JSON_STRING) continue;
        int dup = 0;
        for (size_t j = 0; j < k; j++) if (!strcmp(ids[j], v->str_val)) { dup = 1; break; }
        if (!dup) ids[k++] = v->str_val;
    }
    for (size_t i = 1; i < k; i++) {
        char *key = ids[i];
        size_t j = i;
        while (j > 0 && strcmp(ids[j - 1], key) > 0) { ids[j] = ids[j - 1]; j--; }
        ids[j] = key;
    }
    json_t *list = json_array();
    for (size_t i = 0; i < k; i++) arr_add_str(list, ids[i]);
    json_set(o, "chat_ids", list);
    free(ids);
    return o;
}

json_t *w33_maintenanc_3e1b75_p2_auto_archive_extra_set(void) {
    return json_string(AUTO_ARCHIVE_EXTRA_SET);
}

json_t *w33_maintenanc_3e1b75_p2_heartbeat_register(const json_t *req) {
    const char *backend = json_get_str(req, "backend_id", "");
    if (!backend || !*backend) {
        json_t *skip = json_object();
        json_set(skip, "skip", json_bool(1));
        return skip;
    }

    double pid = 0, started = 0, now = 0, last = 0;
    py_number(json_obj_get(req, "pid"), &pid);
    py_number(json_obj_get(req, "started_at"), &started);
    py_number(json_obj_get(req, "now"), &now);
    int have_last = py_number(json_obj_get(req, "last_heartbeat"), &last);
    double ts = have_last ? last : now;

    json_t *params = json_array();
    arr_add_str(params, backend);
    arr_add_num(params, (double)py_int(pid));
    arr_add_num(params, started);
    arr_add_num(params, ts);
    arr_add_str(params, json_get_str(req, "profile", ""));
    arr_add_str(params, json_get_str(req, "host", ""));

    json_t *o = json_object();
    json_set(o, "skip", json_bool(0));
    json_set(o, "sql", json_string(HEARTBEAT_INSERT_SQL));
    json_set(o, "params", params);
    return o;
}

json_t *w33_maintenanc_3e1b75_p2_heartbeat_delete(const json_t *req) {
    const char *backend = json_get_str(req, "backend_id", "");
    json_t *o = json_object();
    if (!backend || !*backend) {
        json_set(o, "skip", json_bool(1));
        return o;
    }
    json_t *params = json_array();
    arr_add_str(params, backend);
    json_set(o, "skip", json_bool(0));
    json_set(o, "sql", json_string(HEARTBEAT_DELETE_SQL));
    json_set(o, "params", params);
    return o;
}

json_t *w33_maintenanc_3e1b75_p2_prune_batches(const json_t *req) {
    const json_t *ids = json_obj_get(req, "ids");
    double size = 900;
    py_number(json_obj_get(req, "size"), &size);
    long long step = py_int(size);
    json_t *out = json_array();
    size_t n = ids ? json_len(ids) : 0;
    if (step <= 0) return out;
    for (size_t start = 0; start < n; start += (size_t)step) {
        size_t end = start + (size_t)step;
        if (end > n) end = n;
        json_t *chunk = json_array();
        sb_t ph = {0};
        for (size_t i = start; i < end; i++) {
            if (i > start) sb_addc(&ph, ',');
            sb_addc(&ph, '?');
            json_append(chunk, json_copy(json_get(ids, i)));
        }
        char *holders = sb_take(&ph);

        sb_t up = {0};
        sb_add(&up, "UPDATE sessions SET parent_session_id = NULL WHERE parent_session_id IN (");
        sb_add(&up, holders);
        sb_add(&up, ")");
        sb_t dm = {0};
        sb_add(&dm, "DELETE FROM messages WHERE session_id IN (");
        sb_add(&dm, holders);
        sb_add(&dm, ")");
        sb_t ds = {0};
        sb_add(&ds, "DELETE FROM sessions WHERE id IN (");
        sb_add(&ds, holders);
        sb_add(&ds, ")");

        json_t *entry = json_object();
        json_set(entry, "placeholders", json_string(holders));
        json_set(entry, "update_parent_sql", sb_out(&up));
        json_set(entry, "delete_messages_sql", sb_out(&dm));
        json_set(entry, "delete_sessions_sql", sb_out(&ds));
        json_set(entry, "params", chunk);
        json_append(out, entry);
        free(holders);
    }
    return out;
}

json_t *w33_maintenanc_3e1b75_p2_retained_tree_sql(int pending_only) {
    sb_t b = {0};
    sb_add(&b, "SELECT 1 FROM sessions s WHERE hermes_inside_tree(s.cwd) AND (");
    if (!pending_only) sb_add(&b, "s.message_count>0 OR ");
    sb_add(&b, "EXISTS (SELECT 1 FROM session_admissions a WHERE a.target_session_id=s.id "
               "AND a.status!='terminal')) LIMIT 1");
    return sb_out(&b);
}

json_t *w33_maintenanc_3e1b75_p2_served_tree_sql(void) {
    return json_string(SERVED_TREE_SQL);
}
