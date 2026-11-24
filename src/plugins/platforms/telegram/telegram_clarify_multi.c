/*
 * telegram_clarify_multi.c - C11 port of upstream
 * plugins/platforms/telegram/telegram_clarify_multi.py
 * (gate w26_telegramcl_ca0267).
 *
 * PoP: w26_telegramcl_ca0267_keyboard @ plugins/platforms/telegram/telegram_clarify_multi.py:multi_keyboard
 * PoP: w26_telegramcl_ca0267_pending_multi @ plugins/platforms/telegram/telegram_clarify_multi.py:pending_multi_entry
 * PoP: w26_telegramcl_ca0267_token_action @ plugins/platforms/telegram/telegram_clarify_multi.py:handle_multi_token
 * PoP: w26_telegramcl_ca0267_resolve_ack @ plugins/platforms/telegram/telegram_clarify_multi.py:resolve_clarify_choice
 *
 * The Telegram client, the pending-clarify registry, the i18n lookup and the
 * callback answer are seams; what is ported is what those values decide -- the
 * keyboard rows, which token means what, the labels a Send submits, and the
 * acknowledgement body the card is edited to.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_telegramcl_ca0267.h"

/* i18n keys (the t() seam is fixed to these documented keys). */
#define TC_TOAST_INVALID   "platform.telegram.prompt.invalid_choice"
#define TC_TOAST_SELECT    "platform.telegram.prompt.select_one"
#define TC_LABEL_OTHER     "Other"
#define TC_LABEL_SEND      "Send"

