/*
 * _schema.c - C11 port of upstream persistence/sqlite/_schema.py
 * (gates w27_schema_3eeff1_p1, w27_schema_3eeff1_p2, w27_schema_3eeff1_p3).
 *
 * PoP: w27_schema_3eeff1_positive_int @ persistence/sqlite/_schema.py:_positive_int
 * PoP: w27_schema_3eeff1_quote_ident @ persistence/sqlite/_schema.py:_q
 * PoP: w27_schema_3eeff1_legacy_inline_reinsert_sql @ persistence/sqlite/_schema.py:_legacy_inline_reinsert_sql
 * PoP: w27_schema_3eeff1_fts_update_trigger_needs_narrowing @ persistence/sqlite/_schema.py:_fts_update_trigger_needs_narrowing
 * PoP: w27_schema_3eeff1_statement_complete @ persistence/sqlite/_schema.py:_execute_ddl_*:sqlite3.complete_statement
 * PoP: w27_schema_3eeff1_ddl_script_statements @ persistence/sqlite/_schema.py:_execute_ddl_script_transactional
 * PoP: w27_schema_3eeff1_ddl_skipping_settled @ persistence/sqlite/_schema.py:_execute_ddl_skipping_settled_triggers
 * PoP: w27_schema_3eeff1_constants @ persistence/sqlite/_schema.py:<module constants>
 * PoP: w27_schema_3eeff1_fts_index_is_misaligned_source @ persistence/sqlite/_schema.py:_fts_index_is_misaligned_source
 * PoP: w27_schema_3eeff1_live_pk_columns @ persistence/sqlite/_schema.py:_live_pk_columns
 * PoP: w27_schema_3eeff1_rebuild_table_statements @ persistence/sqlite/_schema.py:_rebuild_table_statements
 * PoP: w27_schema_3eeff1_rebuild_table @ persistence/sqlite/_schema.py:_rebuild_table
 * PoP: w27_schema_3eeff1_rebuild_fts_indexes @ persistence/sqlite/_schema.py:_rebuild_fts_indexes
 * PoP: w27_schema_3eeff1_drop_fts_triggers @ persistence/sqlite/_schema.py:_drop_fts_triggers
 * PoP: w27_schema_3eeff1_fts_triggers_missing @ persistence/sqlite/_schema.py:_fts_triggers_missing
 * PoP: w27_schema_3eeff1_is_trigram_unavailable_error @ persistence/sqlite/_schema.py:_is_trigram_unavailable_error
 * PoP: w27_schema_3eeff1_is_fts5_unavailable_error @ persistence/sqlite/_schema.py:_is_fts5_unavailable_error
 * PoP: w27_schema_3eeff1_db_has_legacy_inline_fts @ persistence/sqlite/_schema.py:_db_has_legacy_inline_fts
 * PoP: w27_schema_3eeff1_db_has_trigram_tool_calls_projection @ persistence/sqlite/_schema.py:_db_has_trigram_tool_calls_projection
 * PoP: w27_schema_3eeff1_title_rank @ persistence/sqlite/_schema.py:_title_rank
 * PoP: w27_schema_3eeff1_has_fts_trash @ persistence/sqlite/_schema.py:_has_fts_trash
 * PoP: w27_schema_3eeff1_retired_cjk_index_present @ persistence/sqlite/_schema.py:retired_cjk_index_present
 * PoP: w27_schema_3eeff1_next_title_in_lineage @ persistence/sqlite/_schema.py:next_title_in_lineage
 *
 * SEAMS (deliberately not ported: they are the connection, not the decision):
 *   the writer connection / read pool (sqlite3 handle), the cross-process FTS
 *   rebuild admission lease, psutil-backed orphan reaping, the on-disk column
 *   cache and the sessions.json backfill, logging and the startup watchdog.
 *   What this file owns is what the schema builder decides from VALUES: the
 *   exact SQL text it pins, the statement splitter every DDL script is fed
 *   through, the drop/create elision for settled triggers, PK repair decisions
 *   from PRAGMA rows, capability predicates over stored CREATE text, and the
 *   title-lineage numbering.
 *
 * Every string builder here is byte-for-byte pinned to the upstream text
 * (whitespace included): the SQL trace harness compares them verbatim.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"

/* ── growable buffer ─────────────────────────────────────────────────── */
typedef struct { char *p; size_t n, cap; } ws_sb;

static void ws_sb_reserve(ws_sb *b, size_t extra) {
    if (b->n + extra + 1 <= b->cap) return;
    size_t cap = b->cap ? b->cap : 64;
    while (cap < b->n + extra + 1) cap *= 2;
    char *np = (char *)realloc(b->p, cap);
    if (!np) { fputs("w27_schema: OOM\n", stderr); exit(1); }
    b->p = np;
    b->cap = cap;
}

static void ws_sb_addn(ws_sb *b, const char *s, size_t len) {
    ws_sb_reserve(b, len);
    if (len) memcpy(b->p + b->n, s, len);
    b->n += len;
    b->p[b->n] = '\0';
}

