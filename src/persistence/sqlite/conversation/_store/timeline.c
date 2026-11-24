/*
 * timeline.c - C11 port of upstream
 * persistence/sqlite/conversation/_store/timeline.py (gate w26_timeline_c7132f).
 *
 * PoP: w26_timeline_c7132f_display_rows_sql @ persistence/sqlite/conversation/_store/timeline.py:_display_rows_sql
 * PoP: w26_timeline_c7132f_prompt_preview @ persistence/sqlite/conversation/_store/timeline.py:_prompt_preview
 *
 * The SQLite connection is the seam: the index probe arrives as `indexed` and a
 * row's stored payload arrives as `content`.  Ported here: the display-set SQL
 * the count and the page are read through, and the one-line prompt preview.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_timeline_c7132f.h"

#define TL_PREVIEW_MAX 120
#define TL_PREVIEW_CUT 119

typedef struct {
    char *p;
    size_t len, cap;
} tl_sb;

static void tl_cat(tl_sb *b, const char *s) {
    size_t n = s ? strlen(s) : 0;
    size_t need = b->len + n + 1;
    if (need > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *np;
        while (cap < need) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) return;
        b->p = np;
        b->cap = cap;
    }
    if (n) memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static json_t *tl_take(tl_sb *b) {
    json_t *out;
    if (!b->p) return json_string("");
    out = json_string(b->p);
    free(b->p);
    b->p = NULL;
    return out;
}

/* everything after this line is generated from the live f-strings */
/* messages_sql.py:DISPLAY_VISIBLE_SQL -- model-only rows never enter the display set. */
static const char *TL_VIS = " AND COALESCE(json_extract((CASE WHEN json_valid(display_metadata) THEN display_metadata ELSE json_object() END), '$.model_only'), 0) = 0";

/* compaction_display.py:not_a_later_tool_copy_sql(alias) */
static char *tl_notl(const char *alias) {
    tl_sb b = {NULL, 0, 0};
    tl_cat(&b, " AND NOT (");
    tl_cat(&b, alias);
    tl_cat(&b, ".role = 'tool' AND ");
    tl_cat(&b, alias);
    tl_cat(&b, ".message_uid IS NOT NULL AND EXISTS (SELECT 1 FROM messages AS original WHERE original.session_id = ");
    tl_cat(&b, alias);
    tl_cat(&b, ".session_id AND original.timestamp = ");
    tl_cat(&b, alias);
    tl_cat(&b, ".timestamp AND original.message_uid = ");
    tl_cat(&b, alias);
    tl_cat(&b, ".message_uid AND original.role = 'tool' AND original.active = 0 AND original.compacted = 1 AND +original.display_order < ");
    tl_cat(&b, alias);
    tl_cat(&b, ".display_order AND COALESCE(json_extract((CASE WHEN json_valid(original.display_metadata) THEN original.display_metadata ELSE json_object() END), '$.model_only'), 0) = 0))");
    if (!b.p) {
        char *e = (char *)malloc(1);
        if (e) e[0] = '\0';
        return e;
    }
    return b.p;
}

/* append a just-built fragment, then release it */
static void tl_cat_take(tl_sb *b, char *s) {
    tl_cat(b, s ? s : "");
    free(s);
}

json_t *w26_timeline_c7132f_display_rows_sql(int indexed, int users_only) {
    tl_sb b = {NULL, 0, 0};
    if (indexed) {
    tl_cat(&b, "WITH display_rows AS (\n            SELECT (SELECT candidate.id FROM messages candidate\n                    WHERE candidate.session_id = :sid\n                      AND candidate.display_order = m.display_order\n                      AND (candidate.active = 1 OR candidate.compacted = 1)");
    tl_cat(&b, TL_VIS);
    tl_cat_take(&b, tl_notl("candidate"));
    tl_cat(&b, "\n                    ORDER BY candidate.active DESC, candidate.id DESC LIMIT 1) AS row_id,\n                   m.display_order AS sort_id\n            FROM messages m WHERE session_id = :sid AND (active = 1 OR compacted = 1)");
    if (users_only) tl_cat(&b, " AND role = 'user'");
    tl_cat(&b, TL_VIS);
    tl_cat_take(&b, tl_notl("m"));
    tl_cat(&b, "\n            GROUP BY m.display_order\n        )");
    } else {
    tl_cat(&b, "WITH ranked AS (\n        SELECT id, MIN(id) OVER identity AS sort_id,\n               ROW_NUMBER() OVER (identity ORDER BY active DESC, id DESC) AS preference\n        FROM messages WHERE session_id = :sid AND (active = 1 OR compacted = 1)");
    if (users_only) tl_cat(&b, " AND role = 'user'");
    tl_cat(&b, TL_VIS);
    tl_cat(&b, "\n        WINDOW identity AS (PARTITION BY role,\n            CASE WHEN role = 'user' THEN timeline_identity_content(content, display_kind) ELSE content END,\n            timestamp, tool_call_id, tool_calls, tool_name)\n    ), display_rows AS (SELECT id AS row_id, sort_id FROM ranked WHERE preference = 1)");
    }
    return tl_take(&b);
}

/* ---- prompt preview ----------------------------------------------------- */

