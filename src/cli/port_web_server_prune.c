/*
 * port_web_server_prune.c — session prune engine.
 * Faithful port of SessionDB._prune_filter_where, list_prune_candidates,
 * prune_sessions (hermes_state.py) and the _prune_sessions endpoint
 * wrapper (hermes_cli/web_server.py).
 */

#include "web_server_prune.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hermes_json.h"
#include "sqlite3.h"

#define LAST_ACTIVE_EXPR \
    "COALESCE((SELECT MAX(m.timestamp) FROM messages m " \
    "WHERE m.session_id = s.id), s.started_at)"

typedef struct {
    char sql[4096];
    /* param slots, in order */
    struct { bool is_num; double num; const char *str; char strbuf[1024]; } p[32];
    int np;
} where_t;

static void add_clause(where_t *w, const char *clause) {
    if (w->sql[0]) strcat(w->sql, " AND ");
    strcat(w->sql, clause);
}

static void add_num(where_t *w, double v) {
    w->p[w->np].is_num = true;
    w->p[w->np].num = v;
    w->np++;
}

static void add_str(where_t *w, const char *s) {
    w->p[w->np].is_num = false;
    snprintf(w->p[w->np].strbuf, sizeof(w->p[w->np].strbuf), "%s", s);
    w->p[w->np].str = w->p[w->np].strbuf;
    w->np++;
}

static void add_lower_pat(where_t *w, const char *s) {
    char buf[1024];
    size_t j = 0;
    buf[j++] = '%';
    for (const char *c = s; *c && j < sizeof(buf) - 2; c++)
        buf[j++] = (char)tolower((unsigned char)*c);
    buf[j++] = '%';
    buf[j] = '\0';
    add_str(w, buf);
}

static void add_lower(where_t *w, const char *s) {
    char buf[1024];
    size_t j = 0;
    for (const char *c = s; *c && j < sizeof(buf) - 1; c++)
        buf[j++] = (char)tolower((unsigned char)*c);
    buf[j] = '\0';
    add_str(w, buf);
}

/* PoP: ws_prune_filter_where @ hermes_state.py:_prune_filter_where */
static void build_where(where_t *w, const ws_prune_filters_t *f) {
    w->sql[0] = '\0';
    w->np = 0;
    add_clause(w, "s.ended_at IS NOT NULL");
    if (!f) return;
    if (f->has_last_active_before) {
        add_clause(w, LAST_ACTIVE_EXPR " < ?");
        add_num(w, f->last_active_before);
    }
    if (f->has_last_active_after) {
        add_clause(w, LAST_ACTIVE_EXPR " >= ?");
        add_num(w, f->last_active_after);
    }
    if (f->has_started_before) {
        add_clause(w, "s.started_at < ?");
        add_num(w, f->started_before);
    }
    if (f->has_started_after) {
        add_clause(w, "s.started_at >= ?");
        add_num(w, f->started_after);
    }
    if (f->source && f->source[0]) {
        add_clause(w, "s.source = ?");
        add_str(w, f->source);
    }
    if (f->title_like && f->title_like[0]) {
        add_clause(w, "LOWER(COALESCE(s.title, '')) LIKE ?");
        add_lower_pat(w, f->title_like);
    }
    if (f->end_reason && f->end_reason[0]) {
        add_clause(w, "s.end_reason = ?");
        add_str(w, f->end_reason);
    }
    if (f->cwd_prefix && f->cwd_prefix[0]) {
        /* _cwd_prefix_clause: prefix = cwd_prefix.rstrip("/\\") or cwd_prefix */
        char prefix[1024];
        snprintf(prefix, sizeof(prefix), "%s", f->cwd_prefix);
        size_t L = strlen(prefix);
        while (L > 0 && (prefix[L - 1] == '/' || prefix[L - 1] == '\\'))
            prefix[--L] = '\0';
        if (L == 0) snprintf(prefix, sizeof(prefix), "%s", f->cwd_prefix);
        add_clause(w, "(s.cwd = ? OR s.cwd LIKE ? OR s.cwd LIKE ?)");
        add_str(w, prefix);
        char pat[1040];
        snprintf(pat, sizeof(pat), "%s/%%", prefix);
        add_str(w, pat);
        snprintf(pat, sizeof(pat), "%s\\%%", prefix);
        add_str(w, pat);
    }
    if (f->has_min_messages) {
        add_clause(w, "s.message_count >= ?");
        add_num(w, f->min_messages);
    }
    if (f->has_max_messages) {
        add_clause(w, "s.message_count <= ?");
        add_num(w, f->max_messages);
    }
    if (f->model_like && f->model_like[0]) {
        add_clause(w, "LOWER(COALESCE(s.model, '')) LIKE ?");
        add_lower_pat(w, f->model_like);
    }
    if (f->provider && f->provider[0]) {
        add_clause(w, "LOWER(COALESCE(s.billing_provider, '')) = ?");
        add_lower(w, f->provider);
    }
    if (f->user_id && f->user_id[0]) {
        add_clause(w, "s.user_id = ?");
        add_str(w, f->user_id);
    }
    if (f->chat_id && f->chat_id[0]) {
        add_clause(w, "s.chat_id = ?");
        add_str(w, f->chat_id);
    }
    if (f->chat_type && f->chat_type[0]) {
        add_clause(w, "s.chat_type = ?");
        add_str(w, f->chat_type);
    }
    if (f->branch_like && f->branch_like[0]) {
        add_clause(w, "LOWER(COALESCE(s.git_branch, '')) LIKE ?");
        add_lower_pat(w, f->branch_like);
    }
    if (f->has_min_tokens) {
        add_clause(w, "(COALESCE(s.input_tokens, 0) + COALESCE(s.output_tokens, 0)) >= ?");
        add_num(w, (double)f->min_tokens);
    }
    if (f->has_max_tokens) {
        add_clause(w, "(COALESCE(s.input_tokens, 0) + COALESCE(s.output_tokens, 0)) <= ?");
        add_num(w, (double)f->max_tokens);
    }
    if (f->has_min_cost) {
        add_clause(w, "COALESCE(s.actual_cost_usd, s.estimated_cost_usd, 0) >= ?");
        add_num(w, f->min_cost);
    }
    if (f->has_max_cost) {
        add_clause(w, "COALESCE(s.actual_cost_usd, s.estimated_cost_usd, 0) <= ?");
        add_num(w, f->max_cost);
    }
    if (f->has_min_tool_calls) {
        add_clause(w, "COALESCE(s.tool_call_count, 0) >= ?");
        add_num(w, f->min_tool_calls);
    }
    if (f->has_max_tool_calls) {
        add_clause(w, "COALESCE(s.tool_call_count, 0) <= ?");
        add_num(w, f->max_tool_calls);
    }
    if (f->archived == 1) add_clause(w, "s.archived = 1");
    else if (f->archived == 0) add_clause(w, "s.archived = 0");
}