static void ws_sb_add(ws_sb *b, const char *s) { ws_sb_addn(b, s, strlen(s)); }

static void ws_sb_addc(ws_sb *b, char c) { ws_sb_addn(b, &c, 1); }

static void ws_sb_spaces(ws_sb *b, int n) { for (int i = 0; i < n; i++) ws_sb_addc(b, ' '); }

static void ws_sb_addf(ws_sb *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    int k = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (k < 0) return;
    if ((size_t)k < sizeof tmp) { ws_sb_addn(b, tmp, (size_t)k); return; }
    char *big = (char *)malloc((size_t)k + 1);
    if (!big) { fputs("w27_schema: OOM\n", stderr); exit(1); }
    va_start(ap, fmt);
    vsnprintf(big, (size_t)k + 1, fmt, ap);
    va_end(ap);
    ws_sb_addn(b, big, (size_t)k);
    free(big);
}

/* Hand the buffer's bytes to the caller; the buffer is reset. */
static char *ws_sb_take(ws_sb *b) {
    if (!b->p) { b->p = (char *)calloc(1, 1); if (!b->p) exit(1); }
    char *p = b->p;
    b->p = NULL;
    b->n = b->cap = 0;
    return p;
}

static char *ws_dup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) { fputs("w27_schema: OOM\n", stderr); exit(1); }
    memcpy(p, s, n + 1);
    return p;
}

/* ASCII uppercase copy: str.upper() over the range the pinned SQL uses. */
static char *ws_dup_upper(const char *s) {
    char *p = ws_dup(s);
    for (char *q = p; *q; q++) *q = (char)toupper((unsigned char)*q);
    return p;
}

/* Python str.strip() over the ASCII whitespace set. */
static char *ws_strip_dup(const char *s, size_t len) {
    size_t a = 0, b = len;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *r = (char *)malloc(b - a + 1);
    if (!r) { fputs("w27_schema: OOM\n", stderr); exit(1); }
    if (b > a) memcpy(r, s + a, b - a);
    r[b - a] = '\0';
    return r;
}

/* {"error": kind, "message": text} — the shape every raising builder returns. */
static json_t *ws_error_obj(const char *kind, const char *message) {
    json_t *o = json_object();
    json_set(o, "error", json_string(kind ? kind : ""));
    json_set(o, "message", json_string(message ? message : ""));
    return o;
}

/* ── statement list ──────────────────────────────────────────────────── */
typedef struct { char **p; size_t n, cap; } ws_slist;

static void ws_slist_push(ws_slist *l, char *owned) {
    if (l->n == l->cap) {
        size_t cap = l->cap ? l->cap * 2 : 8;
        char **np = (char **)realloc(l->p, cap * sizeof *np);
        if (!np) { fputs("w27_schema: OOM\n", stderr); exit(1); }
        l->p = np;
        l->cap = cap;
    }
    l->p[l->n++] = owned;
}

static void ws_slist_push_copy(ws_slist *l, const char *s) { ws_slist_push(l, ws_dup(s)); }

static json_t *ws_slist_json(ws_slist *l) {
    json_t *arr = json_array();
    for (size_t i = 0; i < l->n; i++) {
        json_append(arr, json_string(l->p[i]));
        free(l->p[i]);
    }
    free(l->p);
    l->p = NULL;
    l->n = l->cap = 0;
    return arr;
}

static void ws_slist_free(ws_slist *l) {
    for (size_t i = 0; i < l->n; i++) free(l->p[i]);
    free(l->p);
    l->p = NULL;
    l->n = l->cap = 0;
}

/* ── SQL statement completeness (sqlite3.complete_statement) ──────────── */
static int ws_is_ident(unsigned char c) {
    return isalnum(c) || c == '_' || c == '$' || c >= 0x80;
}

/*
 * True when *s* holds one complete SQL statement: the last significant token
 * is a top-level ';'.  A CREATE [TEMP] TRIGGER body runs until its BEGIN..END
 * nesting closes, so the semicolons inside it do not terminate the statement
 * — the same rule sqlite3.complete_statement applies.
 */
