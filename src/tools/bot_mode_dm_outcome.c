/*
 * bot_mode_dm_outcome.c — C home for upstream Hermes tools/bot_mode_dm_outcome.py.
 * PoP: w12_botmodedm_efcb4f_outcome_line @ tools/bot_mode_dm_outcome.py:outcome_line
 * PoP: w12_botmodedm_efcb4f_combined_summary @ tools/bot_mode_dm_outcome.py:combined_summary
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "noc_w12_botmodedm_efcb4f.h"

typedef struct { char *data; size_t len, cap; } w12_sb_t;
static void w12_sb_init(w12_sb_t *b) {
    b->cap = 256; b->len = 0; b->data = (char *)malloc(b->cap);
    if (b->data) b->data[0] = '\0';
}
static void w12_sb_free(w12_sb_t *b) { free(b->data); b->data = NULL; b->len = b->cap = 0; }
static void w12_sb_addn(w12_sb_t *b, const char *s, size_t n) {
    if (!b->data || !s || !n) return;
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap;
        while (nc < b->len + n + 1) nc *= 2;
        char *nd = (char *)realloc(b->data, nc);
        if (!nd) return;
        b->data = nd; b->cap = nc;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n; b->data[b->len] = '\0'; /* NUL at logical length */
}
static void w12_sb_add(w12_sb_t *b, const char *s) { if (s) w12_sb_addn(b, s, strlen(s)); }
static char *w12_sb_take(w12_sb_t *b) {
    char *p = b->data; b->data = NULL; b->len = b->cap = 0; return p;
}
static char *w12_fmt(const char *fmt, ...) {
    char buf[8192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return strdup(buf);
}
static int w12_py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count ? 1 : 0;
        default: return 0;
    }
}
static const char *w12_jstr(const json_t *n, const char *def) {
    return (n && n->type == JSON_STRING && n->str_val) ? n->str_val : def;
}
static char *w12_strip(const char *s) {
    char *out; size_t n;
    if (!s) return NULL;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
    n = strlen(s);
    while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' ||
                 s[n-1] == '\r' || s[n-1] == '\f' || s[n-1] == '\v')) n--;
    out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

/* str(entry.get("error") or entry.get("detail") or "").strip().rstrip(".") */
static char *w12_why(const json_t *entry) {
    json_t *e = json_obj_get(entry, "error");
    json_t *d = json_obj_get(entry, "detail");
    json_t *chosen = w12_py_truthy(e) ? e : (w12_py_truthy(d) ? d : NULL);
    char *base = strdup(w12_jstr(chosen, ""));
    char *out = w12_strip(base ? base : "");
    size_t n;
    free(base);
    if (!out) return strdup("");
    n = strlen(out);
    while (n && out[n - 1] == '.') { out[--n] = '\0'; } /* NUL at new length */
    return out;
}

char *w12_botmodedm_efcb4f_outcome_line(const char *to, const json_t *entry) {
    const char *status = w12_jstr(json_obj_get(entry, "status"), "");
    if (!strcmp(status, "settled")) {
        if (w12_py_truthy(json_obj_get(entry, "reply")))
            return w12_fmt("%s answered.", to);
        return w12_fmt("%s finished its turn without replying.", to);
    }
    {
        char *why = w12_why(entry);
        char *res;
        if (!strcmp(status, "cancelled")) {
            free(why);
            return w12_fmt("%s never answered: the request was cancelled.", to);
        }
        if (!strcmp(status, "queued") || !strcmp(status, "claimed") || !strcmp(status, "pending")) {
            const char *state = w12_jstr(json_obj_get(entry, "target_state"), "");
            const char *where = "it has not finished";
            if (!strcmp(state, "unreachable")) where = "its gateway is not reachable";
            else if (!strcmp(state, "waiting_approval")) where = "it is waiting for its own user to approve a command";
            else if (!strcmp(state, "waiting_clarify")) where = "it is waiting for its own user to answer a question";
            else if (!strcmp(state, "paused")) where = "it is not starting new turns (restarting or updating)";
            else if (!strcmp(state, "working")) where = "its turn is still running";
            free(why);
            return w12_fmt("%s never answered within the wait: %s. The request is still on their side and its answer will NOT wake you; tell your user, do not resend.", to, where);
        }
        if (!strcmp(status, "ambiguous")) {
            res = w12_fmt("%s never answered: whether it received the request is unknown (%s). It may still run once their gateway is back; do not resend.", to, why);
            free(why);
            return res;
        }
        {
            int safe = (strstr(why, "NOT sent") || strstr(why, "safe to resend")) ? 1 : 0;
            const char *w = why[0] ? why : "the delivery failed";
            res = w12_fmt("%s never answered: %s. %s", to, w,
                          safe ? "Nothing reached them; you may resend." : "Do not resend blindly.");
        }
        free(why);
        return res;
    }
}

char *w12_botmodedm_efcb4f_combined_summary(const json_t *entries) {
    size_t n = (entries && entries->type == JSON_ARRAY) ? json_len(entries) : 0;
    char **lines = (char **)calloc(n ? n : 1, sizeof(char *));
    w12_sb_t out;
    size_t i, j;
    w12_sb_init(&out);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(entries, i);
        json_t *t = e ? json_obj_get(e, "to") : NULL;
        const char *to = (t && t->type == JSON_STRING && t->str_val && t->str_val[0])
                         ? t->str_val : "a teammate";
        lines[i] = w12_botmodedm_efcb4f_outcome_line(to, e);
    }
    /* stable sort: key = (" never answered" in line) -> False (0) first */
    for (i = 1; i < n; i++) {
        char *key = lines[i];
        int kf = strstr(key, " never answered") ? 1 : 0;
        j = i - 1;
        while ((int)j >= 0) {
            int jf = strstr(lines[j], " never answered") ? 1 : 0;
            if (jf <= kf) break;
            lines[j + 1] = lines[j];
            if (j == 0) { j = (size_t)-1; break; }
            j--;
        }
        lines[j + 1] = key;
    }
    for (i = 0; i < n; i++) {
        if (i) w12_sb_add(&out, " ");
        w12_sb_add(&out, lines[i]);
        free(lines[i]);
    }
    free(lines);
    return w12_sb_take(&out);
}
