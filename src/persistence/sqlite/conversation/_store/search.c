/*
 * search.c — C11 port of upstream Hermes
 * persistence/sqlite/conversation/_store/search.py.
 *
 * Gates (all against this one port file):
 *   w30_search_d40ff0_p1 — query normalization: FTS5 sanitisation, token
 *                          quoting/normalisation, CJK classification, LIKE
 *                          escaping, multimodal text flattening, result-field
 *                          projection, backfill percentages.
 *   w30_search_d40ff0_p2 — statement composition and routing: the pinned SQL
 *                          fragments/templates, LIKE-boolean compilation, the
 *                          FTS/LIKE/trigram route predicates, the session-id
 *                          result ranking, and the diagnostic path name.
 *
 * Seams (deliberately NOT ported, answered by the caller):
 *   - the sqlite connection and its read pool: every executed statement's
 *     answer arrives in the request, or (for the route builders) the exact
 *     SQL text and parameter list the port produces is what gets compared;
 *   - `routed_sessions_setting` / `_search_slow_ms` (env + settings file);
 *   - `fts_rebuild_status` / `_backfill_progress` (state_meta reads): the port
 *     receives the high-water/progress pair and reproduces the derived numbers
 *     and the statement text;
 *   - `_read_retrying_ioerr`, `_in_one_read`, `note_storage_error`,
 *     `_enter_fts_fail_open`, `_mark_fts_detached` (storage health / retry);
 *   - writes: `rebuild_fts` and the boundary/chunk sweep INSERTs are pinned as
 *     exact statement text only (see `..._p2_sql_constants`).
 *
 * PoP tags name the upstream symbol each function ports.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w30_search_d40ff0_p1.h"
#include "noc_w30_search_d40ff0_p2.h"

/* ── byte-string builder ────────────────────────────────────────────────
 * Explicit length: the sanitiser hides protected phrases behind a NUL
 * sentinel, so a buffer can hold a 0x00 byte mid-string. */
typedef struct {
    char *p;
    size_t n;
    size_t cap;
} sr_sb;

static void sr_sb_init(sr_sb *b) { b->p = NULL; b->n = 0; b->cap = 0; }
static void sr_sb_free(sr_sb *b) { free(b->p); b->p = NULL; b->n = b->cap = 0; }

static int sr_sb_need(sr_sb *b, size_t extra) {
    size_t want, cap;
    char *np;
    if (b->n + extra + 1 <= b->cap) return 1;
    want = b->n + extra + 1;
    cap = b->cap ? b->cap : 64;
    while (cap < want) cap *= 2;
    np = (char *)realloc(b->p, cap);
    if (!np) return 0;
    b->p = np;
    b->cap = cap;
    return 1;
}
static void sr_sb_put(sr_sb *b, const char *s, size_t n) {
    if (!n || !s) return;
    if (!sr_sb_need(b, n)) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
}
static void sr_sb_puts(sr_sb *b, const char *s) { sr_sb_put(b, s, strlen(s)); }
static void sr_sb_putc(sr_sb *b, char c) { sr_sb_put(b, &c, 1); }
static void sr_sb_printf(sr_sb *b, const char *fmt, ...) {
    va_list ap;
    char tmp[512];
    int k;
    va_start(ap, fmt);
    k = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (k < 0) return;
    if ((size_t)k < sizeof tmp) {
        sr_sb_put(b, tmp, (size_t)k);
        return;
    }
    if (sr_sb_need(b, (size_t)k)) {
        va_start(ap, fmt);
        vsnprintf(b->p + b->n, (size_t)k + 1, fmt, ap);
        va_end(ap);
        b->n += (size_t)k;
    }
}
/* NUL-terminated view; keeps b->n as the true length. */
static char *sr_sb_cstr(sr_sb *b) {
    if (!sr_sb_need(b, 0)) return NULL;
    b->p[b->n] = '\0';
    return b->p;
}

/* ── UTF-8 / unicode helpers ─────────────────────────────────────────── */
static unsigned sr_cp(const char *s, size_t n, size_t i, size_t *len) {
    const unsigned char *u = (const unsigned char *)s;
    unsigned c;
    if (i >= n) { *len = 1; return 0; }
    c = u[i];
    if (c < 0x80) { *len = 1; return c; }
    if ((c & 0xE0) == 0xC0 && i + 1 < n && (u[i + 1] & 0xC0) == 0x80) {
        *len = 2;
        return ((c & 0x1Fu) << 6) | (u[i + 1] & 0x3Fu);
    }
    if ((c & 0xF0) == 0xE0 && i + 2 < n && (u[i + 1] & 0xC0) == 0x80 && (u[i + 2] & 0xC0) == 0x80) {
        *len = 3;
        return ((c & 0x0Fu) << 12) | ((u[i + 1] & 0x3Fu) << 6) | (u[i + 2] & 0x3Fu);
    }
    if ((c & 0xF8) == 0xF0 && i + 3 < n && (u[i + 1] & 0xC0) == 0x80 &&
        (u[i + 2] & 0xC0) == 0x80 && (u[i + 3] & 0xC0) == 0x80) {
        *len = 4;
        return ((c & 0x07u) << 18) | ((u[i + 1] & 0x3Fu) << 12) |
               ((u[i + 2] & 0x3Fu) << 6) | (u[i + 3] & 0x3Fu);
    }
    *len = 1;
    return c;
}

static int sr_last_cp(const char *s, size_t n, size_t *off, unsigned *cp) {
    size_t i, l;
    if (!n) return 0;
    i = n - 1;
    while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--;
    *cp = sr_cp(s, n, i, &l);
    *off = i;
    return 1;
}

/* Python str.isspace() over the code points fixtures can carry. */
static int sr_is_space(unsigned cp) {
    if (cp >= 0x09 && cp <= 0x0D) return 1;
    if (cp == 0x20 || cp == 0x85 || cp == 0xA0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000) return 1;
    return 0;
}

/* Python regex \w: alnum or underscore; every non-space non-ASCII code point
 * is treated as a word character (the ranges fixtures use are letters). */
static int sr_is_word(unsigned cp) {
    if (cp < 0x80) return isalnum((int)cp) || cp == '_';
    return !sr_is_space(cp);
}

static const unsigned SR_CJK_RANGES[7][2] = {
    {0x4E00, 0x9FFF}, {0x3400, 0x4DBF}, {0x20000, 0x2A6DF}, {0x3000, 0x303F},
    {0x3040, 0x309F}, {0x30A0, 0x30FF}, {0xAC00, 0xD7AF},
};

/* PoP: sr_is_cjk @ persistence/sqlite/conversation/_store/search.py:_is_cjk */
static int sr_is_cjk(unsigned cp) {
    size_t i;
    for (i = 0; i < 7; i++)
        if (cp >= SR_CJK_RANGES[i][0] && cp <= SR_CJK_RANGES[i][1]) return 1;
    return 0;
}

/* PoP: sr_contains_cjk @ .../search.py:_contains_cjk */
static int sr_contains_cjk(const char *s, size_t n) {
    size_t i = 0, l;
    while (i < n) {
        unsigned cp = sr_cp(s, n, i, &l);
        if (sr_is_cjk(cp)) return 1;
        i += l;
    }
    return 0;
}
/* PoP: sr_count_cjk @ .../search.py:_count_cjk */
static long long sr_count_cjk(const char *s, size_t n) {
    size_t i = 0, l;
    long long k = 0;
    while (i < n) {
        unsigned cp = sr_cp(s, n, i, &l);
        if (sr_is_cjk(cp)) k++;
        i += l;
    }
    return k;
}
/* Python len(str) — code points, not bytes. */
static size_t sr_cp_len(const char *s, size_t n) {
    size_t i = 0, l, k = 0;
    while (i < n) { sr_cp(s, n, i, &l); i += l; k++; }
    return k;
}

static void sr_lstrip(sr_sb *b) {
    size_t i = 0, l;
    unsigned cp;
    while (i < b->n) {
        cp = sr_cp(b->p, b->n, i, &l);
        if (!sr_is_space(cp)) break;
        i += l;
    }
    if (i) {
        memmove(b->p, b->p + i, b->n - i);
        b->n -= i;
    }
}
static void sr_rstrip(sr_sb *b) {
    size_t off;
    unsigned cp;
    while (b->n && sr_last_cp(b->p, b->n, &off, &cp) && sr_is_space(cp)) b->n = off;
}
static void sr_strip(sr_sb *b) { sr_lstrip(b); sr_rstrip(b); }

/* Python str.strip('"') — every leading/trailing quote removed. */
static void sr_strip_quotes(sr_sb *b) {
    size_t i = 0;
    while (i < b->n && b->p[i] == '"') i++;
    if (i) { memmove(b->p, b->p + i, b->n - i); b->n -= i; }
    while (b->n && b->p[b->n - 1] == '"') b->n--;
}

/* Whitespace token iteration (Python str.split()). */
static int sr_next_token(const char *s, size_t n, size_t *i, size_t *start, size_t *len) {
    size_t l;
    unsigned cp;
    while (*i < n) {
        cp = sr_cp(s, n, *i, &l);
        if (!sr_is_space(cp)) break;
        *i += l;
    }
    if (*i >= n) return 0;
    *start = *i;
    while (*i < n) {
        cp = sr_cp(s, n, *i, &l);
        if (sr_is_space(cp)) break;
        *i += l;
    }
    *len = *i - *start;
    return 1;
}