int w27_schema_3eeff1_statement_complete(const char *s) {
    size_t i = 0, n = s ? strlen(s) : 0;
    int depth = 0;        /* BEGIN/CASE nesting inside a trigger body */
    int in_trigger = 0;   /* after CREATE [TEMP] TRIGGER, before its BEGIN */
    int body = 0;         /* inside the trigger body */
    int expect_trigger = 0;
    int last_semi = 0;

    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (isspace(c)) { i++; continue; }
        if (c == '-' && i + 1 < n && s[i + 1] == '-') {
            while (i < n && s[i] != '\n') i++;
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(s[i] == '*' && s[i + 1] == '/')) i++;
            i = (i + 1 < n) ? i + 2 : n;
            continue;
        }
        if (c == ';') {
            if (!body || depth == 0) {
                last_semi = 1;
                expect_trigger = 0;
            }
            i++;
            continue;
        }
        if (c == '\'' || c == '"' || c == '`' || c == '[') {
            char close = (c == '[') ? ']' : (char)c;
            i++;
            while (i < n) {
                if ((unsigned char)s[i] == (unsigned char)close) {
                    if (close != ']' && i + 1 < n && s[i + 1] == close) { i += 2; continue; }
                    i++;
                    break;
                }
                i++;
            }
            last_semi = 0;
            expect_trigger = 0;
            continue;
        }
        if (ws_is_ident(c)) {
            size_t start = i;
            char word[16];
            size_t wl;
            while (i < n && ws_is_ident((unsigned char)s[i])) i++;
            wl = i - start;
            if (wl >= sizeof word) wl = sizeof word - 1;
            for (size_t k = 0; k < wl; k++) word[k] = (char)toupper((unsigned char)s[start + k]);
            word[wl] = '\0';
            if (!body && !in_trigger && !strcmp(word, "CREATE")) {
                expect_trigger = 1;
            } else if (expect_trigger && (!strcmp(word, "TEMP") || !strcmp(word, "TEMPORARY"))) {
                /* CREATE TEMP TRIGGER: keep waiting for TRIGGER */
            } else if (expect_trigger && !strcmp(word, "TRIGGER")) {
                in_trigger = 1;
                expect_trigger = 0;
            } else if (in_trigger && !strcmp(word, "BEGIN")) {
                body = 1;
                depth = 1;
                in_trigger = 0;
            } else if (body && !strcmp(word, "BEGIN")) {
                depth++;
            } else if (body && !strcmp(word, "CASE")) {
                depth++;
            } else if (body && !strcmp(word, "END")) {
                if (depth > 0) depth--;
            } else {
                expect_trigger = 0;
            }
            last_semi = 0;
            continue;
        }
        /* any other punctuation is content */
        i++;
        last_semi = 0;
        expect_trigger = 0;
    }
    return last_semi;
}

/*
 * Walk *ddl* line by line exactly as the upstream helpers do
 * (``for line in ddl.splitlines(): statement += line + "\n"``), handing every
 * complete statement to *sink*.  Returns 0 on success, -1 when a tail is left
 * unterminated.
 */
static int ws_ddl_statements(const char *ddl, ws_slist *sink, char **incomplete, int strip) {
    size_t i = 0, n = ddl ? strlen(ddl) : 0;
    ws_sb stmt;
    memset(&stmt, 0, sizeof stmt);
    *incomplete = NULL;

    while (i < n) {
        size_t start = i, end;
        while (i < n && ddl[i] != '\n' && ddl[i] != '\r') i++;
        end = i;
        if (i < n) {
            if (ddl[i] == '\r' && i + 1 < n && ddl[i + 1] == '\n') i += 2;
            else i += 1;
        }
        ws_sb_addn(&stmt, ddl + start, end - start);
        ws_sb_addc(&stmt, '\n');
        if (!w27_schema_3eeff1_statement_complete(stmt.p)) continue;
        if (strip) {
            ws_slist_push(sink, ws_strip_dup(stmt.p, stmt.n));
        } else {
            char *raw = (char *)malloc(stmt.n + 1);
            if (!raw) exit(1);
            memcpy(raw, stmt.p, stmt.n);
            raw[stmt.n] = '\0';
            ws_slist_push(sink, raw);
        }
        stmt.n = 0;
        stmt.p[0] = '\0';
    }
    if (stmt.p) {
        char *tail = ws_strip_dup(stmt.p, stmt.n);
        if (*tail) *incomplete = tail; else free(tail);
        free(stmt.p);
    }
    return *incomplete ? -1 : 0;
}

/* ── p1: _positive_int ───────────────────────────────────────────────── */
/*
 * kind: "int" | "float" | "bool" | "str" | "null" | "list" | "dict" — the
 * Python type of the value.  Only "int" clears isinstance(value, int); bool is
 * tested first upstream and rejected even though it subclasses int.
 */
json_t *w27_schema_3eeff1_positive_int(const char *name, const char *kind, double num, const char *text) {
    const char *nm = name ? name : "";
    (void)text;
    if (!kind || strcmp(kind, "int") != 0) {
        ws_sb b;
        memset(&b, 0, sizeof b);
        ws_sb_addf(&b, "%s must be an integer", nm);
        json_t *o = ws_error_obj("TypeError", b.p);
        free(b.p);
        return o;
    }
    if (num <= 0) {
        ws_sb b;
        memset(&b, 0, sizeof b);
        ws_sb_addf(&b, "%s must be greater than zero", nm);
        json_t *o = ws_error_obj("ValueError", b.p);
        free(b.p);
        return o;
    }
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    return o;
}

/* ── p1: _q ──────────────────────────────────────────────────────────── */
char *w27_schema_3eeff1_quote_ident(const char *ident) {
    ws_sb b;
    memset(&b, 0, sizeof b);
    ws_sb_addc(&b, '"');
    for (const char *p = ident ? ident : ""; *p; p++) {
        if (*p == '"') ws_sb_addc(&b, '"');
        ws_sb_addc(&b, *p);
    }
    ws_sb_addc(&b, '"');
    return ws_sb_take(&b);
}

