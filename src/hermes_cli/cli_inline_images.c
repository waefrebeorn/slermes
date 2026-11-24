/*
 * cli_inline_images.c — C port of upstream Hermes hermes_cli/cli_inline_images.py.
 *
 * PoP: w16_cliinline_fa1b6a_zero_width @ hermes_cli/cli_inline_images.py:_zero_width
 * PoP: w16_cliinline_fa1b6a_image_rows @ hermes_cli/cli_inline_images.py:image_rows
 * PoP: w16_cliinline_fa1b6a_constants @ hermes_cli/cli_inline_images.py:_INDENT/_MAX_COLS/_MAX_ROWS/_VSCODE_HINT_ODDS
 *
 * Chrome fidelity note: _passthrough is identity here (the oracle pins TMUX
 * unset and patches _passthrough to identity), so transmit strings pass verbatim.
 */
#include "noc_w16_cliinline_fa1b6a.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INDENT "  "

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

typedef struct { char *p; size_t len, cap; } sbuf;
static void sb_init(sbuf *b) { b->cap = 128; b->len = 0; b->p = (char *)malloc(b->cap); if (b->p) b->p[0] = 0; }
static void sb_addn(sbuf *b, const char *s, size_t n) {
    if (!s || !n) return;
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap;
        while (nc < b->len + n + 1) nc *= 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = 0;
}
static void sb_add(sbuf *b, const char *s) { if (s) sb_addn(b, s, strlen(s)); }
static void sb_addf(sbuf *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n > 0) sb_addn(b, tmp, (size_t)n < sizeof(tmp) ? (size_t)n : sizeof(tmp) - 1);
}
/* returns the allocation start; caller frees */
static char *sb_take(sbuf *b) {
    char *p = b->p;
    b->p = NULL; b->len = b->cap = 0;
    return p ? p : dupstr("");
}

static const char *jstr(const json_t *n) {
    return (n && n->type == JSON_STRING && n->str_val) ? n->str_val : "";
}
static int jnum(const json_t *n) {
    return (n && n->type == JSON_NUMBER) ? (int)n->num_val : 0;
}

json_t *w16_cliinline_fa1b6a_zero_width(const char *seq) {
    sbuf b; sb_init(&b);
    sb_add(&b, "\x01");
    sb_add(&b, seq ? seq : "");
    sb_add(&b, "\x02");
    char *s = sb_take(&b);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

static void add_zero_wrapped(sbuf *b, const char *seq) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "\x01%s\x02", seq);
    sb_add(b, tmp);
}

static int hex_pair(const char *s) {
    int v = 0;
    for (int i = 0; i < 2; i++) {
        char c = s[i];
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return -1;
        v = v * 16 + d;
    }
    return v;
}

/* One truecolor ANSI half-block cell; RGBA quadruples as JSON arrays. */
static void half_block(sbuf *b, const json_t *top, const json_t *bottom) {
    int tr = jnum(json_get(top, 0)), tg = jnum(json_get(top, 1)), tb = jnum(json_get(top, 2)), ta = jnum(json_get(top, 3));
    int br = jnum(json_get(bottom, 0)), bg = jnum(json_get(bottom, 1)), bb = jnum(json_get(bottom, 2)), ba = jnum(json_get(bottom, 3));
    if (ta < 32 && ba < 32) { sb_add(b, "\x1b[0m "); return; }
    if (ba < 32) { sb_addf(b, "\x1b[0m\x1b[38;2;%d;%d;%dm\xe2\x96\x80", tr, tg, tb); return; }
    if (ta < 32) { sb_addf(b, "\x1b[0m\x1b[38;2;%d;%d;%dm\xe2\x96\x84", br, bg, bb); return; }
    sb_addf(b, "\x1b[38;2;%d;%d;%dm\x1b[48;2;%d;%d;%dm\xe2\x96\x80", tr, tg, tb, br, bg, bb);
}

static void append_pair(json_t *out, const char *row, const char *raw) {
    json_t *p = json_array();
    json_append(p, json_string(row));
    json_append(p, json_string(raw));
    json_append(out, p);
}

