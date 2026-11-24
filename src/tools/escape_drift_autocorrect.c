/*
 * escape_drift_autocorrect.c - C11 port of upstream
 * tools/escape_drift_autocorrect.py (gate w26_escapedrif_a472f8).
 *
 * PoP: w26_escapedrif_a472f8_halve @ tools/escape_drift_autocorrect.py:_halve_backslash_runs
 * PoP: w26_escapedrif_a472f8_runs_even @ tools/escape_drift_autocorrect.py:_all_backslash_runs_even
 * PoP: w26_escapedrif_a472f8_strip_quotes @ tools/escape_drift_autocorrect.py:_strip_spurious_quote_escapes
 * PoP: w26_escapedrif_a472f8_quotes_all_spurious @ tools/escape_drift_autocorrect.py:_quote_escapes_all_spurious
 * PoP: w26_escapedrif_a472f8_backslash_correct @ tools/escape_drift_autocorrect.py:maybe_correct_backslash_doubling
 * PoP: w26_escapedrif_a472f8_quote_correct @ tools/escape_drift_autocorrect.py:maybe_correct_quote_escaping
 * PoP: w26_escapedrif_a472f8_escape_drift @ tools/escape_drift_autocorrect.py:maybe_correct_escape_drift
 * PoP: w26_escapedrif_a472f8_notes @ tools/escape_drift_autocorrect.py:<module notes>
 *
 * The file (patch target) is the seam: it arrives as `content`, a fixed input.
 * What is ported is the decision over three strings and nothing else.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_escapedrif_a472f8.h"

#define ED_NOTE_QUOTE \
    "escape-drift auto-corrected: stripped a spurious backslash before a quote/apostrophe " \
    "in old_string/new_string (a JSON-double-escaping wire artifact) before matching."
#define ED_NOTE_BACKSLASH \
    "escape-drift auto-corrected: halved doubled backslash runs in old_string " \
    "before matching; new_string has no backslashes and was left unchanged."

/* ---- growable byte buffer ---------------------------------------------- */

typedef struct {
    char *p;
    size_t len, cap;
} ed_sb;

static int ed_reserve(ed_sb *b, size_t extra) {
    size_t need = b->len + extra + 1;
    char *np;
    if (need <= b->cap) return 1;
    if (b->cap == 0) b->cap = 32;
    while (b->cap < need) b->cap *= 2;
    np = (char *)realloc(b->p, b->cap);
    if (!np) return 0;
    b->p = np;
    return 1;
}

static int ed_putn(ed_sb *b, const char *s, size_t n) {
    if (!ed_reserve(b, n)) return 0;
    if (n) memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
    return 1;
}

static int ed_putc(ed_sb *b, char c) { return ed_putn(b, &c, 1); }

static json_t *ed_take(ed_sb *b) {
    json_t *out;
    if (!b->p) return json_string("");
    out = json_string(b->p);
    free(b->p);
    b->p = NULL;
    return out;
}

/* ---- string helpers ----------------------------------------------------- */

static const char *ed_safe(const char *s) { return s ? s : ""; }

static char *ed_strdup(const char *s) {
    size_t n = strlen(ed_safe(s));
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, ed_safe(s), n + 1);
    return p;
}

static int ed_contains(const char *hay, const char *needle) {
    return strstr(ed_safe(hay), ed_safe(needle)) != NULL;
}

static int ed_has_backslash(const char *s) { return strchr(ed_safe(s), '\\') != NULL; }

/* ---- _halve_backslash_runs --------------------------------------------- */

json_t *w26_escapedrif_a472f8_halve(const char *s) {
    ed_sb b = {NULL, 0, 0};
    size_t i = 0, n = strlen(ed_safe(s));
    const char *t = ed_safe(s);
    while (i < n) {
        if (t[i] == '\\') {
            size_t run = 0, half, k;
            while (i + run < n && t[i + run] == '\\') run++;
            half = run / 2;
            for (k = 0; k < half; k++)
                if (!ed_putc(&b, '\\')) { free(b.p); return json_string(""); }
            i += run;
            continue;
        }
        if (!ed_putc(&b, t[i])) { free(b.p); return json_string(""); }
        i++;
    }
    return ed_take(&b);
}

/* ---- _all_backslash_runs_even ------------------------------------------ */

static int ed_runs_even(const char *s) {
    size_t i = 0, n = strlen(ed_safe(s));
    const char *t = ed_safe(s);
    while (i < n) {
        if (t[i] == '\\') {
            size_t run = 0;
            while (i + run < n && t[i + run] == '\\') run++;
            if (run % 2 != 0) return 0;
            i += run;
            continue;
        }
        i++;
    }
    return 1;
}