static void bind_where(sqlite3_stmt *st, const where_t *w, int start) {
    for (int i = 0; i < w->np; i++) {
        if (w->p[i].is_num) sqlite3_bind_double(st, start + i, w->p[i].num);
        else sqlite3_bind_text(st, start + i, w->p[i].str, -1, SQLITE_TRANSIENT);
    }
}

/* Apply the implicit-inactivity default shared by candidates/prune. */
static ws_prune_filters_t apply_default_cutoff(const ws_prune_filters_t *f,
                                               bool has_older,
                                               double older_than_days) {
    ws_prune_filters_t out = f ? *f : (ws_prune_filters_t){.archived = -1};
    if (!out.has_last_active_before && !out.has_started_before && has_older) {
        out.has_last_active_before = true;
        out.last_active_before =
            (double)time(NULL) - older_than_days * 86400.0;
    }
    return out;
}

static json_t *col_to_json(sqlite3_stmt *st, int i) {
    switch (sqlite3_column_type(st, i)) {
    case SQLITE_NULL: return json_null();
    case SQLITE_INTEGER:
        return json_number((double)sqlite3_column_int64(st, i));
    case SQLITE_FLOAT: return json_number(sqlite3_column_double(st, i));
    default: {
        const unsigned char *t = sqlite3_column_text(st, i);
        return json_string(t ? (const char *)t : "");
    }
    }
}

/* ── list_prune_candidates ──────────────────────────────────────────────── */
/* PoP: ws_prune_candidates @ hermes_state.py:list_prune_candidates */
json_t *ws_prune_candidates(const char *db_path, bool has_older,
                            double older_than_days,
                            const ws_prune_filters_t *f) {
    json_t *rows = json_array();
    ws_prune_filters_t eff = apply_default_cutoff(f, has_older,
                                                  older_than_days);
    where_t w;
    build_where(&w, &eff);

    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return rows;
    }
    char sql[6144];
    snprintf(sql, sizeof(sql),
             "SELECT s.id, s.source, s.title, s.model, s.started_at, "
             LAST_ACTIVE_EXPR " AS last_active, "
             "s.ended_at, s.message_count, s.archived "
             "FROM sessions s WHERE %s "
             "ORDER BY last_active ASC, s.started_at ASC",
             w.sql);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        bind_where(st, &w, 1);
        static const char *cols[] = {"id", "source", "title", "model",
                                     "started_at", "last_active", "ended_at",
                                     "message_count", "archived"};
        while (sqlite3_step(st) == SQLITE_ROW) {
            json_t *row = json_object();
            for (int i = 0; i < 9; i++)
                json_set(row, cols[i], col_to_json(st, i));
            json_append(rows, row);
        }
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    return rows;
}