static int tl_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python len() counts code points, not bytes. */
static size_t tl_cp_len(const char *s) {
    size_t n = 0;
    const unsigned char *u = (const unsigned char *)s;
    while (*u) {
        if ((*u & 0xC0) != 0x80) n++;
        u++;
    }
    return n;
}

static int tl_ci_prefix(const char *s, const char *pfx) {
    size_t i = 0;
    while (pfx[i]) {
        char a = s[i], b = pfx[i];
        if (!a) return 0;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
        i++;
    }
    return 1;
}

static int tl_word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

/* timeline.py:_SYNTHETIC_PROMPT, anchored at the start of the collapsed text. */
static int tl_synthetic(const char *t) {
    const char *s = t;
    while (*s && tl_is_ws(*s)) s++;
    if (tl_ci_prefix(s, "[IMPORTANT: Background process ")) return 1;
    if (tl_ci_prefix(s, "[ASYNC ")) {
        const char *p = s + 7;
        if (tl_ci_prefix(p, "DELEGATION ")) p += 11;
        if (tl_ci_prefix(p, "BATCH ")) p += 6;
        if (tl_ci_prefix(p, "COMPLETE") && !tl_word_char(p[8])) return 1;
    }
    if (tl_ci_prefix(s, "A background fan-out of ")) {
        const char *p = s + 24;
        const char *digits = p;
        while (*p >= '0' && *p <= '9') p++;
        if (p > digits &&
            tl_ci_prefix(p, " subagent(s) you dispatched earlier has finished.")) return 1;
    }
    if (tl_ci_prefix(s, "A background subagent you dispatched earlier has finished.")) return 1;
    return 0;
}

/* Python truthiness. */
static int tl_truthy(const json_t *v) {
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

/* " ".join(part if str else part.get("text", "") for part in content if str|dict) */
static json_t *tl_join_parts(const json_t *content) {
    tl_sb b = {NULL, 0, 0};
    size_t i, n = json_len(content);
    int first = 1;
    for (i = 0; i < n; i++) {
        const json_t *part = json_get(content, i);
        const char *txt = NULL;
        if (part && part->type == JSON_STRING) {
            txt = part->str_val ? part->str_val : "";
        } else if (part && part->type == JSON_OBJECT) {
            const json_t *t = json_obj_get(part, "text");
            txt = (t && t->type == JSON_STRING && t->str_val) ? t->str_val : "";
        } else {
            continue;
        }
        if (!first) tl_cat(&b, " ");
        tl_cat(&b, txt);
        first = 0;
    }
    return tl_take(&b);
}

json_t *w26_timeline_c7132f_prompt_preview(const json_t *content, const json_t *display_kind,
                                           const json_t *summary) {
    json_t *joined = NULL, *out;
    const char *text;
    tl_sb b = {NULL, 0, 0};
    size_t len;

    /* project_compaction_message_for_display drops every non-default display_kind. */
    if (display_kind && display_kind->type != JSON_NULL) return json_string("");
    /* ...and a compressed summary is not a prompt. */
    if (tl_truthy(summary)) return json_string("");

    if (content && content->type == JSON_ARRAY) {
        joined = tl_join_parts(content);
        text = joined && joined->str_val ? joined->str_val : "";
    } else if (content && content->type == JSON_STRING) {
        text = content->str_val ? content->str_val : "";
    } else {
        return json_string(""); /* None / a number / a bool is not text */
    }

    /* " ".join(content.split()): collapse every whitespace run, trim both ends. */
    {
        const char *s = text;
        int pending = 0;
        while (*s && tl_is_ws(*s)) s++;
        while (*s) {
            if (tl_is_ws(*s)) { pending = 1; s++; continue; }
            if (pending && b.len) tl_cat(&b, " ");
            pending = 0;
            {
                char one[2];
                one[0] = *s;
                one[1] = '\0';
                tl_cat(&b, one);
            }
            s++;
        }
    }
    if (joined) json_free(joined);

    if (!b.p || b.len == 0) { free(b.p); return json_string(""); }
    if (tl_synthetic(b.p)) { free(b.p); return json_string(""); }

    if (tl_cp_len(b.p) <= TL_PREVIEW_MAX) {
        out = json_string(b.p);
        free(b.p);
        return out;
    }

    /* text[:119].rstrip() + "…" */
    {
        size_t at = 0, seen = 0, raw = b.len;
        /* Cut after TL_PREVIEW_CUT whole code points, never inside one. */
        while (at < raw && seen < TL_PREVIEW_CUT) {
            unsigned char c = (unsigned char)b.p[at];
            size_t step = 1;
            if ((c & 0xE0) == 0xC0) step = 2;
            else if ((c & 0xF0) == 0xE0) step = 3;
            else if ((c & 0xF8) == 0xF0) step = 4;
            at += step;
            seen++;
        }
        if (at > raw) at = raw;
        b.p[at] = '\0';
        len = at;
        while (len > 0 && tl_is_ws(b.p[len - 1])) b.p[--len] = '\0';
    }
    {
        tl_sb o = {NULL, 0, 0};
        tl_cat(&o, b.p);
        tl_cat(&o, "\xe2" "\x80" "\xa6"); /* U+2026 HORIZONTAL ELLIPSIS */
        free(b.p);
        return tl_take(&o);
    }
}