/* `_LIKE_TOKEN_RE` = r'"[^"]+"|\S+' — one token at a time from *i. */
static int sr_like_token(const char *s, size_t n, size_t *i, size_t *start, size_t *len) {
    size_t l, j;
    unsigned cp;
    while (*i < n) {
        cp = sr_cp(s, n, *i, &l);
        if (!sr_is_space(cp)) break;
        *i += l;
    }
    if (*i >= n) return 0;
    if (s[*i] == '"') {
        j = *i + 1;
        while (j < n && s[j] != '"') j++;
        if (j > *i + 1 && j < n) { /* balanced phrase, at least one char inside */
            *start = *i;
            *len = j - *i + 1;
            *i = j + 1;
            return 1;
        }
    }
    *start = *i;
    while (*i < n) {
        cp = sr_cp(s, n, *i, &l);
        if (sr_is_space(cp)) break;
        *i += l;
    }
    *len = *i - *start;
    return 1;
}

/* ASCII-case-insensitive compare of a token span to an FTS operator
 * ("AND", "OR", "NOT" — note OR is two letters). */
static int sr_is_op(const char *s, size_t n, const char *op) {
    size_t i, l = strlen(op);
    if (n != l) return 0;
    for (i = 0; i < l; i++)
        if (toupper((unsigned char)s[i]) != (unsigned char)op[i]) return 0;
    return 1;
}
static int sr_is_operator(const char *s, size_t n) {
    return sr_is_op(s, n, "AND") || sr_is_op(s, n, "OR") || sr_is_op(s, n, "NOT");
}

/* ── JSON accessors ──────────────────────────────────────────────────── */
static const char *sr_str(const json_t *o, const char *k) {
    const json_t *v = o ? json_obj_get(o, k) : NULL;
    return v && v->type == JSON_STRING && v->str_val ? v->str_val : "";
}
static int sr_bool(const json_t *o, const char *k, int def) {
    const json_t *v = o ? json_obj_get(o, k) : NULL;
    if (!v) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return def;
}
static const json_t *sr_get(const json_t *o, const char *k) { return o ? json_obj_get(o, k) : NULL; }
static const char *sr_arr_str(const json_t *a, size_t i) {
    const json_t *v = (a && a->type == JSON_ARRAY) ? json_get(a, i) : NULL;
    return v && v->type == JSON_STRING && v->str_val ? v->str_val : NULL;
}
static int sr_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static json_t *sr_json_str(const char *s, size_t n) {
    json_t *v;
    sr_sb t;
    sr_sb_init(&t);
    sr_sb_put(&t, s, n);
    sr_sb_cstr(&t);
    v = json_string(t.p ? t.p : "");
    sr_sb_free(&t);
    return v;
}
static json_t *sr_sb_json(sr_sb *b) {
    sr_sb_cstr(b);
    return json_string(b->p ? b->p : "");
}

/* ── shared pinned SQL ───────────────────────────────────────────────── */
static const char SR_SELECT_TAIL[] =
    "m.timestamp, m.tool_name, s.source, s.model, s.started_at AS session_started";
static const char SR_LIKE_SNIPPET_SQL[] =
    "substr(m.content, max(1, instr(m.content, ?) - 40), 120) AS snippet";
static const char SR_LIKE_ANY_COLUMN_SQL[] =
    "(m.content LIKE ? ESCAPE '\\' OR m.tool_name LIKE ? ESCAPE '\\' "
    "OR m.tool_calls LIKE ? ESCAPE '\\')";
static const char SR_LIKE_COALESCED_COLUMN_SQL[] =
    "(COALESCE(m.content, '') LIKE ? ESCAPE '\\' OR COALESCE(m.tool_name, '') LIKE ? ESCAPE '\\' "
    "OR COALESCE(m.tool_calls, '') LIKE ? ESCAPE '\\')";
static const char SR_FTS_RANK_ORDER_BY[] = "ORDER BY rank";
static const char SR_NEWEST_ORDER_BY[] = "ORDER BY m.timestamp DESC, rank";
static const char SR_OLDEST_ORDER_BY[] = "ORDER BY m.timestamp ASC, rank";
static const char SR_CONTEXT_WINDOW_SQL[] =
    "WITH target AS (\n"
    "    SELECT session_id, timestamp, id FROM messages WHERE id IN ({ids})\n"
    ")\n"
    "SELECT t.id AS match_id, m.role, m.content\n"
    "FROM target t JOIN messages m ON m.id IN (\n"
    "    t.id,\n"
    "    (SELECT p.id FROM messages p\n"
    "     WHERE p.session_id = t.session_id AND (p.timestamp, p.id) < (t.timestamp, t.id)\n"
    "     ORDER BY p.timestamp DESC, p.id DESC LIMIT 1),\n"
    "    (SELECT n.id FROM messages n\n"
    "     WHERE n.session_id = t.session_id AND (n.timestamp, n.id) > (t.timestamp, t.id)\n"
    "     ORDER BY n.timestamp, n.id LIMIT 1)\n"
    ")\n"
    "ORDER BY t.id, m.timestamp, m.id";
static const char SR_BASE_BOUNDARY_SWEEP_SQL[] =
    "INSERT INTO messages_fts(rowid, content, tool_name, tool_calls) "
    "SELECT m.id, CASE WHEN m.role = 'tool' THEN substr(COALESCE(m.content, ''), 1, ?) "
    "ELSE m.content END, m.tool_name, m.tool_calls FROM messages m WHERE m.id > ? AND m.id <= ? "
    "AND NOT EXISTS (SELECT 1 FROM messages_fts_docsize d WHERE d.id = m.id)";
static const char SR_CHUNK_INSERT_SQL[] =
    "INSERT INTO messages_fts(rowid, content, tool_name, tool_calls) "
    "SELECT id, CASE WHEN role = 'tool' "
    "THEN substr(COALESCE(content, ''), 1, 8192) "
    "ELSE content END, tool_name, tool_calls FROM messages WHERE id > ? AND id <= ?";

static const char SR_RESULT_FIELDS[10][16] = {
    "id", "session_id", "role", "snippet", "timestamp",
    "tool_name", "source", "model", "session_started", "context",
};
#define SR_RESULT_FIELD_COUNT 10

/* PoP: sr_trigram_session_sql @ persistence/sqlite/_schema_ddl.py:fts_trigram_session_sql */
static void sr_trigram_session_sql(sr_sb *b, const char *alias) {
    char q[64];
    q[0] = '\0';
    if (alias && alias[0]) {
        snprintf(q, sizeof q, "%s.", alias);
    }
    sr_sb_printf(b,
                 "%ssource NOT IN ('cron', 'subagent') AND json_extract((CASE WHEN json_valid(%smodel_config) "
                 "THEN %smodel_config ELSE json_object() END), '$._delegate_from') IS NULL",
                 q, q, q);
}

/* ── shared predicates ───────────────────────────────────────────────── */
/* PoP: sr_non_op_tokens @ .../search.py:_non_operator_tokens */
static json_t *sr_non_op_tokens(const char *s, size_t n) {
    json_t *a = json_array();
    size_t i = 0, st = 0, ln = 0;
    while (sr_next_token(s, n, &i, &st, &ln))
        if (!sr_is_operator(s + st, ln)) json_append(a, sr_json_str(s + st, ln));
    return a;
}

/* PoP: sr_strip_cjk_wildcards @ .../search.py:_strip_cjk_wildcards */
static void sr_strip_cjk_wildcards(sr_sb *out, const char *s, size_t n) {
    size_t i = 0, st = 0, ln = 0;
    int first = 1;
    if (!n || !memchr(s, '*', n)) { sr_sb_put(out, s, n); return; }
    while (sr_next_token(s, n, &i, &st, &ln)) {
        const char *t = s + st;
        size_t tl = ln;
        if (!first)
            sr_sb_putc(out, ' ');
        else
            first = 0;
        if (sr_is_operator(t, tl)) {
            sr_sb_put(out, t, tl);
        } else {
            while (tl && t[tl - 1] == '*') tl--;
            if (tl)
                sr_sb_put(out, t, tl);
            else
                sr_sb_put(out, t, ln); /* all stars: keep the original form */
        }
    }
    if (out->n == 0) sr_sb_put(out, s, n); /* "or raw_query" */
}

/* PoP: sr_has_short_cjk_token @ .../search.py:_has_short_cjk_token */
static int sr_has_short_cjk(const char *s, size_t n) {
    size_t i = 0, st = 0, ln = 0;
    while (sr_next_token(s, n, &i, &st, &ln)) {
        const char *t = s + st;
        if (sr_is_operator(t, ln)) continue;
        if (sr_contains_cjk(t, ln) && sr_count_cjk(t, ln) < 3) return 1;
    }
    return 0;
}

/* PoP: sr_trigram_route_ok @ .../search.py:_trigram_route_ok (holder flag = seam) */
static int sr_trigram_ok(const char *s, size_t n, int trigram_available) {
    if (sr_count_cjk(s, n) < 3) return 0;
    if (sr_has_short_cjk(s, n)) return 0;
    return trigram_available ? 1 : 0;
}

/* PoP: sr_trigram_eligible @ .../search.py:_trigram_eligible_tokens */
static int sr_trigram_eligible(const char *s, size_t n) {
    sr_sb t;
    json_t *toks;
    size_t i, k;
    int ok = 1;
    sr_sb_init(&t);
    sr_sb_put(&t, s, n);
    sr_sb_cstr(&t);
    sr_strip_quotes(&t);
    sr_strip(&t);
    toks = sr_non_op_tokens(t.p ? t.p : "", t.n);
    k = json_len(toks);
    if (k == 0) ok = 0;
    for (i = 0; ok && i < k; i++) {
        const char *tv = sr_arr_str(toks, i);
        if (!tv || sr_cp_len(tv, strlen(tv)) < 3) ok = 0;
    }
    json_free(toks);
    sr_sb_free(&t);
    return ok;
}

