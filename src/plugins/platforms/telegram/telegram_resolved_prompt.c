/*
 * telegram_resolved_prompt.c -- real C11 port of the pure shaping/decision
 * logic of upstream Hermes
 *   plugins/platforms/telegram/telegram_resolved_prompt.py.
 *
 * PoP: w22_telegramre_8bc0fe_escape @ plugins/platforms/telegram/telegram_resolved_prompt.py:edit_keeping_prompt
 * PoP: w22_telegramre_8bc0fe_strip @ plugins/platforms/telegram/telegram_resolved_prompt.py:edit_keeping_prompt
 * PoP: w22_telegramre_8bc0fe_plan @ plugins/platforms/telegram/telegram_resolved_prompt.py:edit_keeping_prompt
 *
 * The rest of the module -- the `telegram.error` imports, `query.message`,
 * `adapter._ea_deadline_line()`, `query.edit_message_text(...)` and
 * `adapter._edit_md_quiet(...)` -- is the live transport: those stay upstream.
 * Everything the function decides about the edited text is here.
 */
#include "hermes_json.h"
#include "noc_w22_telegramre_8bc0fe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- a small, always NUL-terminated string builder ------------------------ */

typedef struct {
    char *p;
    size_t n;
    size_t cap;
    int failed;
} tg18_buf;

static char *tg18_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static void tg18_init(tg18_buf *b) {
    b->p = NULL;
    b->n = 0;
    b->cap = 0;
    b->failed = 0;
}

static void tg18_reserve(tg18_buf *b, size_t extra) {
    size_t need, cap;
    char *np;
    if (b->failed) return;
    need = b->n + extra + 1;
    if (need <= b->cap) return;
    cap = b->cap ? b->cap : 64;
    while (cap < need) cap *= 2;
    np = (char *)realloc(b->p, cap);
    if (!np) {
        b->failed = 1;
        return;
    }
    b->p = np;
    b->cap = cap;
}

static void tg18_add(tg18_buf *b, const char *text, size_t n) {
    tg18_reserve(b, n);
    if (b->failed) return;
    if (n) memcpy(b->p + b->n, text, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void tg18_adds(tg18_buf *b, const char *text) {
    tg18_add(b, text, strlen(text));
}

/* Hands the bytes over; the empty path still yields a terminated "". */
static char *tg18_take(tg18_buf *b) {
    char *out;
    if (b->failed) {
        free(b->p);
        b->p = NULL;
        return NULL;
    }
    if (!b->p) return tg18_dup("");
    out = b->p;
    b->p = NULL;
    return out;
}

/* --- html.escape(text, quote=True) ---------------------------------------- */

static char *tg18_escape(const char *text) {
    tg18_buf b;
    const char *p;
    tg18_init(&b);
    if (!text) text = "";
    for (p = text; *p; p++) {
        switch (*p) {
        case '&':  tg18_adds(&b, "&amp;"); break;
        case '<':  tg18_adds(&b, "&lt;"); break;
        case '>':  tg18_adds(&b, "&gt;"); break;
        case '"':  tg18_adds(&b, "&quot;"); break;
        case '\'': tg18_adds(&b, "&#x27;"); break;
        default:   tg18_add(&b, p, 1); break;
        }
    }
    return tg18_take(&b);
}

/* --- str.replace(deadline, "") -------------------------------------------- */

static char *tg18_strip(const char *body, const char *deadline) {
    tg18_buf b;
    const char *p;
    size_t dl;
    tg18_init(&b);
    if (!body) return tg18_take(&b);
    if (!deadline) deadline = "";
    dl = strlen(deadline);
    if (dl == 0) {              /* "abc".replace("", "") is "abc" */
        tg18_adds(&b, body);
        return tg18_take(&b);
    }
    p = body;
    while (*p) {
        if (!strncmp(p, deadline, dl)) {
            p += dl;
            continue;
        }
        tg18_add(&b, p, 1);
        p++;
    }
    return tg18_take(&b);
}

/* --- the composed edit text ---------------------------------------------- */

/* f"{body}\n\n<em dash> {html.escape(decision)}" */
static char *tg18_compose(const char *body, const char *decision) {
    tg18_buf b;
    char *escaped;
    tg18_init(&b);
    tg18_adds(&b, body ? body : "");
    tg18_adds(&b, "\n\n\xe2\x80\x94 ");
    escaped = tg18_escape(decision);
    if (!escaped) {
        free(tg18_take(&b));
        return NULL;
    }
    tg18_adds(&b, escaped);
    free(escaped);
    return tg18_take(&b);
}

/* --- the ported API ------------------------------------------------------- */

json_t *w22_telegramre_8bc0fe_escape(const char *text) {
    char *out = tg18_escape(text);
    json_t *node;
    if (!out) return json_null();
    node = json_string(out);
    free(out);
    return node;
}

json_t *w22_telegramre_8bc0fe_strip(const char *body, const char *deadline) {
    char *out = tg18_strip(body, deadline);
    json_t *node;
    if (!out) return json_null();
    node = json_string(out);
    free(out);
    return node;
}

json_t *w22_telegramre_8bc0fe_plan(const json_t *body, const char *deadline,
                                   const char *decision, const char *error) {
    json_t *out = json_object();
    const char *kind = error ? error : "";
    int readable = body && body->type == JSON_STRING && body->str_val && body->str_val[0];

    if (!readable) {
        /* `isinstance(body, str) and body` failed: the decision-only edit runs. */
        json_set(out, "path", json_string("quiet_no_body"));
        json_set(out, "text", json_null());
        json_set(out, "parse_mode", json_null());
        json_set(out, "quiet_decision", json_string(decision ? decision : ""));
        return out;
    }

    {
        char *stripped = tg18_strip(body->str_val, deadline);
        char *text = stripped ? tg18_compose(stripped, decision) : NULL;
        free(stripped);
        json_set(out, "text", text ? json_string(text) : json_null());
        free(text);
    }
    json_set(out, "parse_mode", json_string("HTML"));

    if (!strcmp(kind, "")) {
        /* The edit landed: the prompt keeps its rendering plus the decision. */
        json_set(out, "path", json_string("edit"));
        json_set(out, "quiet_decision", json_null());
    } else if (!strcmp(kind, "BadRequest")) {
        /* `except BadRequest: pass` -- fall through to the decision-only edit. */
        json_set(out, "path", json_string("quiet_after_badrequest"));
        json_set(out, "quiet_decision", json_string(decision ? decision : ""));
    } else if (!strcmp(kind, "NetworkError")) {
        /* `except NetworkError: return` -- the edit may have landed; no second one. */
        json_set(out, "path", json_string("return_after_network"));
        json_set(out, "quiet_decision", json_null());
    } else {
        /* Any other exception propagates out of the resolver. */
        json_set(out, "path", json_string("raise"));
        json_set(out, "quiet_decision", json_null());
    }
    return out;
}
