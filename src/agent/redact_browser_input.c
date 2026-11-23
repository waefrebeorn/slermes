/*
 * redact_browser_input.c — C port of upstream Hermes agent/redact_browser_input.py.
 *
 * PoP: w13_redactbrow_551284_literal_end @ agent/redact_browser_input.py:_literal_end
 * PoP: w13_redactbrow_551284_skip_comment @ agent/redact_browser_input.py:_skip_comment
 * PoP: w13_redactbrow_551284_literal_mask @ agent/redact_browser_input.py:_literal_mask
 * PoP: w13_redactbrow_551284_call_end @ agent/redact_browser_input.py:_call_end
 * PoP: w13_redactbrow_551284_literal_regions @ agent/redact_browser_input.py:_literal_regions
 * PoP: w13_redactbrow_551284_unparsed_masks @ agent/redact_browser_input.py:_unparsed_masks
 * PoP: w13_redactbrow_551284_mask_unparsed @ agent/redact_browser_input.py:mask_typed_browser_text
 *
 * The Python AST-parsed path cannot be mirrored in C; this gate drives the
 * fail-closed, raw-text branch the module uses when ast.parse raises (the
 * oracle forces that branch with a parse seam). All scanning below reproduces
 * the module's fixed regexes byte-for-byte.
 */
#include "noc_w13_redactbrow_551284.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REPL_CAP 96
#define MASK_CAP 1024

struct lit { int ok; char prefix[8]; char quote[8]; int end; };
struct mask { int start, end; char repl[REPL_CAP]; };
struct masks { struct mask *v; size_t n, cap; };

static int is_pref(char c) {
    return c=='f'||c=='F'||c=='r'||c=='R'||c=='b'||c=='B'||c=='u'||c=='U';
}
static int is_quote_lead(char c) { return c=='"'||c=='\''||c=='`'||c=='\\'; }

/* _QUOTES in the module's exact priority order. */
static const char *const QUOTES[] = {"'''", "\"\"\"", "\\'", "\\\"", "\\`", "'", "\"", "`"};

static int starts_at(const char *code, int i, const char *q) {
    size_t l = strlen(q);
    int n = (int)strlen(code);
    if (i < 0 || (size_t)(n - i) < l) return 0;
    return memcmp(code + i, q, l) == 0;
}

static struct lit literal_end(const char *code, int start) {
    struct lit L;
    memset(&L, 0, sizeof(L));
    int n = (int)strlen(code);
    int ps = 0;
    if (start < n && is_pref(code[start])) {
        if (start + 2 < n && is_pref(code[start + 1]) && is_quote_lead(code[start + 2])) ps = 2;
        else if (start + 1 < n && is_quote_lead(code[start + 1])) ps = 1;
    }
    memcpy(L.prefix, code + start, (size_t)ps);
    L.prefix[ps] = '\0';
    int qs = start + ps;
    const char *quote = NULL;
    for (size_t q = 0; q < sizeof(QUOTES) / sizeof(QUOTES[0]); q++)
        if (starts_at(code, qs, QUOTES[q])) { quote = QUOTES[q]; break; }
    if (!quote) return L;  /* ok = 0 */
    int qlen = (int)strlen(quote);
    int i = qs + qlen;
    int multiline = (qlen == 3);
    while (i < n) {
        if (starts_at(code, i, quote)) {
            L.ok = 1; snprintf(L.quote, sizeof(L.quote), "%s", quote);
            L.end = i + qlen; return L;
        }
        if (!multiline && (code[i] == '\r' || code[i] == '\n')) break;
        i += (code[i] == '\\') ? 2 : 1;
    }
    L.ok = 1; L.quote[0] = '\0'; L.end = i;  /* unterminated: empty quote */
    return L;
}

static int skip_comment(const char *code, int i) {
    int n = (int)strlen(code);
    if (i >= n || code[i] != '#') return i;
    i++;
    while (i < n && code[i] != '\r' && code[i] != '\n') i++;
    return i;
}

static void masks_add(struct masks *m, int start, int end, const char *repl) {
    if (m->n == m->cap) {
        m->cap = m->cap ? m->cap * 2 : MASK_CAP;
        m->v = (struct mask *)realloc(m->v, m->cap * sizeof(struct mask));
    }
    m->v[m->n].start = start;
    m->v[m->n].end = end;
    snprintf(m->v[m->n].repl, REPL_CAP, "%s", repl);
    m->n++;
}