/* PoP: sr_escape_like @ persistence/sqlite/conversation/_store/sql_common.py:escape_like */
static void sr_escape_like(sr_sb *b, const char *s, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (s[i] == '\\' || s[i] == '%' || s[i] == '_') sr_sb_putc(b, '\\');
        sr_sb_putc(b, s[i]);
    }
}

/* PoP: sr_or_relaxed @ .../search.py:_or_relaxed_query
 * NULL => relaxation does not apply. */
static char *sr_or_relaxed(const char *s, size_t n) {
    sr_sb out;
    size_t i = 0, st = 0, ln = 0, k = 0;
    sr_sb_init(&out);
    while (sr_like_token(s, n, &i, &st, &ln)) {
        if (sr_is_op(s + st, ln, "OR") || sr_is_op(s + st, ln, "NOT")) {
            sr_sb_free(&out);
            return NULL;
        }
        if (!sr_is_op(s + st, ln, "AND")) {
            if (k++) sr_sb_puts(&out, " OR ");
            sr_sb_put(&out, s + st, ln);
        }
    }
    if (k < 2) { sr_sb_free(&out); return NULL; }
    return sr_sb_cstr(&out);
}

/* Ops the LIKE term splitter skips (``_LIKE_SKIP_TOKENS``). */
static int sr_is_like_skip(const char *s, size_t n) {
    return sr_is_operator(s, n) || sr_is_op(s, n, "NEAR");
}

/* Trim quotes, then stars, then whitespace — ``strip('"').strip("*").strip()``. */
static void sr_trim_term(const char *t, size_t n, size_t *a_out, size_t *e_out) {
    size_t a = 0, e = n;
    while (a < e && t[a] == '"') a++;
    while (e > a && t[e - 1] == '"') e--;
    while (a < e && t[a] == '*') a++;
    while (e > a && t[e - 1] == '*') e--;
    while (a < e) {
        size_t l;
        unsigned cp = sr_cp(t, e, a, &l);
        if (!sr_is_space(cp)) break;
        a += l;
    }
    while (e > a) {
        size_t off;
        unsigned cp;
        if (!sr_last_cp(t + a, e - a, &off, &cp) || !sr_is_space(cp)) break;
        e = a + off;
    }
    *a_out = a;
    *e_out = e;
}

/* End of the maximal \w run starting at i. */
static size_t sr_run_end(const char *s, size_t n, size_t i) {
    size_t j = i, l;
    while (j < n) {
        unsigned cp = sr_cp(s, n, j, &l);
        if (!sr_is_word(cp)) break;
        j += l;
    }
    return j;
}

/* ``\b(\w+(?:[._-]\w+)+)\b`` matched greedily (with the backtracking the
 * regex engine performs) at a word run starting at i: the end offset of the
 * match, or i when the run carries no usable separator. */
static size_t sr_dotted_end(const char *s, size_t n, size_t i) {
    size_t j = sr_run_end(s, n, i);
    int used = 0;
    for (;;) {
        size_t l;
        unsigned cp;
        if (j >= n || (s[j] != '.' && s[j] != '-') || j + 1 >= n) break;
        cp = sr_cp(s, n, j + 1, &l);
        if (!sr_is_word(cp)) break;
        j = sr_run_end(s, n, j + 1);
        used = 1;
    }
    if (used) return j;
    {
        /* no '.'/'-' separator: an underscore inside the run is the only one,
         * and greedy backtracking picks the last (the match still ends here) */
        size_t k, last = 0;
        int found = 0;
        for (k = i + 1; k + 1 < j; k++)
            if (s[k] == '_') { last = k; found = 1; }
        (void)last;
        return found ? j : 0;
    }
}

/* ── the FTS5 sanitiser ──────────────────────────────────────────────── */
/* PoP: sr_sanitize_fts5 @ .../search.py:_sanitize_fts5_query */
static char *sr_sanitize_fts5(const char *q, size_t n) {
    typedef struct { size_t off, len; } span;
    span *parts = NULL;
    size_t nparts = 0, cap = 0, i, p, e;
    sr_sb s, t, u, w, r;
    int last_ws;
    char *out;

    /* 0. cap at MAX_FTS5_QUERY_CHARS code points. */
    e = n;
    {
        size_t l, cps = 0, k = 0;
        while (k < n) {
            sr_cp(q, n, k, &l);
            k += l;
            if (++cps == 2048) { e = k; break; }
        }
    }

    sr_sb_init(&s);
    /* 1. protect balanced quoted phrases behind a NUL sentinel. */
    p = 0;
    while (p < e) {
        if (q[p] == '"') {
            size_t close = p + 1;
            while (close < e && q[close] != '"') close++;
            if (close < e) {
                if (nparts == cap) {
                    size_t ncap = cap ? cap * 2 : 8;
                    span *np = (span *)realloc(parts, ncap * sizeof(span));
                    if (!np) break;
                    parts = np;
                    cap = ncap;
                }
                parts[nparts].off = p;
                parts[nparts].len = close - p + 1;
                nparts++;
                sr_sb_putc(&s, '\0');
                p = close + 1;
                continue;
            }
            sr_sb_putc(&s, ' '); /* unmatched quote -> space */
            p++;
            continue;
        }
        sr_sb_putc(&s, q[p]);
        p++;
    }

    /* 2. strip FTS5-special characters. */
    sr_sb_init(&t);
    for (i = 0; i < s.n; i++) {
        unsigned char c = (unsigned char)s.p[i];
        if (c != 0 && c < 0x80 && strchr("+{}():\"^@/#&|~[]<>,;!?$=\\'", (int)c))
            sr_sb_putc(&t, ' ');
        else
            sr_sb_putc(&t, (char)c);
    }
    /* '%' survives only for the CJK LIKE fallback. */
    if (t.n && memchr(t.p, '%', t.n) && !sr_contains_cjk(t.p, t.n)) {
        for (i = 0; i < t.n; i++)
            if (t.p[i] == '%') t.p[i] = ' ';
    }

    /* 3. collapse '*' runs and drop a star at start / after whitespace. */
    sr_sb_init(&u);
    last_ws = 1;
    {
        int last_star = 0;
        for (i = 0; i < t.n; i++) {
            size_t l;
            unsigned cp;
            if (t.p[i] == '*') {
                /* (^\s)\* -> star removed, whitespace kept; \*+ -> one star */
                if (last_ws || last_star) continue;
                sr_sb_putc(&u, '*');
                last_star = 1;
                last_ws = 0;
                continue;
            }
            cp = sr_cp(t.p, t.n, i, &l);
            last_ws = sr_is_space(cp) ? 1 : 0;
            last_star = 0;
            sr_sb_put(&u, t.p + i, l);
            i += l - 1;
        }
    }

    /* 4a. drop a dangling leading boolean operator. */
    sr_strip(&u);
    {
        size_t oplen = 0;
        if (u.n >= 2 && sr_is_op(u.p, 2, "OR")) oplen = 2;
        else if (u.n >= 3 && (sr_is_op(u.p, 3, "AND") || sr_is_op(u.p, 3, "NOT"))) oplen = 3;
        if (oplen) {
            size_t l;
            unsigned cp = u.n > oplen ? sr_cp(u.p, u.n, oplen, &l) : 0;
            if (u.n == oplen || !sr_is_word(cp)) {
                memmove(u.p, u.p + oplen, u.n - oplen);
                u.n -= oplen;
            }
        }
    }
    /* 4b. drop a dangling trailing boolean operator (``\s+(AND|OR|NOT)\s*$``). */
    sr_strip(&u);
    {
        size_t oplen = 0;
        if (u.n > 2 && sr_is_op(u.p + u.n - 2, 2, "OR")) oplen = 2;
        else if (u.n > 3 && (sr_is_op(u.p + u.n - 3, 3, "AND") || sr_is_op(u.p + u.n - 3, 3, "NOT")))
            oplen = 3;
        if (oplen) {
            size_t cut = u.n - oplen;
            int had_ws = 0;
            for (;;) {
                size_t off;
                unsigned cp;
                if (cut == 0) break;
                if (!sr_last_cp(u.p, cut, &off, &cp) || !sr_is_space(cp)) break;
                had_ws = 1;
                cut = off;
            }
            if (had_ws) u.n = cut;
        }
    }

    /* 5. quote dotted/hyphenated/underscored terms in one pass. */
    sr_sb_init(&w);
    i = 0;
    {
        int prev_word = 0;
        while (i < u.n) {
            size_t l;
            unsigned cp = sr_cp(u.p, u.n, i, &l);
            if (sr_is_word(cp) && !prev_word) {
                size_t e = sr_dotted_end(u.p, u.n, i);
                if (e > i) {
                    sr_sb_putc(&w, '"');
                    sr_sb_put(&w, u.p + i, e - i);
                    sr_sb_putc(&w, '"');
                    prev_word = 1;
                    i = e;
                    continue;
                }
                /* no [._-] group: copy the whole word run verbatim */
                {
                    size_t j = i + l;
                    while (j < u.n) {
                        size_t l2;
                        unsigned c2 = sr_cp(u.p, u.n, j, &l2);
                        if (!sr_is_word(c2)) break;
                        j += l2;
                    }
                    sr_sb_put(&w, u.p + i, j - i);
                    prev_word = 1;
                    i = j;
                    continue;
                }
            }
            sr_sb_put(&w, u.p + i, l);
            prev_word = sr_is_word(cp);
            i += l;
        }
    }

    /* 6. restore the protected phrases, in order of appearance. */
    sr_sb_init(&r);
    {
        size_t k = 0;
        for (i = 0; i < w.n; i++) {
            if (w.p[i] == '\0' && k < nparts) {
                sr_sb_put(&r, q + parts[k].off, parts[k].len);
                k++;
            } else {
                sr_sb_putc(&r, w.p[i]);
            }
        }
    }
    sr_strip(&r);
    out = sr_sb_cstr(&r);
    sr_sb_free(&w);
    sr_sb_free(&u);
    sr_sb_free(&t);
    sr_sb_free(&s);
    free(parts);
    return out;
}