static int tc_truthy(const json_t *v) {
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

/* ------------------------------------------------------------ growable text */

typedef struct { char *p; size_t len, cap; } tc_buf;

static void tc_buf_add(tc_buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 128;
        char *np;
        while (cap < b->len + n + 1) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) return;
        b->p = np;
        b->cap = cap;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void tc_buf_str(tc_buf *b, const char *s) { tc_buf_add(b, s, strlen(s)); }

static char *tc_buf_take(tc_buf *b) {
    if (!b->p) {
        char *empty = (char *)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    return b->p;
}

/* shown[:limit] over code points. */
static char *tc_cp_slice(const char *s, size_t limit) {
    size_t n = strlen(s), i = 0, seen = 0;
    while (i < n && seen < limit) {
        i++;
        while (i < n && ((unsigned char)s[i] & 0xC0) == 0x80) i++;
        seen++;
    }
    {
        char *out = (char *)malloc(i + 1);
        if (!out) return NULL;
        memcpy(out, s, i);
        out[i] = '\0';
        return out;
    }
}

/* html.escape(s) with quote=True. */
static char *tc_html_escape(const char *s) {
    tc_buf b = {0};
    size_t i, n = strlen(s);
    tc_buf_add(&b, "", 0);
    for (i = 0; i < n; i++) {
        switch (s[i]) {
        case '&':  tc_buf_str(&b, "&amp;"); break;
        case '<':  tc_buf_str(&b, "&lt;"); break;
        case '>':  tc_buf_str(&b, "&gt;"); break;
        case '"':  tc_buf_str(&b, "&quot;"); break;
        case '\'': tc_buf_str(&b, "&#x27;"); break;
        default:   tc_buf_add(&b, s + i, 1); break;
        }
    }
    return tc_buf_take(&b);
}

/* Python str.isdigit() over the token tail. */
static int tc_all_digits(const char *s) {
    if (!s[0]) return 0;
    for (; *s; s++) if (*s < '0' || *s > '9') return 0;
    return 1;
}

/* ------------------------------------------------------------------ keyboard */

json_t *w26_telegramcl_ca0267_keyboard(const json_t *req) {
    const char *cid = json_get_str(req, "clarify_id", "");
    long count = (long)json_get_num(req, "count", 0.0);
    const json_t *selected = json_obj_get(req, "selected");
    json_t *rows = json_array();
    json_t *out = json_object();
    long idx;

    if (!selected || selected->type != JSON_ARRAY) selected = NULL;
    for (idx = 0; idx < count; idx++) {
        json_t *row = json_array();
        json_t *btn = json_object();
        char *text, *data;
        size_t need = strlen(cid) + 48;
        int on = 0;
        size_t i;
        if (selected) {
            for (i = 0; i < json_len(selected); i++) {
                const json_t *s = json_get(selected, i);
                if (s && s->type == JSON_NUMBER && (long)s->num_val == idx) on = 1;
            }
        }
        text = (char *)malloc(16);
        data = (char *)malloc(need);
        if (text) snprintf(text, 16, "%s %ld", on ? "\xe2\x98\x91" : "\xe2\x98\x90", idx + 1);
        if (data) snprintf(data, need, "cl:%s:t%ld", cid, idx);
        json_set(btn, "text", json_string(text ? text : ""));
        json_set(btn, "callback_data", json_string(data ? data : ""));
        free(text);
        free(data);
        json_append(row, btn);
        json_append(rows, row);
    }
    {
        const char *tail[2] = {"other", "go"};
        const char *label[2] = {TC_LABEL_OTHER, TC_LABEL_SEND};
        int k;
        for (k = 0; k < 2; k++) {
            json_t *row = json_array();
            json_t *btn = json_object();
            char *data = (char *)malloc(strlen(cid) + 16);
            if (data) snprintf(data, strlen(cid) + 16, "cl:%s:%s", cid, tail[k]);
            json_set(btn, "text", json_string(label[k]));
            json_set(btn, "callback_data", json_string(data ? data : ""));
            free(data);
            json_append(row, btn);
            json_append(rows, row);
        }
    }
    json_set(out, "rows", rows);
    return out;
}

/* ------------------------------------------------------------ pending entry */

/* pending_multi_entry: entry is not None and entry.multi_select and entry.choices */
static const json_t *tc_pending(const json_t *entry, const json_t **choices_out) {
    const json_t *choices;
    if (!entry || entry->type != JSON_OBJECT) return NULL;
    if (!tc_truthy(json_obj_get(entry, "multi_select"))) return NULL;
    choices = json_obj_get(entry, "choices");
    if (!tc_truthy(choices)) return NULL;
    if (choices_out) *choices_out = choices;
    return entry;
}

json_t *w26_telegramcl_ca0267_pending_multi(const json_t *req) {
    return json_bool(tc_pending(json_obj_get(req, "entry"), NULL) != NULL);
}

/* ------------------------------------------------------------ token_action */

static void tc_push_answer(json_t *answers, const char *key) {
    json_append(answers, json_string(key));
}

/* sorted(selected): the tick set the Send tap submits. */
static json_t *tc_sorted_selected(const json_t *selected_node) {
    json_t *sorted_sel = json_array();
    double *vals;
    size_t i, n = 0, k;
    if (!selected_node || selected_node->type != JSON_ARRAY) return sorted_sel;
    vals = (double *)calloc(json_len(selected_node), sizeof(double));
    if (!vals) return sorted_sel;
    for (i = 0; i < json_len(selected_node); i++) {
        const json_t *s = json_get(selected_node, i);
        if (!s || s->type != JSON_NUMBER) continue;
        vals[n++] = s->num_val;
    }
    for (i = 1; i < n; i++) {
        double v = vals[i];
        size_t j = i;
        while (j > 0 && vals[j - 1] > v) { vals[j] = vals[j - 1]; j--; }
        vals[j] = v;
    }
    for (k = 0; k < n; k++) json_append(sorted_sel, json_number(vals[k]));
    free(vals);
    return sorted_sel;
}

json_t *w26_telegramcl_ca0267_token_action(const json_t *req) {
    const char *token = json_get_str(req, "token", "");
    const json_t *selected_node = json_obj_get(req, "selected");
    const json_t *choices = NULL;
    const json_t *entry = tc_pending(json_obj_get(req, "entry"), &choices);
    json_t *out = json_object();
    json_t *answers = json_array();
    json_t *labels = json_array();
    int is_toggle = token[0] == 't' && tc_all_digits(token + 1);
    long idx = 0;

    json_set(out, "handled", json_bool(0));
    json_set(out, "expired", json_bool(0));
    json_set(out, "answers", answers);
    json_set(out, "toggle", json_null());
    json_set(out, "resolve", json_null());

    if (!is_toggle && strcmp(token, "go") != 0) return out;
    json_set(out, "handled", json_bool(1));

    if (!entry) {
        /* timed out / answered by text / reset: retire the card */
        json_set(out, "expired", json_bool(1));
        return out;
    }
    if (is_toggle) {
        json_t *toggle;
        idx = atol(token + 1);
        if (idx < 0 || idx >= (long)json_len(choices)) {
            tc_push_answer(answers, TC_TOAST_INVALID);
            return out;
        }
        /* _toggle(adapter, query, clarify_id, entry, selected, idx, user_display) */
        toggle = json_object();
        json_set(toggle, "idx", json_number((double)idx));
        json_set(toggle, "selected", tc_sorted_selected(selected_node));
        json_set(out, "toggle", toggle);
        return out;
    }
    /* Send: labels = [str(choices[i]).strip() for i in sorted(selected) if i < len(choices)] */
    {
        long len = (long)json_len(choices);
        size_t i;
        json_t *sorted_sel = tc_sorted_selected(selected_node);
        for (i = 0; i < json_len(sorted_sel); i++) {
            long j = (long)json_get(sorted_sel, i)->num_val;
            const json_t *c;
            const char *text;
            char *trimmed;
            if (j >= len) continue;
            if (j < 0) j += len;
            if (j < 0 || j >= len) continue;
            c = json_get(choices, (size_t)j);
            text = (c && c->type == JSON_STRING && c->str_val) ? c->str_val : "";
            trimmed = (char *)malloc(strlen(text) + 1);
            if (!trimmed) continue;
            {
                const char *p = text;
                size_t tlen = strlen(text);
                while (tlen && (*p == ' ' || (unsigned char)*p == '\t' || *p == '\n' ||
                                *p == '\r' || *p == '\f' || *p == '\v')) { p++; tlen--; }
                while (tlen && (p[tlen - 1] == ' ' || p[tlen - 1] == '\t' || p[tlen - 1] == '\n' ||
                                p[tlen - 1] == '\r' || p[tlen - 1] == '\f' ||
                                p[tlen - 1] == '\v')) tlen--;
                memcpy(trimmed, p, tlen);
                trimmed[tlen] = '\0';
            }
            json_append(labels, json_string(trimmed));
            free(trimmed);
        }
        json_free(sorted_sel);
        if (json_len(labels) == 0) {
            tc_push_answer(answers, TC_TOAST_SELECT);
            return out;
        }
        {
            /* json.dumps(labels) with the default ", " separator. */
            tc_buf rt = {0}, shown = {0};
            size_t k;
            json_t *res = json_object();
            tc_buf_str(&rt, "[");
            for (k = 0; k < json_len(labels); k++) {
                const json_t *lv = json_get(labels, k);
                const char *sv = (lv && lv->type == JSON_STRING && lv->str_val) ? lv->str_val : "";
                if (k) { tc_buf_str(&rt, ", "); tc_buf_str(&shown, ", "); }
                {
                    char *q = json_escape_string(sv);
                    tc_buf_str(&rt, q ? q : "\"\"");
                    free(q);
                }
                tc_buf_str(&shown, sv);
            }
            tc_buf_str(&rt, "]");
            json_set(res, "resolved_text", json_string(rt.p ? rt.p : "[]"));
            json_set(res, "shown", json_string(shown.p ? shown.p : ""));
            json_set(out, "resolve", res);
            free(rt.p);
            free(shown.p);
        }
        json_free(labels);
        return out;
    }
}

/* ------------------------------------------------------------- resolve_ack */

json_t *w26_telegramcl_ca0267_resolve_ack(const json_t *req) {
    const char *cid = json_get_str(req, "clarify_id", "");
    const char *resolved_text = json_get_str(req, "resolved_text", "");
    const json_t *shown_node = json_obj_get(req, "shown");
    const char *shown = resolved_text;
    const char *user = json_get_str(req, "user", "");
    const char *message_text = json_get_str(req, "message_text", "");
    json_t *out = json_object();

    if (shown_node && shown_node->type == JSON_STRING && shown_node->str_val)
        shown = shown_node->str_val;

    json_set(out, "expired", json_bool(0));
    if (!tc_truthy(json_obj_get(req, "resolved"))) {
        json_set(out, "answer", json_null());
        json_set(out, "html", json_null());
        json_set(out, "logged", json_null());
        json_set(out, "expired", json_bool(1));
        return out;
    }
    {
        char *sliced = tc_cp_slice(shown, W26_TELEGRAMCL_CA0267_ACK_MAX_CHARS);
        char *body = tc_html_escape(message_text ? message_text : "");
        char *who = tc_html_escape(user ? user : "");
        char *what = tc_html_escape(shown ? shown : "");
        tc_buf answer = {0}, html = {0};
        json_t *logged = json_object();

        tc_buf_str(&answer, "\xe2\x9c\x93 ");
        tc_buf_str(&answer, sliced ? sliced : "");
        tc_buf_str(&html, "\xe2\x9d\x93 ");
        tc_buf_str(&html, body ? body : "");
        tc_buf_str(&html, "\n\n<b>");
        tc_buf_str(&html, who ? who : "");
        tc_buf_str(&html, ":</b> ");
        tc_buf_str(&html, what ? what : "");

        json_set(out, "answer", json_string(answer.p ? answer.p : ""));
        json_set(out, "html", json_string(html.p ? html.p : ""));
        json_set(logged, "clarify_id", json_string(cid));
        json_set(logged, "shown", json_string(shown ? shown : ""));
        json_set(logged, "user", json_string(user ? user : ""));
        json_set(logged, "resolved_text", json_string(resolved_text ? resolved_text : ""));
        json_set(out, "logged", logged);
        free(answer.p);
        free(html.p);
        free(sliced);
        free(body);
        free(who);
        free(what);
    }
    return out;
}
