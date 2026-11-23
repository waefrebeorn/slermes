/*
 * history_commentary.c — C home for upstream Hermes agent/history_commentary.py.
 *
 * PoP: w10_historycom_1bfe19_commentary_items @ agent/history_commentary.py:commentary_items
 * PoP: w10_historycom_1bfe19_final_items @ agent/history_commentary.py:_final_items
 * PoP: w10_historycom_1bfe19_without_flattened_commentary @ agent/history_commentary.py:_without_flattened_commentary
 *
 * The display-only projection's pure core: phase-filtered message-item text and
 * whole-line commentary omission. Redaction/home/config side effects stay in
 * the Python host and are not part of these exports.
 */
#include "noc_w10_historycom_1bfe19.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const COMMENTARY_WITHHELD = "commentary_withheld"; /* mirrored literal */

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char *strip_ws(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (!out) return xstrdup("");
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static void lower_ascii(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* mode 0: commentary only. mode 1: final/final_answer/None (unphased). */
static json_t *phase_items(const json_t *message, int mode) {
    (void)COMMENTARY_WITHHELD;
    json_t *result = json_array();
    const json_t *items = message ? json_obj_get(message, "codex_message_items") : NULL;
    json_t *parsed = NULL;
    if (items && items->type == JSON_STRING) {
        parsed = json_parse(items->str_val, NULL);
        items = parsed;
    }
    if (!items || items->type != JSON_ARRAY) { json_free(parsed); return result; }

    for (size_t i = 0; i < items->c.count; i++) {
        const json_t *item = items->c.items[i];
        if (!item || item->type != JSON_OBJECT) continue;
        const json_t *t = json_obj_get(item, "type");
        if (!t || t->type != JSON_STRING || strcmp(t->str_val, "message")) continue;
        const json_t *r = json_obj_get(item, "role");
        if (!r || r->type != JSON_STRING || strcmp(r->str_val, "assistant")) continue;
        const json_t *ph = json_obj_get(item, "phase");
        if (ph && ph->type != JSON_STRING && ph->type != JSON_NULL) continue;
        char norm[256] = {0};
        int has_phase = (ph && ph->type == JSON_STRING);
        if (has_phase) {
            char *tmp = strip_ws(ph->str_val);
            if (strlen(tmp) >= sizeof(norm)) tmp[sizeof(norm) - 1] = '\0';
            strcpy(norm, tmp);
            lower_ascii(norm);
            free(tmp);
        }
        int match;
        if (mode == 0) match = has_phase && !strcmp(norm, "commentary");
        else match = !has_phase || !strcmp(norm, "final") || !strcmp(norm, "final_answer");
        if (!match) continue;

        const json_t *content = json_obj_get(item, "content");
        if (!content || content->type != JSON_ARRAY) continue;
        size_t cap = 256, used = 0;
        char *text = (char *)malloc(cap);
        if (!text) continue;
        text[0] = '\0';
        for (size_t j = 0; j < content->c.count; j++) {
            const json_t *part = content->c.items[j];
            if (!part || part->type != JSON_OBJECT) continue;
            const json_t *pt = json_obj_get(part, "type");
            if (!pt || pt->type != JSON_STRING || strcmp(pt->str_val, "output_text")) continue;
            const json_t *pv = json_obj_get(part, "text");
            if (!pv || pv->type != JSON_STRING || !pv->str_val) continue;
            size_t need = used + strlen(pv->str_val) + 1;
            if (need > cap) { cap = need; char *nt = (char *)realloc(text, cap); if (!nt) break; text = nt; }
            memcpy(text + used, pv->str_val, strlen(pv->str_val) + 1);
            used += strlen(pv->str_val);
        }
        char *stripped = strip_ws(text);
        free(text);
        if (stripped[0]) json_append(result, json_string(stripped));
        free(stripped);
    }
    json_free(parsed);
    return result;
}

json_t *w10_historycom_1bfe19_commentary_items(const json_t *message) {
    return phase_items(message, 0);
}

json_t *w10_historycom_1bfe19_final_items(const json_t *message) {
    return phase_items(message, 1);
}

static char *find_from(const char *hay, const char *needle, size_t from) {
    if (from > strlen(hay)) return NULL;
    return strstr(hay + from, needle);
}

static void rstrip_nl(char *s) {
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == '\n') s[--n] = '\0';
}

static void lstrip_nl(char *s) {
    if (!s[0]) return;
    size_t n = strlen(s), i = 0;
    while (i < n && s[i] == '\n') i++;
    if (i) memmove(s, s + i, n - i + 1);
}

static char *concat3(const char *a, const char *b, const char *c) {
    size_t n = strlen(a) + strlen(b) + strlen(c) + 1;
    char *out = (char *)malloc(n);
    if (!out) return xstrdup("");
    snprintf(out, n, "%s%s%s", a, b, c);
    return out;
}

static int cmp_len_desc(const void *pa, const void *pb) {
    const char *a = *(const char *const *)pa;
    const char *b = *(const char *const *)pb;
    size_t la = strlen(a), lb = strlen(b);
    if (la != lb) return (la > lb) ? -1 : 1;
    return strcmp(a, b);
}

char *w10_historycom_1bfe19_without_flattened_commentary(const char *reasoning, const json_t *commentary) {
    char *remaining = xstrdup(reasoning ? reasoning : "");
    size_t n = (commentary && commentary->type == JSON_ARRAY) ? commentary->c.count : 0;
    const char **texts = (const char **)calloc(n ? n : 1, sizeof(*texts));
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *v = commentary->c.items[i];
        if (!v || v->type != JSON_STRING) continue;
        int dup = 0;
        for (size_t k = 0; k < m; k++) if (!strcmp(texts[k], v->str_val)) { dup = 1; break; }
        if (!dup) texts[m++] = v->str_val;
    }
    qsort(texts, m, sizeof(*texts), cmp_len_desc);

    for (size_t idx = 0; idx < m; idx++) {
        const char *text = texts[idx];
        if (!text[0]) continue;
        size_t search_from = 0;
        for (;;) {
            char *at = find_from(remaining, text, search_from);
            if (!at) break;
            size_t pos = (size_t)(at - remaining);
            size_t end = pos + strlen(text);
            int line_start = (pos == 0 || remaining[pos - 1] == '\n');
            int line_end = (remaining[end] == '\0' || remaining[end] == '\n');
            if (!line_start || !line_end) { search_from = end; continue; }
            char *before = (char *)malloc(pos + 1);
            memcpy(before, remaining, pos); before[pos] = '\0';
            char *after = xstrdup(remaining + end);
            int dbl = (strlen(before) >= 2 && before[strlen(before) - 1] == '\n' && before[strlen(before) - 2] == '\n')
                      || (strlen(after) >= 2 && after[0] == '\n' && after[1] == '\n');
            rstrip_nl(before);
            lstrip_nl(after);
            char *sep = (before[0] && after[0]) ? (dbl ? "\n\n" : "\n") : "";
            char *next = concat3(before, sep, after);
            free(before); free(after); free(remaining);
            remaining = next;
            search_from = 0;
        }
    }
    free(texts);
    return remaining;
}