/* ══════════════════════════════════════════════════════════════════════ */
/* Gate w30_search_d40ff0_p1 — query normalization.                       */
/* ══════════════════════════════════════════════════════════════════════ */

/* PoP: w30_search_d40ff0_p1_is_cjk @ .../search.py:_is_cjk */
json_t *w30_search_d40ff0_p1_is_cjk(const json_t *req) {
    const json_t *v = sr_get(req, "codepoint");
    double d = v && v->type == JSON_NUMBER ? v->num_val : 0.0;
    return json_bool(sr_is_cjk((unsigned)(long long)d) ? true : false);
}

/* PoP: w30_search_d40ff0_p1_contains_cjk @ .../search.py:_contains_cjk */
json_t *w30_search_d40ff0_p1_contains_cjk(const json_t *req) {
    const char *s = sr_str(req, "text");
    return json_bool(sr_contains_cjk(s, strlen(s)) ? true : false);
}

/* PoP: w30_search_d40ff0_p1_count_cjk @ .../search.py:_count_cjk */
json_t *w30_search_d40ff0_p1_count_cjk(const json_t *req) {
    const char *s = sr_str(req, "text");
    return json_number((double)sr_count_cjk(s, strlen(s)));
}

/* PoP: w30_search_d40ff0_p1_escape_like @ .../sql_common.py:escape_like */
json_t *w30_search_d40ff0_p1_escape_like(const json_t *req) {
    const char *s = sr_str(req, "term");
    sr_sb b;
    json_t *v;
    sr_sb_init(&b);
    sr_escape_like(&b, s, strlen(s));
    v = sr_sb_json(&b);
    sr_sb_free(&b);
    return v;
}

/* PoP: w30_search_d40ff0_p1_like_params @ .../search.py:_like_params */
json_t *w30_search_d40ff0_p1_like_params(const json_t *req) {
    const char *s = sr_str(req, "term");
    size_t n = strlen(s);
    sr_sb b;
    json_t *a = json_array();
    int k;
    for (k = 0; k < 3; k++) {
        sr_sb_init(&b);
        sr_sb_putc(&b, '%');
        sr_escape_like(&b, s, n);
        sr_sb_putc(&b, '%');
        json_append(a, sr_sb_json(&b));
        sr_sb_free(&b);
    }
    return a;
}

/* PoP: w30_search_d40ff0_p1_non_op_tokens @ .../search.py:_non_operator_tokens */
json_t *w30_search_d40ff0_p1_non_op_tokens(const json_t *req) {
    const char *s = sr_str(req, "raw");
    return sr_non_op_tokens(s, strlen(s));
}

/* PoP: w30_search_d40ff0_p1_quote_tokens @ .../search.py:_quote_fts_tokens */
json_t *w30_search_d40ff0_p1_quote_tokens(const json_t *req) {
    const char *s = sr_str(req, "raw");
    size_t n = strlen(s), i = 0, st = 0, ln = 0;
    int first = 1;
    sr_sb b;
    json_t *v;
    sr_sb_init(&b);
    while (sr_next_token(s, n, &i, &st, &ln)) {
        size_t k;
        if (!first) sr_sb_putc(&b, ' ');
        first = 0;
        if (sr_is_operator(s + st, ln)) {
            sr_sb_put(&b, s + st, ln);
            continue;
        }
        sr_sb_putc(&b, '"');
        for (k = 0; k < ln; k++) {
            if (s[st + k] == '"') sr_sb_putc(&b, '"');
            sr_sb_putc(&b, s[st + k]);
        }
        sr_sb_putc(&b, '"');
    }
    v = sr_sb_json(&b);
    sr_sb_free(&b);
    return v;
}

/* PoP: w30_search_d40ff0_p1_strip_wildcards @ .../search.py:_strip_cjk_wildcards */
json_t *w30_search_d40ff0_p1_strip_wildcards(const json_t *req) {
    const char *s = sr_str(req, "raw");
    sr_sb b;
    json_t *v;
    sr_sb_init(&b);
    sr_strip_cjk_wildcards(&b, s, strlen(s));
    v = sr_sb_json(&b);
    sr_sb_free(&b);
    return v;
}

/* PoP: w30_search_d40ff0_p1_sanitize @ .../search.py:_sanitize_fts5_query */
json_t *w30_search_d40ff0_p1_sanitize(const json_t *req) {
    const char *s = sr_str(req, "query");
    char *out = sr_sanitize_fts5(s, strlen(s));
    json_t *v = json_string(out ? out : "");
    free(out);
    return v;
}

/* PoP: w30_search_d40ff0_p1_or_relaxed @ .../search.py:_or_relaxed_query */
json_t *w30_search_d40ff0_p1_or_relaxed(const json_t *req) {
    const char *s = sr_str(req, "query");
    char *out = sr_or_relaxed(s, strlen(s));
    json_t *v;
    if (!out) return json_null();
    v = json_string(out);
    free(out);
    return v;
}

/* PoP: w30_search_d40ff0_p1_trigram_eligible @ .../search.py:_trigram_eligible_tokens */
json_t *w30_search_d40ff0_p1_trigram_eligible(const json_t *req) {
    const char *s = sr_str(req, "query");
    return json_bool(sr_trigram_eligible(s, strlen(s)) ? true : false);
}

/* PoP: w30_search_d40ff0_p1_short_cjk @ .../search.py:_has_short_cjk_token */
json_t *w30_search_d40ff0_p1_short_cjk(const json_t *req) {
    const char *s = sr_str(req, "raw");
    return json_bool(sr_has_short_cjk(s, strlen(s)) ? true : false);
}

/* PoP: w30_search_d40ff0_p1_trigram_route_ok @ .../search.py:_trigram_route_ok */
json_t *w30_search_d40ff0_p1_trigram_route_ok(const json_t *req) {
    const char *s = sr_str(req, "raw");
    int avail = sr_bool(req, "trigram_available", 0);
    return json_bool(sr_trigram_ok(s, strlen(s), avail) ? true : false);
}

/* PoP: w30_search_d40ff0_p1_flatten_text @ .../search.py:_flatten_text */
json_t *w30_search_d40ff0_p1_flatten_text(const json_t *req) {
    const json_t *d = sr_get(req, "decoded");
    json_t *v;
    sr_sb b;
    size_t i, n;
    int first = 1;
    if (!d) return json_string("");
    if (d->type == JSON_STRING) return json_string(d->str_val ? d->str_val : "");
    if (d->type != JSON_ARRAY) return json_string("");
    sr_sb_init(&b);
    n = json_len(d);
    for (i = 0; i < n; i++) {
        const json_t *part = json_get(d, i);
        const char *type, *text;
        if (!part || part->type != JSON_OBJECT) continue;
        type = sr_str(part, "type");
        if (strcmp(type, "text") != 0) continue;
        text = sr_str(part, "text");
        if (!text[0]) continue;
        if (!first) sr_sb_putc(&b, ' ');
        first = 0;
        sr_sb_puts(&b, text);
    }
    sr_strip(&b);
    if (b.n == 0) {
        sr_sb_free(&b);
        return json_string("[multimodal content]");
    }
    v = sr_sb_json(&b);
    sr_sb_free(&b);
    return v;
}