static int literal_mask(const char *code, int start, struct mask *out) {
    struct lit L = literal_end(code, start);
    if (!L.ok) return 0;
    char repl[REPL_CAP];
    snprintf(repl, sizeof(repl), "%s%s***%s", L.prefix, L.quote, L.quote);
    out->start = start; out->end = L.end;
    snprintf(out->repl, REPL_CAP, "%s", repl);
    return 1;
}

/* _call_end */
static int call_end(const char *code, int i) {
    int n = (int)strlen(code);
    int depth = 1;
    while (i < n) {
        if (is_quote_lead(code[i])) {
            struct lit L = literal_end(code, i);
            if (L.ok) {
                int ne = L.end > i + 1 ? L.end : i + 1;
                i = ne;
                continue;
            }
        }
        if (code[i] == '#') { i = skip_comment(code, i); continue; }
        if (code[i] == '(') depth++;
        else if (code[i] == ')') { depth--; if (depth == 0) return i; }
        i++;
    }
    return i;
}

/* _literal_regions */
static void literal_regions(const char *code, int start, int end, struct masks *out) {
    int i = start, comment_end = start;
    int n = (int)strlen(code);
    while (i < end && i < n) {
        if (code[i] == '#' && i >= comment_end) comment_end = skip_comment(code, i);
        if (is_quote_lead(code[i])) {
            struct lit L = literal_end(code, i);
            if (L.ok) {
                if (!L.quote[0] && i >= comment_end) {
                    char rep[REPL_CAP];
                    snprintf(rep, sizeof(rep), "%s***", L.prefix);
                    int e = L.end > end ? L.end : end;
                    masks_add(out, i, e, rep);
                    break;
                }
                char rep[REPL_CAP];
                snprintf(rep, sizeof(rep), "%s%s***%s", L.prefix, L.quote, L.quote);
                masks_add(out, i, L.end, rep);
                int ne = L.end > i + 1 ? L.end : i + 1;
                i = ne;
                continue;
            }
        }
        i++;
    }
}

/* _TYPING_CALL_RE: \b(?:fill_input|type_text)\s*\(|insertText  (end offsets) */
static void typing_call_ends(const char *code, struct masks *out) {
    int n = (int)strlen(code);
    const char *names[3] = {"fill_input", "type_text", "insertText"};
    for (int i = 0; i < n; i++) {
        int matched = 0, endp = 0;
        int prev_ok = (i == 0) || !(code[i-1] >= 'a' && code[i-1] <= 'z' ||
                                   code[i-1] >= 'A' && code[i-1] <= 'Z' ||
                                   code[i-1] >= '0' && code[i-1] <= '9' || code[i-1] == '_');
        if (prev_ok) {
            for (int k = 0; k < 2; k++) {
                size_t l = strlen(names[k]);
                if ((size_t)(n - i) >= l && memcmp(code + i, names[k], l) == 0) {
                    int j = i + (int)l;
                    while (j < n && (code[j]==' '||code[j]=='\t'||code[j]=='\n'||code[j]=='\r'||code[j]=='\f'||code[j]=='\v')) j++;
                    if (j < n && code[j] == '(') { matched = 1; endp = j + 1; }
                    break;
                }
            }
        }
        if (!matched && (size_t)(n - i) >= strlen(names[2]) &&
            memcmp(code + i, names[2], strlen(names[2])) == 0) {
            matched = 1; endp = i + (int)strlen(names[2]);
        }
        if (matched) {
            masks_add(out, endp, endp, "");  /* end stored in .start */
            i = endp - 1;
        }
    }
}

/* _VALUE_ASSIGN: \.value\s*=(?!=)\s*  (end offsets) */
static void value_assign_ends(const char *code, struct masks *out) {
    int n = (int)strlen(code);
    for (int i = 0; i + 6 <= n; i++) {
        if (memcmp(code + i, ".value", 6) != 0) continue;
        int j = i + 6;
        while (j < n && (code[j]==' '||code[j]=='\t'||code[j]=='\n'||code[j]=='\r'||code[j]=='\f'||code[j]=='\v')) j++;
        if (j >= n || code[j] != '=' || (j + 1 < n && code[j+1] == '=')) continue;
        j++;
        while (j < n && (code[j]==' '||code[j]=='\t'||code[j]=='\n'||code[j]=='\r'||code[j]=='\f'||code[j]=='\v')) j++;
        masks_add(out, j, j, "");
        i = j - 1;
    }
}