json_t *w26_escapedrif_a472f8_runs_even(const char *s) { return json_bool(ed_runs_even(s)); }

/* ---- _strip_spurious_quote_escapes ------------------------------------- */

json_t *w26_escapedrif_a472f8_strip_quotes(const char *s) {
    ed_sb b = {NULL, 0, 0};
    size_t i = 0, n = strlen(ed_safe(s));
    const char *t = ed_safe(s);
    while (i < n) {
        if (t[i] == '\\' && i + 1 < n && (t[i + 1] == '\'' || t[i + 1] == '"')) {
            if (!ed_putc(&b, t[i + 1])) { free(b.p); return json_string(""); }
            i += 2;
            continue;
        }
        if (!ed_putc(&b, t[i])) { free(b.p); return json_string(""); }
        i++;
    }
    return ed_take(&b);
}

/* ---- _quote_escapes_all_spurious --------------------------------------- */

/* A quote kind that appears escaped anywhere must be escaped by exactly one
 * backslash at EVERY occurrence, in every string. */
static int ed_kind_all_spurious(const json_t *strings, char quote) {
    size_t n = json_len(strings), i;
    int seen_escaped = 0;
    char needle[3];
    needle[0] = '\\';
    needle[1] = quote;
    needle[2] = '\0';

    for (i = 0; i < n; i++) {
        const json_t *v = json_get(strings, i);
        const char *s;
        if (!v || v->type != JSON_STRING) continue;
        s = v->str_val ? v->str_val : "";
        if (ed_contains(s, needle)) { seen_escaped = 1; break; }
    }
    if (!seen_escaped) return 1;

    for (i = 0; i < n; i++) {
        const json_t *v = json_get(strings, i);
        const char *s;
        size_t p = 0;
        if (!v || v->type != JSON_STRING) continue;
        s = v->str_val ? v->str_val : "";
        while (s[p]) {
            if (s[p] == quote) {
                size_t run = 0;
                size_t q = p;
                while (q > 0 && s[q - 1] == '\\') { run++; q--; }
                if (run != 1) return 0;
            }
            p++;
        }
    }
    return 1;
}

static int ed_quotes_all_spurious(const json_t *strings) {
    if (!strings || strings->type != JSON_ARRAY) return 1;
    if (!ed_kind_all_spurious(strings, '\'')) return 0;
    if (!ed_kind_all_spurious(strings, '"')) return 0;
    return 1;
}

json_t *w26_escapedrif_a472f8_quotes_all_spurious(const json_t *strings) {
    return json_bool(ed_quotes_all_spurious(strings));
}

/* ---- result builders ---------------------------------------------------- */

static json_t *ed_pair(const char *o, const char *nw) {
    json_t *r = json_object();
    json_set(r, "old", o ? json_string(o) : json_null());
    json_set(r, "new", nw ? json_string(nw) : json_null());
    json_set(r, "error", json_null());
    return r;
}

static json_t *ed_fail(void) {
    json_t *r = json_object();
    json_set(r, "old", json_null());
    json_set(r, "new", json_null());
    json_set(r, "error", json_string(W26_ESCAPEDRIF_A472F8_ERROR));
    return r;
}

/* ---- maybe_correct_backslash_doubling ---------------------------------- */

/* Returns 1 and fills *ho and *hn (malloc'd) on correction, 0 when unchanged,
 * -1 when the replacement backslashes make the correction ambiguous. */
static int ed_backslash_try(const char *old_string, const char *new_string,
                            const char *content, char **ho, char **hn) {
    json_t *halved;
    const char *h;
    if (!ed_has_backslash(old_string) || ed_contains(content, old_string)) return 0;
    if (!ed_runs_even(old_string)) return 0;
    halved = w26_escapedrif_a472f8_halve(old_string);
    h = halved && halved->type == JSON_STRING && halved->str_val ? halved->str_val : "";
    if (!ed_contains(content, h)) { json_free(halved); return 0; }
    if (ed_has_backslash(new_string)) { json_free(halved); return -1; }
    *ho = ed_strdup(h);
    *hn = ed_strdup(ed_safe(new_string));
    json_free(halved);
    if (!*ho || !*hn) { free(*ho); free(*hn); return 0; }
    return 1;
}

json_t *w26_escapedrif_a472f8_backslash_correct(const char *old_string,
                                               const char *new_string,
                                               const char *content) {
    char *ho = NULL, *hn = NULL;
    json_t *r;
    int rc = ed_backslash_try(old_string, new_string, content, &ho, &hn);
    if (rc < 0) return ed_fail();
    if (rc == 0) return ed_pair(old_string, new_string);
    r = ed_pair(ho, hn);
    free(ho);
    free(hn);
    return r;
}