/* PoP: w30_search_d40ff0_p1_fields @ .../search.py:_search_message_fields */
json_t *w30_search_d40ff0_p1_fields(const json_t *req) {
    const json_t *f = sr_get(req, "fields");
    json_t *res;
    size_t i, k;
    char unknown[1024];
    size_t ulen = 0;

    res = json_object();
    if (sr_is_null(f)) {
        json_set(res, "ok", json_bool(true));
        json_set(res, "fields", json_null());
        return res;
    }
    if (f->type == JSON_STRING) {
        json_set(res, "ok", json_bool(false));
        json_set(res, "error", json_string(
            "TypeError: search fields must be a collection of field names, not a string"));
        return res;
    }
    if (f->type != JSON_ARRAY) {
        json_set(res, "ok", json_bool(false));
        json_set(res, "error", json_string("TypeError: fields is not iterable"));
        return res;
    }
    unknown[0] = '\0';
    k = json_len(f);
    for (i = 0; i < k; i++) {
        const char *name = sr_arr_str(f, i);
        size_t j, nl;
        int known = 0, dup = 0;
        if (!name) continue;
        nl = strlen(name);
        for (j = 0; j < SR_RESULT_FIELD_COUNT; j++)
            if (strcmp(name, SR_RESULT_FIELDS[j]) == 0) { known = 1; break; }
        if (known) continue;
        for (j = 0; j < ulen;) {
            size_t e = j;
            while (e < ulen && unknown[e] != '\n') e++;
            if (e - j == nl && memcmp(unknown + j, name, nl) == 0) { dup = 1; break; }
            j = e + 1;
        }
        if (dup) continue;
        if (ulen + nl + 2 < sizeof unknown) {
            memcpy(unknown + ulen, name, nl);
            ulen += nl;
            unknown[ulen++] = '\n';
            unknown[ulen] = '\0';
        }
    }
    if (ulen) {
        char *names[64];
        size_t cnt = 0, j, m;
        char msg[1400];
        for (j = 0; j < ulen && cnt < 64;) {
            size_t e = j;
            while (e < ulen && unknown[e] != '\n') e++;
            names[cnt++] = unknown + j;
            unknown[e] = '\0';
            j = e + 1;
        }
        for (j = 1; j < cnt; j++) {
            char *key = names[j];
            m = j;
            while (m > 0 && strcmp(names[m - 1], key) > 0) { names[m] = names[m - 1]; m--; }
            names[m] = key;
        }
        msg[0] = '\0';
        strcpy(msg, "ValueError: unknown search result field(s): ");
        for (j = 0; j < cnt; j++) {
            if (j) strcat(msg, ", ");
            strcat(msg, names[j]);
        }
        json_set(res, "ok", json_bool(false));
        json_set(res, "error", json_string(msg));
        return res;
    }
    {
        json_t *out = json_array();
        for (i = 0; i < SR_RESULT_FIELD_COUNT; i++) {
            for (k = 0; k < json_len(f); k++) {
                const char *name = sr_arr_str(f, k);
                if (name && strcmp(name, SR_RESULT_FIELDS[i]) == 0) {
                    json_append(out, json_string(SR_RESULT_FIELDS[i]));
                    break;
                }
            }
        }
        json_set(res, "ok", json_bool(true));
        json_set(res, "fields", out);
    }
    return res;
}

/* PoP: w30_search_d40ff0_p1_backfill_info @ .../search.py:_BackfillProgress.info */
json_t *w30_search_d40ff0_p1_backfill_info(const json_t *req) {
    const char *phase = sr_str(req, "phase");
    const json_t *tv = sr_get(req, "total");
    const json_t *iv = sr_get(req, "indexed");
    long long total = tv && tv->type == JSON_NUMBER ? (long long)tv->num_val : 0;
    long long done = iv && iv->type == JSON_NUMBER ? (long long)iv->num_val : 0;
    long long percent, indexed;
    json_t *o = json_object();
    percent = total ? (100 * done) / total : 100;
    if (percent > 100) percent = 100;
    indexed = done < total ? done : total;
    json_set(o, "phase", json_string(phase));
    json_set(o, "percent", json_number((double)percent));
    json_set(o, "indexed", json_number((double)indexed));
    json_set(o, "total", json_number((double)total));
    return o;
}

/* PoP: w30_search_d40ff0_p1_constants @ .../search.py:<query-normalisation constants> */
json_t *w30_search_d40ff0_p1_constants(void) {
    json_t *o = json_object();
    json_t *ranges = json_array();
    json_t *ops = json_array();
    json_t *skip = json_array();
    json_t *fields = json_array();
    size_t i;
    for (i = 0; i < 7; i++) {
        json_t *pair = json_array();
        json_append(pair, json_number((double)SR_CJK_RANGES[i][0]));
        json_append(pair, json_number((double)SR_CJK_RANGES[i][1]));
        json_append(ranges, pair);
    }
    json_append(ops, json_string("AND"));
    json_append(ops, json_string("NOT"));
    json_append(ops, json_string("OR"));
    json_append(skip, json_string("AND"));
    json_append(skip, json_string("NEAR"));
    json_append(skip, json_string("NOT"));
    json_append(skip, json_string("OR"));
    for (i = 0; i < SR_RESULT_FIELD_COUNT; i++) json_append(fields, json_string(SR_RESULT_FIELDS[i]));
    json_set(o, "max_fts5_query_chars", json_number(2048.0));
    json_set(o, "fts5_special_chars", json_string("+{}():\"^@/#&|~[]<>,;!?$=\\'"));
    json_set(o, "cjk_ranges", ranges);
    json_set(o, "fts_operators", ops);
    json_set(o, "like_skip_tokens", skip);
    json_set(o, "result_fields", fields);
    return o;
}

/* ══════════════════════════════════════════════════════════════════════ */
/* Gate w30_search_d40ff0_p2 — statement composition, routing, ranking.    */
/* ══════════════════════════════════════════════════════════════════════ */

typedef struct {
    const json_t *source_filter;
    const json_t *exclude_sources;
    const json_t *role_filter;
    const json_t *after_ts;
    const json_t *before_ts;
    int include_inactive;
} sr_filters;

/* kwargs come from the request: an explicit JSON null is Python's None. */
static void sr_filters_read(const json_t *req, sr_filters *f) {
    const json_t *sf = sr_get(req, "source_filter");
    const json_t *es = sr_get(req, "exclude_sources");
    const json_t *rf = sr_get(req, "role_filter");
    f->source_filter = sr_is_null(sf) ? NULL : sf;
    f->exclude_sources = sr_is_null(es) ? NULL : es;
    f->role_filter = sr_is_null(rf) ? NULL : rf;
    f->after_ts = sr_get(req, "after_ts");
    f->before_ts = sr_get(req, "before_ts");
    if (sr_is_null(f->after_ts)) f->after_ts = NULL;
    if (sr_is_null(f->before_ts)) f->before_ts = NULL;
    f->include_inactive = sr_bool(req, "include_inactive", 0);
    if (f->source_filter && f->source_filter->type != JSON_ARRAY) f->source_filter = NULL;
    if (f->exclude_sources && f->exclude_sources->type != JSON_ARRAY) f->exclude_sources = NULL;
    if (f->role_filter && f->role_filter->type != JSON_ARRAY) f->role_filter = NULL;
}

/* PoP: sr_filter_clauses @ .../search.py:_search_filter_clauses */
static void sr_filter_clauses(json_t *where, json_t *params, const sr_filters *f) {
    size_t i, n;
    if (!f->include_inactive) json_append(where, json_string("(m.active = 1 OR m.compacted = 1)"));
    json_append(where, json_string("COALESCE(m.display_kind, '') <> 'hidden'"));
    if (f->source_filter) {
        sr_sb b;
        n = json_len(f->source_filter);
        sr_sb_init(&b);
        sr_sb_puts(&b, "s.source IN (");
        for (i = 0; i < n; i++) { if (i) sr_sb_putc(&b, ','); sr_sb_putc(&b, '?'); }
        sr_sb_putc(&b, ')');
        json_append(where, sr_sb_json(&b));
        sr_sb_free(&b);
        for (i = 0; i < n; i++) {
            const char *s = sr_arr_str(f->source_filter, i);
            json_append(params, json_string(s ? s : ""));
        }
    }
    if (f->exclude_sources) {
        sr_sb b;
        n = json_len(f->exclude_sources);
        sr_sb_init(&b);
        sr_sb_puts(&b, "s.source NOT IN (");
        for (i = 0; i < n; i++) { if (i) sr_sb_putc(&b, ','); sr_sb_putc(&b, '?'); }
        sr_sb_putc(&b, ')');
        json_append(where, sr_sb_json(&b));
        sr_sb_free(&b);
        for (i = 0; i < n; i++) {
            const char *s = sr_arr_str(f->exclude_sources, i);
            json_append(params, json_string(s ? s : ""));
        }
    }
    if (f->role_filter && json_len(f->role_filter) > 0) {
        sr_sb b;
        n = json_len(f->role_filter);
        sr_sb_init(&b);
        sr_sb_puts(&b, "m.role IN (");
        for (i = 0; i < n; i++) { if (i) sr_sb_putc(&b, ','); sr_sb_putc(&b, '?'); }
        sr_sb_putc(&b, ')');
        json_append(where, sr_sb_json(&b));
        sr_sb_free(&b);
        for (i = 0; i < n; i++) {
            const char *s = sr_arr_str(f->role_filter, i);
            json_append(params, json_string(s ? s : ""));
        }
    }
    if (f->after_ts) {
        json_append(where, json_string("s.started_at >= ?"));
        json_append(params, json_number((double)(long long)f->after_ts->num_val));
    }
    if (f->before_ts) {
        json_append(where, json_string("s.started_at < ?"));
        json_append(params, json_number((double)(long long)f->before_ts->num_val));
    }
}

/* PoP: sr_select_sql @ .../search.py:_search_select_sql */
static void sr_select_sql(sr_sb *b, const char *snippet, const char *from_sql,
                          const json_t *where, const char *order_by, const char *limit_sql) {
    size_t i, n = where ? json_len(where) : 0;
    sr_sb_puts(b, "\n            SELECT m.id, m.session_id, m.role,\n                   ");
    sr_sb_puts(b, snippet);
    sr_sb_puts(b, ",\n                   ");
    sr_sb_puts(b, SR_SELECT_TAIL);
    sr_sb_puts(b, "\n            FROM ");
    sr_sb_puts(b, from_sql);
    sr_sb_puts(b, "\n            JOIN sessions s ON s.id = m.session_id\n            WHERE ");
    for (i = 0; i < n; i++) {
        const char *w = sr_arr_str(where, i);
        if (i) sr_sb_puts(b, " AND ");
        sr_sb_puts(b, w ? w : "");
    }
    sr_sb_puts(b, "\n            ");
    sr_sb_puts(b, order_by);
    sr_sb_puts(b, "\n            ");
    sr_sb_puts(b, limit_sql);
    sr_sb_puts(b, "\n        ");
}