/* ── prune_sessions ─────────────────────────────────────────────────────── */
/* PoP: ws_prune_sessions @ hermes_state.py:prune_sessions */
int ws_prune_sessions(const char *db_path, bool has_older,
                      double older_than_days, const ws_prune_filters_t *f) {
    ws_prune_filters_t eff = apply_default_cutoff(f, has_older,
                                                  older_than_days);
    where_t w;
    build_where(&w, &eff);

    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return -1;
    }
    sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);

    char sql[6144];
    snprintf(sql, sizeof(sql), "SELECT s.id FROM sessions s WHERE %s", w.sql);
    char **ids = NULL;
    size_t n = 0, cap = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        bind_where(st, &w, 1);
        while (sqlite3_step(st) == SQLITE_ROW) {
            if (n == cap) {
                cap = cap ? cap * 2 : 16;
                ids = realloc(ids, cap * sizeof *ids);
            }
            const unsigned char *t = sqlite3_column_text(st, 0);
            ids[n++] = strdup(t ? (const char *)t : "");
        }
    }
    sqlite3_finalize(st);

    if (n == 0) {
        sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
        free(ids);
        sqlite3_close(db);
        return 0;
    }

    size_t up_len = 128 + n * 2;
    char *up = malloc(up_len);
    strcpy(up, "UPDATE sessions SET parent_session_id = NULL "
               "WHERE parent_session_id IN (");
    for (size_t i = 0; i < n; i++) strcat(up, i + 1 < n ? "?," : "?");
    strcat(up, ")");
    if (sqlite3_prepare_v2(db, up, -1, &st, NULL) == SQLITE_OK) {
        for (size_t i = 0; i < n; i++)
            sqlite3_bind_text(st, (int)i + 1, ids[i], -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
    }
    sqlite3_finalize(st);
    free(up);

    for (size_t i = 0; i < n; i++) {
        if (sqlite3_prepare_v2(db, "DELETE FROM messages WHERE session_id = ?",
                               -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, ids[i], -1, SQLITE_TRANSIENT);
            sqlite3_step(st);
        }
        sqlite3_finalize(st);
        if (sqlite3_prepare_v2(db, "DELETE FROM sessions WHERE id = ?",
                               -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, ids[i], -1, SQLITE_TRANSIENT);
            sqlite3_step(st);
        }
        sqlite3_finalize(st);
    }
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    for (size_t i = 0; i < n; i++) free(ids[i]);
    free(ids);
    sqlite3_close(db);
    return (int)n;
}

/* ── endpoint wrapper ───────────────────────────────────────────────────── */

static bool get_opt_num(const json_t *body, const char *key, double *out) {
    json_t *v = body ? json_object_get((json_t *)body, key) : NULL;
    if (!v || v->type != JSON_NUMBER) return false;
    *out = v->num_val;
    return true;
}

static const char *get_opt_str(const json_t *body, const char *key) {
    json_t *v = body ? json_object_get((json_t *)body, key) : NULL;
    if (!v || v->type != JSON_STRING || !v->str_val[0]) return NULL;
    return v->str_val;
}