/* ── p1: _legacy_inline_reinsert_sql ─────────────────────────────────── */
char *w27_schema_3eeff1_legacy_inline_reinsert_sql(const char *table, int indent, int delete_first) {
    const char *tbl = table ? table : "";
    ws_sb b;
    memset(&b, 0, sizeof b);
    ws_sb_addc(&b, '\n');
    if (delete_first) {
        ws_sb_spaces(&b, indent);
        ws_sb_addf(&b, "DELETE FROM %s;\n", tbl);
    }
    ws_sb_spaces(&b, indent);
    ws_sb_addf(&b, "INSERT INTO %s(rowid, content)\n", tbl);
    ws_sb_spaces(&b, indent);
    ws_sb_add(&b, "SELECT id,\n");
    ws_sb_spaces(&b, indent);
    ws_sb_add(&b, "       COALESCE(content, '') || ' ' ||\n");
    ws_sb_spaces(&b, indent);
    ws_sb_add(&b, "       COALESCE(tool_name, '') || ' ' ||\n");
    ws_sb_spaces(&b, indent);
    ws_sb_add(&b, "       COALESCE(tool_calls, '')\n");
    ws_sb_spaces(&b, indent);
    ws_sb_add(&b, "FROM messages;\n");
    if (indent > 4) ws_sb_spaces(&b, indent - 4);   /* pad[:-4]; "" when shorter */
    return ws_sb_take(&b);
}

/* ── p1: _fts_update_trigger_needs_narrowing ─────────────────────────── */
int w27_schema_3eeff1_fts_update_trigger_needs_narrowing(const char *sql) {
    if (!sql || !*sql) return 0;
    ws_sb compact;
    memset(&compact, 0, sizeof compact);
    int pending_space = 0, wrote = 0;
    for (const char *p = sql; *p; p++) {
        if (isspace((unsigned char)*p)) { pending_space = 1; continue; }
        if (pending_space && wrote) ws_sb_addc(&compact, ' ');
        pending_space = 0;
        ws_sb_addc(&compact, (char)toupper((unsigned char)*p));
        wrote = 1;
    }
    if (!compact.p) return 0;
    int needs = !strstr(compact.p, "AFTER UPDATE OF ") && strstr(compact.p, "AFTER UPDATE ON ") != NULL;
    free(compact.p);
    return needs;
}

/* ── p1: _execute_ddl_script_transactional ───────────────────────────── */
json_t *w27_schema_3eeff1_ddl_script_statements(const char *ddl) {
    ws_slist list;
    memset(&list, 0, sizeof list);
    char *incomplete = NULL;
    if (ws_ddl_statements(ddl, &list, &incomplete, 0) != 0) {
        json_t *o = ws_error_obj("OperationalError", "incomplete FTS DDL statement");
        free(incomplete);
        ws_slist_free(&list);
        return o;
    }
    json_t *o = json_object();
    json_set(o, "statements", ws_slist_json(&list));
    return o;
}

/* ── p1: _execute_ddl_skipping_settled_triggers ──────────────────────── */
/* Split on whitespace with maxsplit semantics; returns malloc'd token *idx*. */
static char *ws_nth_word(const char *s, int idx, int maxsplit) {
    const char *p = s;
    int seen = 0;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) return NULL;
        const char *start = p;
        if (seen == maxsplit) p += strlen(p);
        else while (*p && !isspace((unsigned char)*p)) p++;
        if (seen == idx) {
            size_t len = (size_t)(p - start);
            char *w = (char *)malloc(len + 1);
            if (!w) exit(1);
            memcpy(w, start, len);
            w[len] = '\0';
            return w;
        }
        seen++;
    }
    return NULL;
}