/* PoP: w30_search_d40ff0_p2_filter_clauses @ .../search.py:_search_filter_clauses */
json_t *w30_search_d40ff0_p2_filter_clauses(const json_t *req) {
    sr_filters f;
    json_t *where, *params, *o, *seed_w, *seed_p;
    size_t i;
    sr_filters_read(req, &f);
    where = json_array();
    params = json_array();
    seed_w = (json_t *)sr_get(req, "where");
    seed_p = (json_t *)sr_get(req, "params");
    if (seed_w && seed_w->type == JSON_ARRAY)
        for (i = 0; i < json_len(seed_w); i++) json_append(where, json_copy(json_get(seed_w, i)));
    if (seed_p && seed_p->type == JSON_ARRAY)
        for (i = 0; i < json_len(seed_p); i++) json_append(params, json_copy(json_get(seed_p, i)));
    sr_filter_clauses(where, params, &f);
    o = json_object();
    json_set(o, "where", where);
    json_set(o, "params", params);
    return o;
}

/* PoP: w30_search_d40ff0_p2_select_sql @ .../search.py:_search_select_sql */
json_t *w30_search_d40ff0_p2_select_sql(const json_t *req) {
    const json_t *where = sr_get(req, "where");
    json_t *own = NULL;
    sr_sb b;
    json_t *v;
    if (!where || where->type != JSON_ARRAY) {
        own = json_array();
        where = own;
    }
    sr_sb_init(&b);
    sr_select_sql(&b, sr_str(req, "snippet_sql"), sr_str(req, "from_sql"), where,
                  sr_str(req, "order_by"), sr_str(req, "limit_sql"));
    v = sr_sb_json(&b);
    sr_sb_free(&b);
    if (own) json_free(own);
    return v;
}

/* PoP: w30_search_d40ff0_p2_fts_match_sql @ .../search.py:_fts_match_sql */
json_t *w30_search_d40ff0_p2_fts_match_sql(const json_t *req) {
    const char *table = sr_str(req, "table");
    const char *match = sr_str(req, "match_query");
    const char *order_by = sr_str(req, "order_by_sql");
    const json_t *lv = sr_get(req, "limit");
    const json_t *ov = sr_get(req, "offset");
    long long limit = lv && lv->type == JSON_NUMBER ? (long long)lv->num_val : 0;
    long long offset = ov && ov->type == JSON_NUMBER ? (long long)ov->num_val : 0;
    sr_filters f;
    json_t *where = json_array();
    json_t *params = json_array();
    sr_sb b, snip, from, m;
    json_t *o;
    sr_filters_read(req, &f);
    sr_sb_init(&m);
    sr_sb_puts(&m, table);
    sr_sb_puts(&m, " MATCH ?");
    json_append(where, sr_sb_json(&m));
    sr_sb_free(&m);
    json_append(params, json_string(match));
    sr_filter_clauses(where, params, &f);
    json_append(params, json_number((double)limit));
    json_append(params, json_number((double)offset));
    sr_sb_init(&snip);
    sr_sb_printf(&snip, "snippet(%s, -1, '>>>', '<<<', '...', 40) AS snippet", table);
    sr_sb_init(&from);
    sr_sb_printf(&from, "%s\n            JOIN messages m ON m.id = %s.rowid", table, table);
    sr_sb_init(&b);
    sr_select_sql(&b, sr_sb_cstr(&snip), sr_sb_cstr(&from), where, order_by, "LIMIT ? OFFSET ?");
    o = json_object();
    json_set(o, "sql", sr_sb_json(&b));
    json_set(o, "params", params);
    sr_sb_free(&b);
    sr_sb_free(&snip);
    sr_sb_free(&from);
    json_free(where);
    return o;
}

/* PoP: sr_like_params_into @ .../search.py:_like_params */
static void sr_like_params_into(json_t *params, const char *term, size_t n) {
    int k;
    for (k = 0; k < 3; k++) {
        sr_sb b;
        sr_sb_init(&b);
        sr_sb_putc(&b, '%');
        sr_escape_like(&b, term, n);
        sr_sb_putc(&b, '%');
        json_append(params, sr_sb_json(&b));
        sr_sb_free(&b);
    }
}

/* PoP: sr_compile_like_boolean @ .../search.py:_compile_like_boolean_query */
static char *sr_compile_like_boolean(const char *q, size_t n, json_t **params_out, char **snippet_out) {
    enum { MAXG = 16, MAXT = 32 };
    struct sr_term { const char *p; size_t n; int neg; };
    static struct { struct sr_term t[MAXT]; size_t n; } groups[MAXG];
    size_t i = 0, st = 0, ln = 0;
    size_t ngroups = 1;
    int negate_next = 0;
    json_t *params = json_array();
    sr_sb pred;
    char *snippet = NULL;
    size_t g;
    int bad = 0;

    for (g = 0; g < MAXG; g++) groups[g].n = 0;
    while (sr_like_token(q, n, &i, &st, &ln)) {
        const char *tok = q + st;
        if (sr_is_op(tok, ln, "OR")) {
            if (groups[ngroups - 1].n && ngroups < MAXG) ngroups++;
            negate_next = 0;
            continue;
        }
        if (sr_is_op(tok, ln, "AND") || sr_is_op(tok, ln, "NEAR")) continue;
        if (sr_is_op(tok, ln, "NOT")) { negate_next = 1; continue; }
        {
            size_t a, e;
            sr_trim_term(tok, ln, &a, &e);
            if (e > a && groups[ngroups - 1].n < MAXT) {
                size_t k = groups[ngroups - 1].n;
                groups[ngroups - 1].t[k].p = tok + a;
                groups[ngroups - 1].t[k].n = e - a;
                groups[ngroups - 1].t[k].neg = negate_next;
                groups[ngroups - 1].n = k + 1;
                negate_next = 0;
            }
        }
    }

    sr_sb_init(&pred);
    {
        int first_group = 1;
        for (g = 0; g < ngroups; g++) {
            size_t t, has_pos = 0, cnt = groups[g].n;
            int first_clause = 1;
            for (t = 0; t < cnt; t++) if (!groups[g].t[t].neg) has_pos = 1;
            if (!cnt || !has_pos) continue;
            if (!first_group) sr_sb_puts(&pred, " OR ");
            first_group = 0;
            sr_sb_putc(&pred, '(');
            for (t = 0; t < cnt; t++) {
                if (!first_clause) sr_sb_puts(&pred, " AND ");
                first_clause = 0;
                if (groups[g].t[t].neg) sr_sb_puts(&pred, "NOT ");
                sr_sb_puts(&pred, SR_LIKE_COALESCED_COLUMN_SQL);
                sr_like_params_into(params, groups[g].t[t].p, groups[g].t[t].n);
                if (!snippet && !groups[g].t[t].neg) {
                    snippet = (char *)malloc(groups[g].t[t].n + 1);
                    if (snippet) {
                        memcpy(snippet, groups[g].t[t].p, groups[g].t[t].n);
                        snippet[groups[g].t[t].n] = '\0';
                    } else {
                        bad = 1;
                    }
                }
            }
            sr_sb_putc(&pred, ')');
        }
    }
    *params_out = params;
    *snippet_out = snippet;
    if (bad || pred.n == 0) { sr_sb_free(&pred); return NULL; }
    return sr_sb_cstr(&pred);
}

/* PoP: w30_search_d40ff0_p2_like_boolean @ .../search.py:_compile_like_boolean_query */
json_t *w30_search_d40ff0_p2_like_boolean(const json_t *req) {
    const char *q = sr_str(req, "query");
    json_t *params = NULL, *o;
    char *snippet = NULL;
    char *pred = sr_compile_like_boolean(q, strlen(q), &params, &snippet);
    o = json_object();
    json_set(o, "predicate", json_string(pred ? pred : ""));
    json_set(o, "params", params ? params : json_array());
    json_set(o, "snippet_term", snippet ? json_string(snippet) : json_null());
    free(pred);
    free(snippet);
    return o;
}