/* PoP: ws_prune_endpoint @ hermes_cli/web_server.py:_prune_sessions */
json_t *ws_prune_endpoint(const char *db_path, const json_t *body) {
    /* pydantic: default 90; explicit null → None (no cutoff) but still
     * counts as explicitly-set for the suppression rule. */
    json_t *otd = body ? json_object_get((json_t *)body, "older_than_days") : NULL;
    bool older_explicit = otd != NULL; /* key present in body */
    bool older_is_null = otd && otd->type == JSON_NULL;
    double older_than_days = 90;
    if (otd && otd->type == JSON_NUMBER) older_than_days = otd->num_val;

    double sb = 0, sa = 0;
    bool has_sb = get_opt_num(body, "started_before", &sb);
    bool has_sa = get_opt_num(body, "started_after", &sa);
    bool has_window = has_sb || has_sa;

    if (!older_is_null && (!older_explicit || otd->type == JSON_NUMBER) &&
        older_than_days < 1 && !has_window) {
        json_t *err = json_object();
        json_set(err, "status", json_number(400));
        json_set(err, "detail", json_string("older_than_days must be >= 1"));
        return err;
    }

    ws_prune_filters_t f = {.archived = -1};
    f.has_started_before = has_sb; f.started_before = sb;
    f.has_started_after = has_sa; f.started_after = sa;
    f.source = get_opt_str(body, "source");
    f.title_like = get_opt_str(body, "title_like");
    f.end_reason = get_opt_str(body, "end_reason");
    f.cwd_prefix = get_opt_str(body, "cwd_prefix");
    f.model_like = get_opt_str(body, "model_like");
    f.provider = get_opt_str(body, "provider");
    f.user_id = get_opt_str(body, "user_id");
    f.chat_id = get_opt_str(body, "chat_id");
    f.chat_type = get_opt_str(body, "chat_type");
    f.branch_like = get_opt_str(body, "branch_like");
    double d;
    if (get_opt_num(body, "min_messages", &d)) { f.has_min_messages = true; f.min_messages = (int)d; }
    if (get_opt_num(body, "max_messages", &d)) { f.has_max_messages = true; f.max_messages = (int)d; }
    if (get_opt_num(body, "min_tokens", &d)) { f.has_min_tokens = true; f.min_tokens = (long long)d; }
    if (get_opt_num(body, "max_tokens", &d)) { f.has_max_tokens = true; f.max_tokens = (long long)d; }
    if (get_opt_num(body, "min_cost", &d)) { f.has_min_cost = true; f.min_cost = d; }
    if (get_opt_num(body, "max_cost", &d)) { f.has_max_cost = true; f.max_cost = d; }
    if (get_opt_num(body, "min_tool_calls", &d)) { f.has_min_tool_calls = true; f.min_tool_calls = (int)d; }
    if (get_opt_num(body, "max_tool_calls", &d)) { f.has_max_tool_calls = true; f.max_tool_calls = (int)d; }

    /* archived = None if include_archived else False */
    json_t *ia = body ? json_object_get((json_t *)body, "include_archived") : NULL;
    bool include_archived = ia && ((ia->type == JSON_BOOL && ia->bool_val) ||
                                   (ia->type == JSON_NUMBER && ia->num_val != 0));
    f.archived = include_archived ? -1 : 0;

    /* _attr_filters_set: any attribute filter is not None. Numeric fields
     * count when present in the body (pydantic sets them from JSON). */
    bool attr_set =
        f.source || f.title_like || f.end_reason || f.cwd_prefix ||
        f.has_min_messages || f.has_max_messages || f.model_like ||
        f.provider || f.user_id || f.chat_id || f.chat_type ||
        f.branch_like || f.has_min_tokens || f.has_max_tokens ||
        f.has_min_cost || f.has_max_cost || f.has_min_tool_calls ||
        f.has_max_tool_calls;

    /* Effective older_than: body value (null → None) or default 90; then
     * suppressed by a window or by attr filters without explicit older. */
    bool has_older = !older_is_null;
    double eff_older = older_than_days;
    if (has_window || (attr_set && !older_explicit)) has_older = false;

    json_t *dr = body ? json_object_get((json_t *)body, "dry_run") : NULL;
    bool dry_run = dr && ((dr->type == JSON_BOOL && dr->bool_val) ||
                          (dr->type == JSON_NUMBER && dr->num_val != 0));

    if (dry_run) {
        json_t *rows = ws_prune_candidates(db_path, has_older, eff_older, &f);
        size_t nr = json_len(rows);
        json_t *out = json_object();
        json_set(out, "ok", json_bool(true));
        json_set(out, "removed", json_number(0));
        json_set(out, "matched", json_number((double)nr));
        if (nr > 0) {
            json_t *first = json_get(rows, 0);
            json_t *last = json_get(rows, nr - 1);
            json_set(out, "oldest_started_at",
                     json_copy(json_object_get(first, "started_at")));
            json_set(out, "newest_started_at",
                     json_copy(json_object_get(last, "started_at")));
        } else {
            json_set(out, "oldest_started_at", json_null());
            json_set(out, "newest_started_at", json_null());
        }
        json_t *sessions = json_array();
        static const char *keep[] = {"id", "source", "title", "model",
                                         "started_at", "message_count"};
        for (size_t i = 0; i < nr; i++) {
            json_t *r = json_get(rows, i);
            json_t *slim = json_object();
            for (int k = 0; k < 6; k++) {
                json_t *v = json_object_get(r, keep[k]);
                json_set(slim, keep[k], v ? json_copy(v) : json_null());
            }
            json_append(sessions, slim);
        }
        json_set(out, "sessions", sessions);
        json_free(rows);
        return out;
    }

    int removed = ws_prune_sessions(db_path, has_older, eff_older, &f);
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    json_set(out, "removed", json_number(removed < 0 ? 0 : removed));
    return out;
}