json_t *w27_schema_3eeff1_ddl_skipping_settled(const char *ddl, const json_t *stored) {
    ws_slist list;
    memset(&list, 0, sizeof list);
    char *pending_drop = NULL;
    json_t *failure = NULL;

    size_t i = 0, n = ddl ? strlen(ddl) : 0;
    ws_sb stmt;
    memset(&stmt, 0, sizeof stmt);

    while (i < n && !failure) {
        size_t start = i, end;
        while (i < n && ddl[i] != '\n' && ddl[i] != '\r') i++;
        end = i;
        if (i < n) {
            if (ddl[i] == '\r' && i + 1 < n && ddl[i + 1] == '\n') i += 2;
            else i += 1;
        }
        ws_sb_addn(&stmt, ddl + start, end - start);
        ws_sb_addc(&stmt, '\n');
        if (!w27_schema_3eeff1_statement_complete(stmt.p)) continue;

        char *current = ws_strip_dup(stmt.p, stmt.n);
        stmt.n = 0;
        stmt.p[0] = '\0';
        char *upper = ws_dup_upper(current);

        if (!strncmp(upper, "DROP TRIGGER IF EXISTS ", 23)) {
            free(pending_drop);
            pending_drop = current;
            free(upper);
            continue;
        }
        if (!strncmp(upper, "CREATE TRIGGER IF NOT EXISTS ", 29)) {
            /* desired = current.replace("IF NOT EXISTS ", "", 1).rstrip(";") */
            char *desired;
            const char *hit = strstr(current, "IF NOT EXISTS ");
            if (hit) {
                ws_sb db;
                memset(&db, 0, sizeof db);
                ws_sb_addn(&db, current, (size_t)(hit - current));
                ws_sb_add(&db, hit + 14);
                desired = ws_sb_take(&db);
            } else {
                desired = ws_dup(current);
            }
            size_t dl = strlen(desired);
            while (dl && desired[dl - 1] == ';') desired[--dl] = '\0';
            char *name = ws_nth_word(desired, 2, 3);
            if (!name) {
                /* upstream raises IndexError from desired.split(None, 3)[2] */
                failure = ws_error_obj("IndexError", "list index out of range");
                free(desired);
                free(current);
                free(upper);
                break;
            }
            const json_t *have = (stored && stored->type == JSON_OBJECT) ? json_obj_get(stored, name) : NULL;
            int same = have && have->type == JSON_STRING && !strcmp(have->str_val, desired);
            free(name);
            free(desired);
            if (same) {
                free(pending_drop);
                pending_drop = NULL;
                free(current);
                free(upper);
                continue;
            }
        }
        if (pending_drop) {
            ws_slist_push(&list, pending_drop);
            pending_drop = NULL;
        }
        ws_slist_push(&list, current);
        free(upper);
    }
    if (!failure && stmt.p) {
        char *tail = ws_strip_dup(stmt.p, stmt.n);
        if (*tail) failure = ws_error_obj("OperationalError", "incomplete DDL statement");
        free(tail);
    }
    if (stmt.p) free(stmt.p);
    free(pending_drop);
    if (failure) {
        ws_slist_free(&list);
        return failure;
    }
    json_t *o = json_object();
    json_set(o, "statements", ws_slist_json(&list));
    return o;
}

/* The retired messages_fts_cjk object set an earlier release built: the three
 * triggers, the projection view, the vtable and its five FTS5 shadow tables. */
static const char *const WS_RETIRED_CJK_OBJECTS[10] = {
    "messages_fts_cjk_insert", "messages_fts_cjk_delete", "messages_fts_cjk_update",
    "messages_fts_cjk_src", "messages_fts_cjk",
    "messages_fts_cjk_content", "messages_fts_cjk_data", "messages_fts_cjk_docsize",
    "messages_fts_cjk_idx", "messages_fts_cjk_config",
};