/* PoP: w30_search_d40ff0_p2_like_fallback @ .../search.py:_search_messages_like_fallback */
json_t *w30_search_d40ff0_p2_like_fallback(const json_t *req) {
    const char *q = sr_str(req, "query");
    const char *sort = sr_str(req, "sort");
    const json_t *lv = sr_get(req, "limit");
    const json_t *ov = sr_get(req, "offset");
    double limit = lv && lv->type == JSON_NUMBER ? lv->num_val : 0.0;
    double offset = ov && ov->type == JSON_NUMBER ? ov->num_val : 0.0;
    sr_filters f;
    json_t *params = NULL, *where, *o;
    char *snippet = NULL;
    char *pred = sr_compile_like_boolean(q, strlen(q), &params, &snippet);
    const char *order = "DESC";
    sr_sb b, order_by;
    size_t i, n;
    if (!pred || !snippet) {
        free(pred);
        free(snippet);
        if (params) json_free(params);
        return json_null();
    }
    sr_filters_read(req, &f);
    where = json_array();
    {
        sr_sb w;
        sr_sb_init(&w);
        sr_sb_putc(&w, '(');
        sr_sb_puts(&w, pred);
        sr_sb_putc(&w, ')');
        json_append(where, sr_sb_json(&w));
        sr_sb_free(&w);
    }
    sr_filter_clauses(where, params, &f);
    {
        sr_sb t;
        sr_sb_init(&t);
        sr_sb_puts(&t, sort);
        sr_strip(&t);
        for (i = 0; i < t.n; i++) t.p[i] = (char)tolower((unsigned char)t.p[i]);
        if (t.n == 6 && memcmp(t.p, "oldest", 6) == 0) order = "ASC";
        sr_sb_free(&t);
    }
    sr_sb_init(&order_by);
    sr_sb_printf(&order_by, "ORDER BY m.timestamp %s, m.id %s", order, order);
    sr_sb_init(&b);
    sr_select_sql(&b, SR_LIKE_SNIPPET_SQL, "messages m", where, sr_sb_cstr(&order_by), "LIMIT ? OFFSET ?");
    o = json_object();
    {
        json_t *all = json_array();
        json_append(all, json_string(snippet));
        n = json_len(params);
        for (i = 0; i < n; i++) json_append(all, json_copy(json_get(params, i)));
        json_append(all, json_number(limit));
        json_append(all, json_number(offset));
        json_set(o, "sql", sr_sb_json(&b));
        json_set(o, "params", all);
    }
    sr_sb_free(&b);
    sr_sb_free(&order_by);
    json_free(where);
    json_free(params);
    free(pred);
    free(snippet);
    return o;
}

static long long sr_int_of(const json_t *v, long long def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    return def;
}

/* PoP: w30_search_d40ff0_p2_gap @ .../search.py:_search_unindexed_gap */
json_t *w30_search_d40ff0_p2_gap(const json_t *req) {
    const char *fts_query = sr_str(req, "fts_query");
    long long limit = sr_int_of(sr_get(req, "limit"), 20);
    const json_t *meta = sr_get(req, "meta");
    const json_t *hw, *pg;
    long long total, indexed;
    sr_filters f;
    json_t *terms, *where, *params, *o;
    sr_sb b;
    size_t i, n, st = 0, ln = 0, nterms;
    size_t tst = 0;
    if (sr_is_null(meta) || meta->type != JSON_OBJECT) return json_null();
    hw = sr_get(meta, "high_water");
    pg = sr_get(meta, "progress");
    if (sr_is_null(hw)) return json_null();
    if (sr_int_of(hw, 0) <= 0) return json_null();
    if (limit <= 0) return json_null();
    total = sr_int_of(hw, 0);
    indexed = sr_int_of(pg, 0);
    terms = json_array();
    {
        const char *q = fts_query;
        size_t qn = strlen(q);
        while (sr_like_token(q, qn, &st, &tst, &ln)) {
            size_t a, e;
            sr_trim_term(q + tst, ln, &a, &e);
            if (e > a && !sr_is_like_skip(q + tst + a, e - a))
                json_append(terms, sr_json_str(q + tst + a, e - a));
        }
    }
    nterms = json_len(terms);
    if (!nterms) { json_free(terms); return json_null(); }
    sr_filters_read(req, &f);
    where = json_array();
    params = json_array();
    json_append(where, json_string("m.id > ? AND m.id <= ?"));
    json_append(params, json_number((double)indexed));
    json_append(params, json_number((double)total));
    for (i = 0; i < nterms; i++) {
        const char *t = sr_arr_str(terms, i);
        json_append(where, json_string(SR_LIKE_ANY_COLUMN_SQL));
        sr_like_params_into(params, t ? t : "", t ? strlen(t) : 0);
    }
    sr_filter_clauses(where, params, &f);
    sr_sb_init(&b);
    sr_select_sql(&b, SR_LIKE_SNIPPET_SQL, "messages m", where, "ORDER BY m.timestamp DESC", "LIMIT ?");
    o = json_object();
    {
        json_t *all = json_array();
        const char *first = sr_arr_str(terms, 0);
        json_append(all, json_string(first ? first : ""));
        n = json_len(params);
        for (i = 0; i < n; i++) json_append(all, json_copy(json_get(params, i)));
        json_append(all, json_number((double)limit));
        json_set(o, "sql", sr_sb_json(&b));
        json_set(o, "params", all);
    }
    sr_sb_free(&b);
    json_free(terms);
    json_free(where);
    json_free(params);
    return o;
}

/* PoP: w30_search_d40ff0_p2_cjk_like @ .../search.py:_search_cjk (LIKE branch) */
json_t *w30_search_d40ff0_p2_cjk_like(const json_t *req) {
    const char *q = sr_str(req, "query");
    long long limit = sr_int_of(sr_get(req, "limit"), 20);
    long long offset = sr_int_of(sr_get(req, "offset"), 0);
    sr_filters f;
    json_t *tokens, *where, *params, *o;
    sr_sb rawb, b, w;
    size_t i, ntok;
    sr_sb_init(&rawb);
    sr_strip_cjk_wildcards(&rawb, q, strlen(q));
    sr_strip_quotes(&rawb);
    sr_strip(&rawb);
    tokens = sr_non_op_tokens(rawb.p ? rawb.p : "", rawb.n);
    if (json_len(tokens) == 0) {
        json_free(tokens);
        tokens = json_array();
        json_append(tokens, sr_json_str(rawb.p ? rawb.p : "", rawb.n));
    }
    ntok = json_len(tokens);
    sr_filters_read(req, &f);
    where = json_array();
    params = json_array();
    sr_sb_init(&w);
    sr_sb_putc(&w, '(');
    for (i = 0; i < ntok; i++) {
        const char *t = sr_arr_str(tokens, i);
        if (i) sr_sb_puts(&w, " OR ");
        sr_sb_puts(&w, SR_LIKE_ANY_COLUMN_SQL);
        sr_like_params_into(params, t ? t : "", t ? strlen(t) : 0);
    }
    sr_sb_putc(&w, ')');
    json_append(where, sr_sb_json(&w));
    sr_sb_free(&w);
    sr_filter_clauses(where, params, &f);
    sr_sb_init(&b);
    sr_select_sql(&b, SR_LIKE_SNIPPET_SQL, "messages m", where, "ORDER BY m.timestamp DESC",
                  "LIMIT ? OFFSET ?");
    o = json_object();
    {
        json_t *all = json_array();
        const char *first = sr_arr_str(tokens, 0);
        json_append(all, json_string(first ? first : ""));
        for (i = 0; i < json_len(params); i++) json_append(all, json_copy(json_get(params, i)));
        json_append(all, json_number((double)limit));
        json_append(all, json_number((double)offset));
        json_set(o, "sql", sr_sb_json(&b));
        json_set(o, "params", all);
    }
    sr_sb_free(&b);
    sr_sb_free(&rawb);
    json_free(tokens);
    json_free(where);
    json_free(params);
    return o;
}

/* PoP: sr_session_score @ .../search.py:search_sessions_by_id.score */
static int sr_session_score(const char *needle, const json_t *row) {
    static const char *const keys[2] = {"id", "_lineage_root_id"};
    size_t k, nl = strlen(needle);
    int best = 2;
    for (k = 0; k < 2; k++) {
        const json_t *v = row && row->type == JSON_OBJECT ? json_obj_get(row, keys[k]) : NULL;
        sr_sb t;
        size_t i;
        int eq = 1, prefix = 1;
        if (!v || v->type != JSON_STRING) continue;
        if (!v->str_val || !v->str_val[0]) continue;
        sr_sb_init(&t);
        sr_sb_puts(&t, v->str_val);
        for (i = 0; i < t.n; i++) t.p[i] = (char)tolower((unsigned char)t.p[i]);
        if (t.n != nl) eq = 0;
        if (t.n < nl) prefix = 0;
        if (eq && memcmp(t.p, needle, nl) != 0) eq = 0;
        if (prefix && memcmp(t.p, needle, nl) != 0) prefix = 0;
        if (eq) best = 0;
        else if (prefix && best > 1) best = 1;
        sr_sb_free(&t);
    }
    return best;
}

/* PoP: w30_search_d40ff0_p2_session_rank @ .../search.py:search_sessions_by_id */
json_t *w30_search_d40ff0_p2_session_rank(const json_t *req) {
    const char *q = sr_str(req, "query");
    long long limit = sr_int_of(sr_get(req, "limit"), 20);
    const json_t *rows = sr_get(req, "rows");
    sr_sb needle;
    json_t *o, *ranked;
    size_t i, n, taken;
    sr_sb_init(&needle);
    sr_sb_puts(&needle, q);
    sr_strip(&needle);
    for (i = 0; i < needle.n; i++) needle.p[i] = (char)tolower((unsigned char)needle.p[i]);
    o = json_object();
    if (needle.n == 0 || limit <= 0 || !rows || rows->type != JSON_ARRAY) {
        json_set(o, "needle", json_null());
        json_set(o, "fetch_limit", json_null());
        json_set(o, "ranked", json_array());
        sr_sb_free(&needle);
        return o;
    }
    sr_sb_cstr(&needle);
    n = json_len(rows);
    ranked = json_array();
    {
        int *score = (int *)calloc(n ? n : 1, sizeof(int));
        int *used = (int *)calloc(n ? n : 1, sizeof(int));
        if (!score || !used) {
            free(score);
            free(used);
            json_set(o, "needle", json_string(needle.p));
            json_set(o, "fetch_limit", json_number((double)(limit * 4)));
            json_set(o, "ranked", ranked);
            sr_sb_free(&needle);
            return o;
        }
        for (i = 0; i < n; i++) score[i] = sr_session_score(needle.p, json_get(rows, i));
        taken = 0;
        while (taken < n) {
            int best = 3;
            size_t pick = n;
            for (i = 0; i < n; i++)
                if (!used[i] && score[i] < best) { best = score[i]; pick = i; }
            if (pick == n) break;
            used[pick] = 1;
            taken++;
            if ((long long)taken <= limit) json_append(ranked, json_copy(json_get(rows, pick)));
        }
        free(score);
        free(used);
    }
    json_set(o, "needle", json_string(needle.p));
    json_set(o, "fetch_limit", json_number((double)(limit * 4)));
    json_set(o, "ranked", ranked);
    sr_sb_free(&needle);
    return o;
}