/* ---- maybe_correct_quote_escaping -------------------------------------- */

/* 1 = corrected, 0 = unchanged, -1 = ambiguous (unproven quote kind). */
static int ed_quote_try(const char *old_string, const char *new_string,
                        const char *content, char **ho, char **hn) {
    const char *oq1 = "\\'", *dq1 = "\\\"";
    int suspect, spurious, unproven;
    json_t *arr, *stripped, *stripped_new;
    const char *so, *sn;
    static const char kinds[2] = {'\'', '"'};
    char nb[3];
    size_t k;

    suspect = (ed_contains(old_string, oq1) && ed_contains(new_string, oq1)) ||
              (ed_contains(old_string, dq1) && ed_contains(new_string, dq1));
    if (!suspect || ed_contains(content, old_string)) return 0;

    arr = json_array();
    json_append(arr, json_string(ed_safe(old_string)));
    json_append(arr, json_string(ed_safe(new_string)));
    spurious = ed_quotes_all_spurious(arr);
    json_free(arr);
    if (!spurious) return 0;

    stripped = w26_escapedrif_a472f8_strip_quotes(old_string);
    so = stripped && stripped->type == JSON_STRING && stripped->str_val ? stripped->str_val : "";
    if (!ed_contains(content, so)) { json_free(stripped); return 0; }

    unproven = 0;
    for (k = 0; k < 2; k++) {
        nb[0] = '\\';
        nb[1] = kinds[k];
        nb[2] = '\0';
        if (ed_contains(new_string, nb) && !ed_contains(old_string, nb)) unproven = 1;
    }
    if (unproven) { json_free(stripped); return -1; }

    stripped_new = w26_escapedrif_a472f8_strip_quotes(new_string);
    sn = stripped_new && stripped_new->type == JSON_STRING && stripped_new->str_val
             ? stripped_new->str_val : "";
    *ho = ed_strdup(so);
    *hn = ed_strdup(sn);
    json_free(stripped);
    json_free(stripped_new);
    if (!*ho || !*hn) { free(*ho); free(*hn); return 0; }
    return 1;
}

json_t *w26_escapedrif_a472f8_quote_correct(const char *old_string,
                                           const char *new_string,
                                           const char *content) {
    char *ho = NULL, *hn = NULL;
    json_t *r;
    int rc = ed_quote_try(old_string, new_string, content, &ho, &hn);
    if (rc < 0) return ed_fail();
    if (rc == 0) return ed_pair(old_string, new_string);
    r = ed_pair(ho, hn);
    free(ho);
    free(hn);
    return r;
}

/* ---- maybe_correct_escape_drift ---------------------------------------- */

json_t *w26_escapedrif_a472f8_escape_drift(const char *old_string,
                                          const char *new_string,
                                          const char *content) {
    char *ho = NULL, *hn = NULL;
    json_t *r;
    int rc = ed_quote_try(old_string, new_string, content, &ho, &hn);
    if (rc < 0) {
        r = ed_fail();
        json_set(r, "note", json_null());
        return r;
    }
    if (rc == 1 && strcmp(ed_safe(ho), ed_safe(old_string)) != 0) {
        r = ed_pair(ho, hn);
        json_set(r, "note", json_string(ED_NOTE_QUOTE));
        free(ho);
        free(hn);
        return r;
    }
    free(ho);
    free(hn);
    ho = hn = NULL;

    rc = ed_backslash_try(old_string, new_string, content, &ho, &hn);
    if (rc < 0) {
        r = ed_fail();
        json_set(r, "note", json_null());
        return r;
    }
    if (rc == 1 && strcmp(ed_safe(ho), ed_safe(old_string)) != 0) {
        r = ed_pair(ho, hn);
        json_set(r, "note", json_string(ED_NOTE_BACKSLASH));
        free(ho);
        free(hn);
        return r;
    }
    free(ho);
    free(hn);
    r = ed_pair(old_string, new_string);
    json_set(r, "note", json_null());
    return r;
}

/* ---- notes -------------------------------------------------------------- */

json_t *w26_escapedrif_a472f8_notes(void) {
    json_t *o = json_object();
    json_set(o, "quote", json_string(ED_NOTE_QUOTE));
    json_set(o, "backslash", json_string(ED_NOTE_BACKSLASH));
    json_set(o, "ambiguous", json_string(W26_ESCAPEDRIF_A472F8_ERROR));
    return o;
}