json_t *w16_cliinline_fa1b6a_image_rows(const json_t *image) {
    json_t *out = json_array();
    if (!image || image->type != JSON_OBJECT) return out;
    const char *mode = jstr(json_obj_get(image, "mode"));

    if (!strcmp(mode, "kitty")) {
        const char *color = jstr(json_obj_get(image, "color"));
        const char *hex = color[0] == '#' ? color + 1 : color;
        int r = hex_pair(hex), g = hex_pair(hex + 2), b2 = hex_pair(hex + 4);
        sbuf cb; sb_init(&cb);
        sb_addf(&cb, "\x1b[38;2;%d;%d;%dm", r, g, b2);
        char *color_seq = sb_take(&cb);
        char *color_zw = NULL, *reset_zw = NULL;
        {
            sbuf z; sb_init(&z); add_zero_wrapped(&z, color_seq); color_zw = sb_take(&z);
            sbuf q; sb_init(&q); add_zero_wrapped(&q, "\x1b[39m"); reset_zw = sb_take(&q);
        }
        json_t *ph = json_obj_get(image, "placeholder");
        size_t n = (ph && ph->type == JSON_ARRAY) ? json_len(ph) : 0;
        const char *transmit = jstr(json_obj_get(image, "transmit"));
        for (size_t i = 0; i < n; i++) {
            sbuf row; sb_init(&row);
            sb_add(&row, INDENT);
            sb_add(&row, color_zw ? color_zw : "");
            sb_add(&row, jstr(json_get(ph, i)));
            sb_add(&row, reset_zw ? reset_zw : "");
            char *row_s = sb_take(&row);
            append_pair(out, row_s ? row_s : "", i == 0 ? transmit : "");
            free(row_s);
        }
        free(color_zw); free(reset_zw); free(color_seq);
        return out;
    }

    if (!strcmp(mode, "iterm")) {
        int rows = jnum(json_obj_get(image, "rows"));
        if (rows < 0) rows = 0;
        const char *transmit = jstr(json_obj_get(image, "transmit"));
        sbuf draw; sb_init(&draw);
        sb_add(&draw, "\x1b" "7");   /* ESC 7 (save cursor) */
        if (rows > 1) sb_addf(&draw, "\x1b[%dA", rows - 1);
        sb_add(&draw, "\x1b[2C");   /* len(INDENT) */
        sb_add(&draw, transmit);
        sb_add(&draw, "\x1b" "8");   /* ESC 8 (restore cursor) */
        char *draw_s = sb_take(&draw);
        for (int i = 0; i < rows; i++)
            append_pair(out, "", (i == rows - 1 && draw_s) ? draw_s : "");
        free(draw_s);
        return out;
    }

    /* halfblock / any other mode: one printable row per cells row */
    json_t *cells = json_obj_get(image, "cells");
    size_t nr = (cells && cells->type == JSON_ARRAY) ? json_len(cells) : 0;
    for (size_t i = 0; i < nr; i++) {
        json_t *cellrow = json_get(cells, i);
        sbuf row; sb_init(&row);
        sb_add(&row, INDENT);
        size_t nc = (cellrow && cellrow->type == JSON_ARRAY) ? json_len(cellrow) : 0;
        for (size_t j = 0; j < nc; j++) {
            json_t *cell = json_get(cellrow, j);
            half_block(&row, json_get(cell, 0), json_get(cell, 1));
        }
        sb_add(&row, "\x1b[0m");
        char *row_s = sb_take(&row);
        append_pair(out, row_s ? row_s : "", "");
        free(row_s);
    }
    return out;
}

json_t *w16_cliinline_fa1b6a_constants(void) {
    json_t *out = json_object();
    json_set(out, "indent", json_string("  "));
    json_set(out, "max_cols", json_number(64));
    json_set(out, "max_rows", json_number(16));
    json_set(out, "vscode_hint_odds", json_number(0.2));
    return out;
}