/* PoP: w30_search_d40ff0_p2_order_by @ .../search.py:_FTS_ORDER_BY.get */
json_t *w30_search_d40ff0_p2_order_by(const json_t *req) {
    const json_t *sv = sr_get(req, "sort");
    if (!sv || sv->type != JSON_STRING) return json_string(SR_FTS_RANK_ORDER_BY);
    {
        sr_sb t;
        size_t i;
        sr_sb_init(&t);
        sr_sb_puts(&t, sv->str_val ? sv->str_val : "");
        sr_strip(&t);
        for (i = 0; i < t.n; i++) t.p[i] = (char)tolower((unsigned char)t.p[i]);
        if (t.n == 6 && memcmp(t.p, "newest", 6) == 0) {
            sr_sb_free(&t);
            return json_string(SR_NEWEST_ORDER_BY);
        }
        if (t.n == 6 && memcmp(t.p, "oldest", 6) == 0) {
            sr_sb_free(&t);
            return json_string(SR_OLDEST_ORDER_BY);
        }
        sr_sb_free(&t);
    }
    return json_string(SR_FTS_RANK_ORDER_BY);
}

/* PoP: sr_has_tool_role @ .../search.py:_search_messages_impl */
static int sr_has_tool_role(const json_t *role_filter) {
    size_t i, n;
    if (!role_filter || role_filter->type != JSON_ARRAY) return 0;
    n = json_len(role_filter);
    for (i = 0; i < n; i++) {
        const char *s = sr_arr_str(role_filter, i);
        if (s && strcmp(s, "tool") == 0) return 1;
    }
    return 0;
}
static int sr_wants_unindexed(const json_t *role_filter, const json_t *source_filter) {
    size_t i, n;
    if (sr_has_tool_role(role_filter)) return 1;
    if (!source_filter || source_filter->type != JSON_ARRAY) return 0;
    n = json_len(source_filter);
    for (i = 0; i < n; i++) {
        const char *s = sr_arr_str(source_filter, i);
        if (s && (strcmp(s, "cron") == 0 || strcmp(s, "subagent") == 0)) return 1;
    }
    return 0;
}
static int sr_all_space(const char *s, size_t n) {
    size_t i = 0, l;
    while (i < n) {
        unsigned cp = sr_cp(s, n, i, &l);
        if (!sr_is_space(cp)) return 0;
        i += l;
    }
    return 1;
}

/* PoP: w30_search_d40ff0_p2_route @ .../search.py:_search_messages_impl:<route> */
json_t *w30_search_d40ff0_p2_route(const json_t *req) {
    const char *q = sr_str(req, "query");
    size_t n = strlen(q);
    const json_t *role_filter = sr_get(req, "role_filter");
    const json_t *source_filter = sr_get(req, "source_filter");
    int stale = sr_bool(req, "fts_stale", 0);
    int enabled = sr_bool(req, "fts_enabled", 1);
    int avail = sr_bool(req, "trigram_available", 0);
    char *san;
    if (n == 0 || sr_all_space(q, n)) return json_string("empty");
    san = sr_sanitize_fts5(q, n);
    if (!san || !san[0]) { free(san); return json_string("empty"); }
    if (sr_has_tool_role(role_filter)) { free(san); return json_string("tool_like"); }
    if (stale) { free(san); return json_string("stale"); }
    if (!enabled) { free(san); return json_string("disabled"); }
    if (sr_contains_cjk(san, strlen(san))) {
        sr_sb raw;
        int ok;
        sr_sb_init(&raw);
        sr_sb_puts(&raw, san);
        sr_strip_quotes(&raw);
        sr_strip(&raw);
        ok = sr_trigram_ok(raw.p ? raw.p : "", raw.n, avail) &&
             !sr_wants_unindexed(role_filter, source_filter);
        sr_sb_free(&raw);
        free(san);
        return json_string(ok ? "trigram" : "like_scan");
    }
    free(san);
    return json_string("fts");
}

/* PoP: w30_search_d40ff0_p2_describe_path @ .../search.py:_describe_search_path */
json_t *w30_search_d40ff0_p2_describe_path(const json_t *req) {
    const char *q = sr_str(req, "query");
    int stale = sr_bool(req, "fts_stale", 0);
    int avail = sr_bool(req, "trigram_available", 0);
    char *san;
    sr_sb raw;
    int ok;
    if (stale) return json_string("like_scan_fts_stale");
    san = sr_sanitize_fts5(q, strlen(q));
    if (!san || !san[0]) { free(san); return json_string("empty"); }
    if (!sr_contains_cjk(san, strlen(san))) { free(san); return json_string("fts5"); }
    sr_sb_init(&raw);
    sr_sb_puts(&raw, san);
    sr_strip_quotes(&raw);
    sr_strip(&raw);
    ok = sr_trigram_ok(raw.p ? raw.p : "", raw.n, avail);
    sr_sb_free(&raw);
    free(san);
    return json_string(ok ? "trigram" : "like_scan");
}

/* PoP: w30_search_d40ff0_p2_sql_constants @ .../search.py:<pinned SQL + limits> */
json_t *w30_search_d40ff0_p2_sql_constants(void) {
    json_t *o = json_object();
    json_t *order = json_object();
    json_t *excl = json_array();
    json_t *fields = json_array();
    sr_sb tri_s, tri_bare, trigram_sweep, trigram_chunk;
    size_t i;
    sr_sb_init(&tri_s);
    sr_trigram_session_sql(&tri_s, "s");
    sr_sb_init(&tri_bare);
    sr_trigram_session_sql(&tri_bare, "");
    sr_sb_init(&trigram_sweep);
    sr_sb_puts(&trigram_sweep,
               "INSERT INTO messages_fts_trigram(rowid, content, tool_name) "
               "SELECT m.id, m.content, m.tool_name FROM messages m JOIN sessions s ON s.id = m.session_id "
               "WHERE m.id > ? AND m.id <= ? AND m.role <> 'tool' AND ");
    sr_sb_puts(&trigram_sweep, sr_sb_cstr(&tri_s));
    sr_sb_puts(&trigram_sweep,
               " AND NOT EXISTS (SELECT 1 FROM messages_fts_trigram_docsize d WHERE d.id = m.id)");
    sr_sb_init(&trigram_chunk);
    sr_sb_puts(&trigram_chunk,
               "INSERT INTO messages_fts_trigram(rowid, content, tool_name) "
               "SELECT m.id, m.content, m.tool_name FROM messages m JOIN sessions s ON s.id = m.session_id "
               "WHERE m.id > ? AND m.id <= ? AND m.role <> 'tool' AND ");
    sr_sb_puts(&trigram_chunk, sr_sb_cstr(&tri_s));
    json_set(o, "search_select_tail", json_string(SR_SELECT_TAIL));
    json_set(o, "like_snippet_sql", json_string(SR_LIKE_SNIPPET_SQL));
    json_set(o, "like_any_column_sql", json_string(SR_LIKE_ANY_COLUMN_SQL));
    json_set(o, "like_coalesced_column_sql", json_string(SR_LIKE_COALESCED_COLUMN_SQL));
    json_set(o, "fts_rank_order_by", json_string(SR_FTS_RANK_ORDER_BY));
    json_set(order, "newest", json_string(SR_NEWEST_ORDER_BY));
    json_set(order, "oldest", json_string(SR_OLDEST_ORDER_BY));
    json_set(o, "fts_order_by", order);
    json_set(o, "context_window_sql", json_string(SR_CONTEXT_WINDOW_SQL));
    json_set(o, "base_boundary_sweep_sql", json_string(SR_BASE_BOUNDARY_SWEEP_SQL));
    json_set(o, "trigram_boundary_sweep_sql", sr_sb_json(&trigram_sweep));
    json_set(o, "chunk_insert_sql", json_string(SR_CHUNK_INSERT_SQL));
    json_set(o, "trigram_chunk_insert_sql", sr_sb_json(&trigram_chunk));
    json_set(o, "fts_tool_content_prefix_chars", json_number(8192.0));
    json_append(excl, json_string("cron"));
    json_append(excl, json_string("subagent"));
    json_set(o, "fts_trigram_excluded_sources", excl);
    json_set(o, "fts_stale_key", json_string("fts_stale"));
    json_set(o, "trigram_session_sql_s", sr_sb_json(&tri_s));
    json_set(o, "trigram_session_sql_bare", sr_sb_json(&tri_bare));
    for (i = 0; i < SR_RESULT_FIELD_COUNT; i++) json_append(fields, json_string(SR_RESULT_FIELDS[i]));
    json_set(o, "search_message_result_fields", fields);
    sr_sb_free(&tri_s);
    sr_sb_free(&tri_bare);
    sr_sb_free(&trigram_sweep);
    sr_sb_free(&trigram_chunk);
    return o;
}
