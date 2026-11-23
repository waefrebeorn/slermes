/*
 * cli_tui_wrap.c - Row-wrapping helpers for the classic CLI panels (C11 port).
 *
 * PoP: clituiwrap_wrap_rows @ hermes_cli/cli_tui_wrap.py:_wrap_rows
 * PoP: clituiwrap_prefix_wrapped_rows @ hermes_cli/cli_tui_wrap.py:_prefix_wrapped_rows
 *
 * The live helpers take a ``wrap`` callable.  The gate pins it to the standard
 * ``textwrap.wrap`` (whitespace split, greedy packing, long-word splitting,
 * subsequent_indent honoured against the width budget), so the C port performs
 * that wrapping directly.  The row-index pairing and the prefix-after-wrap
 * ordering are transcribed exactly from the live module.
 */
#include "noc_clituiwrap.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *p; size_t n, cap; } Buf;

static void bput(Buf *b, const char *s, size_t n) {
    if (!n) return;
    if (b->n + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->n + n + 1) nc *= 2;
        char *q = (char *)realloc(b->p, nc);
        if (!q) return;
        b->p = q; b->cap = nc;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n; b->p[b->n] = '\0';
}
static void bputs(Buf *b, const char *s) { if (s) bput(b, s, strlen(s)); }

static void cw_emit_line(json_t *out, const char *content, const char *indent, int first) {
    Buf b = {0};
    if (!first && indent && *indent) bputs(&b, indent);
    bputs(&b, content);
    json_append(out, json_string(b.p ? b.p : ""));
    free(b.p);
}

/* textwrap.wrap(text, width=width, subsequent_indent=indent) for the
 * whitespace-separated / ASCII case used by the CLI panels. */
static json_t *cw_textwrap(const char *text, int width, const char *indent) {
    json_t *out = json_array();
    if (!text || !*text) return out;
    if (width < 1) width = 1;

    size_t cap = 8, nw = 0;
    char **words = (char **)malloc(cap * sizeof(char *));
    const char *p = text;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *st = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        size_t l = (size_t)(p - st);
        if (nw >= cap) { cap *= 2; words = (char **)realloc(words, cap * sizeof(char *)); }
        char *w = (char *)malloc(l + 1);
        memcpy(w, st, l); w[l] = '\0';
        words[nw++] = w;
    }
    if (nw == 0) { free(words); return out; }

    size_t ilen = indent ? strlen(indent) : 0;
    int first = 1;
    char *line = NULL; size_t llen = 0;

    for (size_t i = 0; i < nw; i++) {
        char *w = words[i];
        size_t wl = strlen(w);
        size_t avail = (size_t)width - (first ? 0 : ilen);
        if (avail == 0) avail = 1;

        if (llen == 0) {
            if (wl <= avail) {
                free(line);
                line = (char *)malloc(wl + 1);
                memcpy(line, w, wl + 1); llen = wl;
            } else {
                size_t pos = 0;
                while (pos < wl) {
                    size_t take = avail;
                    if (pos + take > wl) take = wl - pos;
                    char chunk[8192];
                    if (take >= sizeof(chunk)) take = sizeof(chunk) - 1;
                    memcpy(chunk, w + pos, take); chunk[take] = '\0';
                    cw_emit_line(out, chunk, indent, first);
                    first = 0; pos += take;
                }
                llen = 0;
            }
        } else if (llen + 1 + wl <= avail) {
            char *q = (char *)realloc(line, llen + wl + 2);
            if (q) { line = q; line[llen] = ' '; memcpy(line + llen + 1, w, wl + 1); llen += wl + 1; }
        } else {
            cw_emit_line(out, line ? line : "", indent, first);
            first = 0;
            free(line); line = NULL; llen = 0;
            if (wl <= avail) {
                line = (char *)malloc(wl + 1);
                memcpy(line, w, wl + 1); llen = wl;
            } else {
                size_t pos = 0;
                while (pos < wl) {
                    size_t take = avail;
                    if (pos + take > wl) take = wl - pos;
                    char chunk[8192];
                    if (take >= sizeof(chunk)) take = sizeof(chunk) - 1;
                    memcpy(chunk, w + pos, take); chunk[take] = '\0';
                    cw_emit_line(out, chunk, indent, first);
                    first = 0; pos += take;
                }
            }
        }
    }
    if (llen > 0) cw_emit_line(out, line ? line : "", indent, first);

    free(line);
    for (size_t i = 0; i < nw; i++) free(words[i]);
    free(words);
    return out;
}

json_t *clituiwrap_wrap_rows(const json_t *items, int width, const char *indent) {
    json_t *out = json_array();
    if (!items || items->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(items); i++) {
        json_t *label = json_get(items, i);
        const char *s = (label && label->type == JSON_STRING && label->str_val) ? label->str_val : "";
        json_t *rows = cw_textwrap(s, width, indent);
        for (size_t j = 0; j < json_len(rows); j++) {
            json_t *pair = json_array();
            json_append(pair, json_number((double)i));
            json_append(pair, json_copy(json_get(rows, j)));
            json_append(out, pair);
        }
        json_free(rows);
    }
    return out;
}

json_t *clituiwrap_prefix_wrapped_rows(const char *label, int width,
                                       const char *first_prefix, const char *indent) {
    /* Upstream calls wrap(label, width) with NO subsequent_indent here. */
    json_t *rows = cw_textwrap(label ? label : "", width, "");
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(rows); i++) {
        json_t *cell = json_get(rows, i);
        const char *row = (cell && cell->type == JSON_STRING && cell->str_val) ? cell->str_val : "";
        Buf b = {0};
        bputs(&b, i == 0 ? (first_prefix ? first_prefix : "") : (indent ? indent : ""));
        bputs(&b, row);
        json_append(out, json_string(b.p ? b.p : ""));
        free(b.p);
    }
    json_free(rows);
    return out;
}