/* ── p1: pinned constants ────────────────────────────────────────────── */
json_t *w27_schema_3eeff1_constants(void) {
    json_t *o = json_object();
    json_set(o, "fts_holder_escalate_attempts", json_number(3));
    json_set(o, "fts_holder_escalate_seconds", json_number(60.0));
    json_set(o, "fts_holder_futile_attempts", json_number(10));
    json_set(o, "fts_holder_futile_seconds", json_number(1800.0));
    json_set(o, "fts_stale_retry_seconds", json_number(60.0));
    json_set(o, "fts_stale_retry_max_seconds", json_number(3600.0));
    json_set(o, "message_uid_backfill_done", json_string("message_uid_backfill"));
    json_set(o, "message_uid_backfill_cursor", json_string("message_uid_backfill_id"));
    json_set(o, "message_uid_backfill_chunk", json_number(2000));
    json_set(o, "message_uid_backfill_budget_s", json_number(1.0));
    json_set(o, "clear_rebuild_markers_sql", json_string(
        "DELETE FROM state_meta WHERE key IN ('fts_rebuild_high_water', 'fts_rebuild_progress')"));
    json_set(o, "drop_retired_tool_high_water_sql", json_string(
        "DELETE FROM state_meta WHERE key = 'fts_tool_full_content_high_water'"));
    json_set(o, "legacy_inline_concat_sql", json_string(
        "COALESCE(content, '') || ' ' || COALESCE(tool_name, '') || ' ' || COALESCE(tool_calls, '') "));
    json_set(o, "title_unique_index_sql", json_string(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_sessions_title_unique ON sessions(title) WHERE title IS NOT NULL"));
    json_set(o, "stale_key_upsert_sql", json_string(
        "INSERT INTO state_meta (key, value) VALUES (?, '1') ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    json_set(o, "state_meta_upsert_sql", json_string(
        "INSERT INTO state_meta (key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    json_t *idx = json_array();
    json_append(idx, json_string(
        "CREATE INDEX IF NOT EXISTS idx_session_model_usage_session ON session_model_usage(session_id)"));
    json_append(idx, json_string(
        "CREATE INDEX IF NOT EXISTS idx_session_model_usage_model ON session_model_usage(model)"));
    json_set(o, "session_model_usage_index_sql", idx);
    json_t *trig = json_array();
    const char *names[6] = {
        "messages_fts_insert", "messages_fts_delete", "messages_fts_update",
        "messages_fts_trigram_insert", "messages_fts_trigram_delete", "messages_fts_trigram_update",
    };
    for (int k = 0; k < 6; k++) json_append(trig, json_string(names[k]));
    json_set(o, "fts_triggers", trig);
    json_set(o, "fts_trash_prefix", json_string("fts_v22_trash_"));
    json_set(o, "schema_version", json_number(33));
    json_set(o, "fts_storage_version", json_number(3));
    json_set(o, "fts_stale_key", json_string("fts_stale"));
    json_set(o, "fts_rebuild_deferral_key", json_string("fts_rebuild_deferral"));
    return o;
}

/* ── p2: _fts_index_is_misaligned_source ─────────────────────────────── */
int w27_schema_3eeff1_fts_index_is_misaligned_source(const json_t *table_sql) {
    if (!table_sql || table_sql->type == JSON_NULL) return 0;     /* no row */
    if (table_sql->type == JSON_STRING)
        return strstr(table_sql->str_val, "messages_fts_src") == NULL;
    /* Upstream tests `"messages_fts_src" not in (row[0] or "")`: a FALSY column
     * value (0, [], {}) becomes "" and therefore reports misaligned, which the
     * port answered as "not a string -> False". A truthy non-string raises
     * TypeError upstream, so it stays False here and shows up as live-error. */
    if ((table_sql->type == JSON_NUMBER && table_sql->num_val == 0.0) ||
        (table_sql->type == JSON_ARRAY && json_len(table_sql) == 0) ||
        (table_sql->type == JSON_OBJECT && json_object_size(table_sql) == 0))
        return 1;
    return 0;
}

/* ── p2: _live_pk_columns ────────────────────────────────────────────── */
json_t *w27_schema_3eeff1_live_pk_columns(const json_t *rows, int missing) {
    if (missing || !rows || rows->type != JSON_ARRAY) return json_null();
    size_t n = json_len(rows);
    if (!n) return json_null();
    /* [r[1] for r in sorted((r for r in rows if r[5]), key=lambda r: r[5])] */
    size_t *order = (size_t *)malloc(n * sizeof *order);
    double *keys = (double *)malloc(n * sizeof *keys);
    if (!order || !keys) exit(1);
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *row = json_get(rows, i);
        json_t *pk = (row && row->type == JSON_ARRAY) ? json_get(row, 5) : NULL;
        if (!pk || pk->type != JSON_NUMBER || pk->num_val == 0) continue;
        double k = pk->num_val;
        size_t j = m;
        while (j > 0 && keys[j - 1] > k) { keys[j] = keys[j - 1]; order[j] = order[j - 1]; j--; }
        keys[j] = k;
        order[j] = i;
        m++;
    }
    json_t *arr = json_array();
    if (!m) { free(order); free(keys); return arr; }
    for (size_t j = 0; j < m; j++) {
        json_t *row = json_get(rows, order[j]);
        json_t *name = (row && row->type == JSON_ARRAY) ? json_get(row, 1) : NULL;
        json_append(arr, json_string(name && name->type == JSON_STRING ? name->str_val : ""));
    }
    free(order);
    free(keys);
    return arr;
}

/* ── p2: _rebuild_table_statements / _rebuild_table ──────────────────── */
static void ws_push_rebuild_seq(ws_slist *list, const char *table, const char *legacy_name,
                                const char *ddl, const char *copy_sql, const json_t *indexes) {
    ws_sb b;
    memset(&b, 0, sizeof b);
    ws_sb_addf(&b, "ALTER TABLE %s RENAME TO %s", table ? table : "", legacy_name ? legacy_name : "");
    ws_slist_push(list, ws_sb_take(&b));
    ws_slist_push_copy(list, ddl ? ddl : "");
    ws_slist_push_copy(list, copy_sql ? copy_sql : "");
    memset(&b, 0, sizeof b);
    ws_sb_addf(&b, "DROP TABLE %s", legacy_name ? legacy_name : "");
    ws_slist_push(list, ws_sb_take(&b));
    if (indexes && indexes->type == JSON_ARRAY) {
        size_t n = json_len(indexes);
        for (size_t i = 0; i < n; i++) {
            json_t *v = json_get(indexes, i);
            ws_slist_push_copy(list, (v && v->type == JSON_STRING) ? v->str_val : "");
        }
    }
}

json_t *w27_schema_3eeff1_rebuild_table_statements(const char *table, const char *legacy_name,
                                                   const char *ddl, const char *copy_sql,
                                                   const json_t *indexes) {
    ws_slist list;
    memset(&list, 0, sizeof list);
    ws_push_rebuild_seq(&list, table, legacy_name, ddl, copy_sql, indexes);
    return ws_slist_json(&list);
}

/*
 * fail_at: -1 = nothing fails, otherwise the 0-based index (within the RENAME/
 * CREATE/COPY/DROP/indexes sequence) whose cursor.execute raises
 * OperationalError("boom").
 */
json_t *w27_schema_3eeff1_rebuild_table(const char *table, const char *legacy_name, const char *ddl,
                                        const char *copy_sql, const json_t *indexes,
                                        int in_transaction, int fail_at) {
    ws_slist seq;
    memset(&seq, 0, sizeof seq);
    ws_push_rebuild_seq(&seq, table, legacy_name, ddl, copy_sql, indexes);

    int failed = (fail_at >= 0 && (size_t)fail_at < seq.n);
    size_t upto = failed ? ((size_t)fail_at + 1) : seq.n;

    json_t *arr = json_array();
    if (!in_transaction) json_append(arr, json_string("BEGIN IMMEDIATE"));
    for (size_t i = 0; i < upto; i++) json_append(arr, json_string(seq.p[i]));
    if (!in_transaction) json_append(arr, json_string(failed ? "ROLLBACK" : "COMMIT"));
    ws_slist_free(&seq);

    json_t *out = json_object();
    json_set(out, "statements", arr);
    if (failed) {
        json_set(out, "error", json_string("OperationalError"));
        json_set(out, "message", json_string("boom"));
    }
    return out;
}

/* ── p2: _rebuild_fts_indexes ────────────────────────────────────────── */
json_t *w27_schema_3eeff1_rebuild_fts_indexes(int legacy, int include_trigram) {
    static const char *const CONCAT =
        "COALESCE(content, '') || ' ' || COALESCE(tool_name, '') || ' ' || COALESCE(tool_calls, '') ";
    const char *tables[2] = { "messages_fts", "messages_fts_trigram" };
    int count = include_trigram ? 2 : 1;
    ws_slist list;
    memset(&list, 0, sizeof list);
    for (int i = 0; i < count; i++) {
        const char *tbl = tables[i];
        ws_sb b;
        memset(&b, 0, sizeof b);
        if (legacy) {
            ws_sb_addf(&b, "DELETE FROM %s", tbl);
            ws_slist_push(&list, ws_sb_take(&b));
            memset(&b, 0, sizeof b);
            ws_sb_addf(&b, "INSERT INTO %s(rowid, content) SELECT id, %sFROM messages", tbl, CONCAT);
            ws_slist_push(&list, ws_sb_take(&b));
        } else {
            ws_sb_addf(&b, "INSERT INTO %s(%s) VALUES('rebuild')", tbl, tbl);
            ws_slist_push(&list, ws_sb_take(&b));
        }
    }
    if (!legacy) {
        ws_slist_push_copy(&list,
            "DELETE FROM state_meta WHERE key IN ('fts_rebuild_high_water', 'fts_rebuild_progress')");
    }
    return ws_slist_json(&list);
}

/* ── p2: _drop_fts_triggers ──────────────────────────────────────────── */
json_t *w27_schema_3eeff1_drop_fts_triggers(void) {
    static const char *const TRIGGERS[6] = {
        "messages_fts_insert", "messages_fts_delete", "messages_fts_update",
        "messages_fts_trigram_insert", "messages_fts_trigram_delete", "messages_fts_trigram_update",
    };
    ws_slist list;
    memset(&list, 0, sizeof list);
    for (int i = 0; i < 6; i++) {
        ws_sb b;
        memset(&b, 0, sizeof b);
        ws_sb_addf(&b, "DROP TRIGGER IF EXISTS %s", TRIGGERS[i]);
        ws_slist_push(&list, ws_sb_take(&b));
    }
    return ws_slist_json(&list);
}

/* ── p2: _fts_triggers_missing ───────────────────────────────────────── */
/* found = the COUNT(*) the caller's cursor returned; -1 = the query failed. */
int w27_schema_3eeff1_fts_triggers_missing(const json_t *names, int found) {
    if (!names || names->type != JSON_ARRAY) return 0;
    size_t n = json_len(names);
    if (!n) return 0;                      /* "name IN ()" is a SQLite syntax error */
    if (found < 0) return 0;
    return found < (int)n;
}

/* ── p3: capability predicates over error text ───────────────────────── */
static int ws_contains_ci(const char *hay, const char *needle) {
    if (!hay || !needle) return 0;
    size_t hn = strlen(hay), nn = strlen(needle);
    if (!nn) return 1;
    for (size_t i = 0; i + nn <= hn; i++) {
        size_t k = 0;
        while (k < nn && tolower((unsigned char)hay[i + k]) == tolower((unsigned char)needle[k])) k++;
        if (k == nn) return 1;
    }
    return 0;
}

int w27_schema_3eeff1_is_trigram_unavailable_error(const char *msg) {
    return ws_contains_ci(msg, "no such tokenizer: trigram");
}

int w27_schema_3eeff1_is_fts5_unavailable_error(const char *msg) {
    int no_module = ws_contains_ci(msg, "no such module") && ws_contains_ci(msg, "fts5");
    return no_module || w27_schema_3eeff1_is_trigram_unavailable_error(msg);
}

/* ── p3: stored-CREATE predicates ────────────────────────────────────── */
int w27_schema_3eeff1_db_has_legacy_inline_fts(const json_t *table_sql) {
    if (!table_sql || table_sql->type != JSON_STRING) return 0;   /* table absent */
    return strstr(table_sql->str_val, "tool_name") == NULL;
}

int w27_schema_3eeff1_db_has_trigram_tool_calls_projection(const json_t *table_sql) {
    if (!table_sql || table_sql->type != JSON_STRING) return 0;
    return ws_contains_ci(table_sql->str_val, "tool_calls");
}

/* ── p3: retired_cjk_index_present ───────────────────────────────────── */
/* SELECT 1 FROM sqlite_master WHERE name IN (<retired objects>) LIMIT 1 —
 * plain (case-sensitive) name membership over whatever the fixture schema holds. */
int w27_schema_3eeff1_retired_cjk_index_present(const json_t *names) {
    if (!names || names->type != JSON_ARRAY) return 0;
    size_t n = json_len(names);
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(names, i);
        if (!v || v->type != JSON_STRING) continue;
        for (int k = 0; k < 10; k++) {
            if (!strcmp(v->str_val, WS_RETIRED_CJK_OBJECTS[k])) return 1;
        }
    }
    return 0;
}

/* ── p3: _title_rank ─────────────────────────────────────────────────── */
int w27_schema_3eeff1_title_rank(const char *source) {
    if (!source) return 2;                 /* NULL ranks as user */
    if (!strcmp(source, "derived")) return 0;
    if (!strcmp(source, "llm")) return 1;
    if (!strcmp(source, "user")) return 2;
    return 0;
}

/* ── p3: _has_fts_trash ──────────────────────────────────────────────── */
/* LIKE 'fts\_v22\_trash\_%' ESCAPE '\' — ASCII-case-insensitive prefix match. */
int w27_schema_3eeff1_has_fts_trash(const json_t *tables) {
    static const char *const PREFIX = "fts_v22_trash_";
    size_t pl = strlen(PREFIX);
    if (!tables || tables->type != JSON_ARRAY) return 0;
    size_t n = json_len(tables);
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(tables, i);
        if (!v || v->type != JSON_STRING) continue;
        const char *name = v->str_val;
        if (strlen(name) < pl) continue;
        size_t k = 0;
        while (k < pl && tolower((unsigned char)name[k]) == tolower((unsigned char)PREFIX[k])) k++;
        if (k == pl) return 1;
    }
    return 0;
}

/* ── p3: next_title_in_lineage ───────────────────────────────────────── */
/*
 * _NUMBERED_TITLE_RE = r'^(.*?) #(\d+)$' — the lazy prefix takes the FIRST
 * " #<digits>" that runs to end of string.  Returns the stripped base (malloc'd)
 * and stores the number in *number when the suffix matched.
 */
static char *ws_strip_number_suffix(const char *t, long *number) {
    size_t n = strlen(t);
    for (size_t i = 0; i + 1 < n; i++) {
        if (t[i] != ' ' || t[i + 1] != '#') continue;
        size_t j = i + 2;
        if (j >= n) continue;
        long v = 0;
        size_t k = j;
        while (k < n && t[k] >= '0' && t[k] <= '9') { v = v * 10 + (t[k] - '0'); k++; }
        if (k == n && k > j) {
            char *base = (char *)malloc(i + 1);
            if (!base) exit(1);
            memcpy(base, t, i);
            base[i] = '\0';
            if (number) *number = v;
            return base;
        }
    }
    return NULL;
}

/* title LIKE <escaped base> ' #%' ESCAPE '\' : base prefix (ASCII-ci) then " #". */
static int ws_like_lineage_match(const char *title, const char *base) {
    size_t bl = strlen(base), tl = strlen(title);
    if (tl < bl + 2) return 0;
    for (size_t k = 0; k < bl; k++) {
        if (tolower((unsigned char)title[k]) != tolower((unsigned char)base[k])) return 0;
    }
    return title[bl] == ' ' && title[bl + 1] == '#';
}

char *w27_schema_3eeff1_next_title_in_lineage(const char *base_title, const json_t *titles) {
    const char *raw = base_title ? base_title : "";
    long ignored = 0;
    char *stripped = ws_strip_number_suffix(raw, &ignored);
    char *base = stripped ? stripped : ws_dup(raw);

    int any = 0;
    long best = 1;   /* the unnumbered original counts as #1 */
    if (titles && titles->type == JSON_ARRAY) {
        size_t n = json_len(titles);
        for (size_t i = 0; i < n; i++) {
            json_t *v = json_get(titles, i);
            if (!v || v->type != JSON_STRING) continue;
            const char *title = v->str_val;
            int matched = strcmp(title, base) == 0 || ws_like_lineage_match(title, base);
            if (!matched) continue;
            any = 1;
            long num = 0;
            char *inner = ws_strip_number_suffix(title, &num);
            if (inner) {
                free(inner);
                if (num > best) best = num;
            }
        }
    }
    if (!any) return base;

    ws_sb b;
    memset(&b, 0, sizeof b);
    ws_sb_addf(&b, "%s #%ld", base, best + 1);
    free(base);
    return ws_sb_take(&b);
}
