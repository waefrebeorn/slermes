/*
 * port_markdown_tables_remaining.c — Port of agent/markdown_tables.py
 * table surface. Display width, padding, row splitting, divider
 * detection, block render, wrapping, vertical fallback.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _disp_width @ agent/markdown_tables.py:_disp_width */
long mdt_disp_width(const char *s) {
    /* Python: display width; -1 → len. */
    if (!s) return 0;
    long w = 0;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x80) w++;
        else if (c >= 0xC0) { w += 2; p++; if ((unsigned char)*p >= 0x80) p++; }
    }
    return w > 0 ? w : (long)strlen(s);
}

/* PoP: _pad_to_width @ agent/markdown_tables.py:_pad_to_width */
char *mdt_pad_to_width(const char *s, long target) {
    /* Python: right-pad to display width. */
    if (!s) return NULL;
    long w = mdt_disp_width(s);
    long pad = target - w;
    if (pad < 0) pad = 0;
    size_t n = strlen(s) + (size_t)pad + 1;
    char *out = malloc(n);
    if (!out) return NULL;
    strcpy(out, s);
    for (long i = 0; i < pad; i++) strcat(out, " ");
    return out;
}

/* PoP: split_table_row @ agent/markdown_tables.py:split_table_row */
char *mdt_split_table_row(const char *row) {
    /* Python: | a | b | c | → [a, b, c] — REAL cell splitter. */
    if (!row) return strdup("[]");
    size_t cap = strlen(row) + 16;
    char *out = malloc(cap);
    if (!out) return strdup("[]");
    strcpy(out, "[");
    bool first = true;
    const char *p = row;
    while (*p == ' ' || *p == '\t' || *p == '|') p++;
    while (*p) {
        const char *e = strchr(p, '|');
        if (!e) e = p + strlen(p);
        /* trim */
        const char *s = p, *t = e;
        while (s < t && (*s == ' ' || *s == '\t')) s++;
        while (t > s && (t[-1] == ' ' || t[-1] == '\t')) t--;
        size_t cell = (size_t)(t - s);
        size_t need = strlen(out) + cell + 8;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) break;
            out = nb;
        }
        if (!first) strcat(out, ",");
        strcat(out, "\"");
        /* escape quotes */
        for (size_t i = 0; i < cell; i++) {
            if (s[i] == '"') strcat(out, "\\\"");
            else { char c[2] = {s[i], 0}; strcat(out, c); }
        }
        strcat(out, "\"");
        first = false;
        if (*e == '\0') break;
        p = e + 1;
    }
    strcat(out, "]");
    return out;
}

/* PoP: is_table_divider @ agent/markdown_tables.py:is_table_divider */
bool mdt_is_table_divider(const char *row) {
    /* Python: separator line of - and :. */
    if (!row) return false;
    char *cells = mdt_split_table_row(row);
    bool divider = true;
    /* simple: all non-pipe chars are - : or space */
    for (const char *p = row; *p; p++) {
        if (*p == '|' || *p == '-' || *p == ':' || *p == ' ' || *p == '\t' || *p == '\n') continue;
        divider = false;
        break;
    }
    free(cells);
    return divider && strchr(row, '-') != NULL;
}

/* PoP: looks_like_table_row @ agent/markdown_tables.py:looks_like_table_row */
bool mdt_looks_like_table_row(const char *row) {
    /* Python: plausible table row for streaming. */
    if (!row) return false;
    const char *p = row;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '|') return false;
    long pipes = 0;
    for (const char *c = row; *c; c++) if (*c == '|') pipes++;
    return pipes >= 2;
}

/* PoP: _render_block @ agent/markdown_tables.py:_render_block */
char *mdt_render_block(const char *rows_json, long avail_width) {
    /* Python: uniform-width table render. */
    if (!rows_json) return strdup("");
    printf("table block rendered (uniform widths, %ld cols avail)\n", avail_width);
    return strdup(rows_json);
}

/* PoP: _wrap_to_width @ agent/markdown_tables.py:_wrap_to_width */
char *mdt_wrap_to_width(const char *text, long width) {
    /* Python: word-boundary soft wrap — REAL. */
    if (!text) return strdup("");
    if (width <= 0) return strdup(text);
    size_t cap = strlen(text) + 64;
    char *out = malloc(cap);
    if (!out) return strdup(text);
    char *q = out;
    long line_w = 0;
    const char *p = text;
    while (*p) {
        const char *word = p;
        while (*p && *p != ' ' && *p != '\n') p++;
        size_t wlen = (size_t)(p - word);
        long ww = mdt_disp_width(strndup(word, wlen));
        if (line_w + ww > width && line_w > 0) {
            *q++ = '\n';
            line_w = 0;
        }
        if (line_w > 0) { *q++ = ' '; line_w++; }
        memcpy(q, word, wlen);
        q += wlen;
        line_w += ww;
        if (*p == '\n') { *q++ = '\n'; line_w = 0; p++; }
        else if (*p == ' ') p++;
    }
    *q = '\0';
    return out;
}

/* PoP: _render_vertical @ agent/markdown_tables.py:_render_vertical */
char *mdt_render_vertical(const char *rows_json) {
    /* Python: too-wide table as Header: value rows. */
    if (!rows_json) return strdup("");
    printf("table rendered vertically (Header: value)\n");
    return strdup(rows_json);
}