static json_t *masks_to_json(const struct masks *m) {
    json_t *out = json_array();
    for (size_t i = 0; i < m->n; i++) {
        json_t *a = json_array();
        json_append(a, json_number((double)m->v[i].start));
        json_append(a, json_number((double)m->v[i].end));
        json_append(a, json_string(m->v[i].repl));
        json_append(out, a);
    }
    return out;
}

/* ---------- public API ---------- */
struct json_t *w13_redactbrow_551284_literal_end(const char *code, int start) {
    struct lit L = literal_end(code ? code : "", start);
    if (!L.ok) return json_null();
    json_t *o = json_object();
    json_set(o, "prefix", json_string(L.prefix));
    json_set(o, "quote", json_string(L.quote));
    json_set(o, "end", json_number((double)L.end));
    return o;
}

int w13_redactbrow_551284_skip_comment(const char *code, int i) {
    return skip_comment(code ? code : "", i);
}

struct json_t *w13_redactbrow_551284_literal_mask(const char *code, int start) {
    struct mask m;
    if (!literal_mask(code ? code : "", start, &m)) return json_null();
    json_t *a = json_array();
    json_append(a, json_number((double)m.start));
    json_append(a, json_number((double)m.end));
    json_append(a, json_string(m.repl));
    return a;
}

int w13_redactbrow_551284_call_end(const char *code, int i) {
    return call_end(code ? code : "", i);
}

struct json_t *w13_redactbrow_551284_literal_regions(const char *code, int start, int end) {
    struct masks m = {0};
    literal_regions(code ? code : "", start, end, &m);
    json_t *o = masks_to_json(&m);
    free(m.v);
    return o;
}

static void unparsed_collect(const char *code, struct masks *out) {
    struct masks ends = {0};
    typing_call_ends(code, &ends);
    for (size_t i = 0; i < ends.n; i++) {
        int e = ends.v[i].start;
        literal_regions(code, e, call_end(code, e), out);
    }
    free(ends.v);
    struct masks va = {0};
    value_assign_ends(code, &va);
    for (size_t i = 0; i < va.n; i++) {
        struct mask lm;
        if (literal_mask(code, va.v[i].start, &lm)) masks_add(out, lm.start, lm.end, lm.repl);
    }
    free(va.v);
}

struct json_t *w13_redactbrow_551284_unparsed_masks(const char *code) {
    struct masks m = {0};
    unparsed_collect(code ? code : "", &m);
    json_t *o = masks_to_json(&m);
    free(m.v);
    return o;
}

struct json_t *w13_redactbrow_551284_mask_unparsed(const char *code) {
    if (!code) code = "";
    struct masks m = {0};
    unparsed_collect(code, &m);
    /* sorted by (start asc, end desc) */
    for (size_t i = 1; i < m.n; i++) {
        struct mask cur = m.v[i];
        size_t j = i;
        while (j > 0) {
            struct mask prev = m.v[j - 1];
            int cmp = (cur.start > prev.start) - (cur.start < prev.start);
            if (cmp == 0) cmp = (prev.end > cur.end) - (prev.end < cur.end);
            if (cmp >= 0) break;
            m.v[j] = prev; j--;
        }
        m.v[j] = cur;
    }
    size_t clen = strlen(code);
    size_t cap = clen + m.n * 8 + 16;
    char *out = (char *)malloc(cap);
    if (!out) { free(m.v); return json_string(""); }
    size_t pos = 0, o = 0;
    for (size_t i = 0; i < m.n; i++) {
        int s = m.v[i].start, e = m.v[i].end;
        if (s < 0 || e < s || (size_t)s < pos || (size_t)e > clen) continue;
        size_t seg = (size_t)s - pos;
        memcpy(out + o, code + pos, seg); o += seg;
        size_t rl = strlen(m.v[i].repl);
        memcpy(out + o, m.v[i].repl, rl); o += rl;
        pos = (size_t)e;
    }
    memcpy(out + o, code + pos, clen - pos); o += clen - pos;
    out[o] = '\0';
    json_t *r = json_string(out);
    free(out);
    free(m.v);
    return r;
}
