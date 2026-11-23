/*
 * cli_panel.c — C port of upstream Hermes hermes_cli/cli_panel.py.
 * PoP: clipanel_row @ hermes_cli/cli_panel.py:_Panel.row
 * PoP: clipanel_blank @ hermes_cli/cli_panel.py:_Panel.blank
 * PoP: clipanel_close @ hermes_cli/cli_panel.py:_Panel.close
 * PoP: clipanel_init @ hermes_cli/cli_panel.py:_Panel.__init__
 * PoP: clipanel_cells @ hermes_cli/cli_render.py:_cells
 *
 * The three lazy helpers from hermes_cli.cli_render are inlined:
 *   _cells (prompt_toolkit get_cwidth), _append_panel_line, _append_blank_panel_line.
 */
#include "noc_clipanel.h"

#include <stdlib.h>
#include <string.h>

/* ---- UTF-8 decode + terminal-cell width (prompt_toolkit get_cwidth) ---- */
static unsigned u8_next(const char *s, size_t *i) {
    size_t n = strlen(s);
    if (*i >= n) return 0;
    unsigned char c = (unsigned char)s[(*i)++];
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0 && *i < n)
        return ((unsigned)(c & 0x1F) << 6) | ((unsigned char)s[(*i)++] & 0x3F);
    if ((c & 0xF0) == 0xE0 && *i + 1 < n) {
        unsigned v = ((unsigned)(c & 0x0F) << 12)
                   | ((unsigned)((unsigned char)s[(*i)++] & 0x3F) << 6);
        return v | ((unsigned char)s[(*i)++] & 0x3F);
    }
    if ((c & 0xF8) == 0xF0 && *i + 2 < n) {
        unsigned v = ((unsigned)(c & 0x07) << 18)
                   | ((unsigned)((unsigned char)s[(*i)++] & 0x3F) << 12)
                   | ((unsigned)((unsigned char)s[(*i)++] & 0x3F) << 6);
        return v | ((unsigned char)s[(*i)++] & 0x3F);
    }
    return c;
}
static int cp_width(unsigned c) {
    if (c < 0x20 || (c >= 0x7f && c < 0xa0)) return 0; /* controls (not used by fixtures) */
    if ((c >= 0x0300 && c <= 0x036F) || (c >= 0x1AB0 && c <= 0x1AFF) ||
        (c >= 0x1DC0 && c <= 0x1DFF) || (c >= 0x20D0 && c <= 0x20FF) ||
        (c >= 0xFE20 && c <= 0xFE2F)) return 0; /* combining */
    if ((c >= 0x1100 && c <= 0x115F) || c == 0x2329 || c == 0x232A ||
        (c >= 0x2E80 && c <= 0xA4CF) || (c >= 0xAC00 && c <= 0xD7A3) ||
        (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFE10 && c <= 0xFE19) ||
        (c >= 0xFE30 && c <= 0xFE6F) || (c >= 0xFF00 && c <= 0xFF60) ||
        (c >= 0xFFE0 && c <= 0xFFE6) || (c >= 0x1F300 && c <= 0x1FAFF) ||
        (c >= 0x20000 && c <= 0x3FFFD)) return 2; /* CJK / emoji wide */
    return 1;
}
int clipanel_cells(const char *text) {
    if (!text) return 0;
    int total = 0;
    size_t i = 0;
    while (i < strlen(text)) total += cp_width(u8_next(text, &i));
    return total;
}

/* ---- fragment accumulation ---- */
static void append_line(clipanel_panel_t *p, const char *style, const char *text) {
    if (p->count == p->cap) {
        size_t cap = p->cap ? p->cap * 2 : 8;
        clipanel_line_t *l = (clipanel_line_t *)realloc(p->lines, cap * sizeof(*l));
        if (!l) return;
        p->lines = l;
        p->cap = cap;
    }
    p->lines[p->count].style = strdup(style ? style : "");
    p->lines[p->count].text = strdup(text ? text : "");
    p->count++;
}
static char *repeat_utf8(const char *s, int n) {
    if (n < 0) n = 0; /* Python "str" * negative == "" */
    size_t one = strlen(s ? s : "");
    char *out = (char *)malloc((size_t)n * one + 1);
    size_t k = 0;
    for (int i = 0; i < n; i++) { memcpy(out + k, s, one); k += one; }
    out[k] = 0;
    return out;
}
static char *spaces(int n) {
    if (n < 0) n = 0;
    char *out = (char *)malloc((size_t)n + 1);
    memset(out, ' ', (size_t)n);
    out[n] = 0;
    return out;
}
static char *cat3(const char *a, const char *b, const char *c) {
    size_t la = strlen(a), lb = strlen(b), lc = strlen(c);
    char *out = (char *)malloc(la + lb + lc + 1);
    memcpy(out, a, la);
    memcpy(out + la, b, lb);
    memcpy(out + la + lb, c, lc);
    out[la + lb + lc] = 0;
    return out;
}

void clipanel_init(clipanel_panel_t *p, const char *border, int width,
                   const char *title, const char *title_style) {
    memset(p, 0, sizeof(*p));
    p->border = strdup(border ? border : "");
    p->width = width;
    if (title && *title) {
        append_line(p, p->border, "╭─ ");
        append_line(p, title_style ? title_style : "", title);
        int n = width - clipanel_cells(title) - 3;
        char *rule = repeat_utf8("─", n);
        char *top = cat3(" ", rule, "╮\n");
        append_line(p, p->border, top);
        free(top);
        free(rule);
    } else {
        char *rule = repeat_utf8("─", width);
        char *top = cat3("╭", rule, "╮\n");
        append_line(p, p->border, top);
        free(top);
        free(rule);
    }
}
void clipanel_row(clipanel_panel_t *p, const char *style, const char *text) {
    const char *t = text ? text : "";
    int pad = p->width - 2 - clipanel_cells(t);
    char *sp = spaces(pad);
    char *body = cat3(t, sp, "");
    append_line(p, p->border, "│ ");
    append_line(p, style ? style : "", body);
    append_line(p, p->border, " │\n");
    free(body);
    free(sp);
}
void clipanel_blank(clipanel_panel_t *p) {
    char *sp = spaces(p->width);
    char *line = cat3("│", sp, "│\n");
    append_line(p, p->border, line);
    free(line);
    free(sp);
}
void clipanel_close(clipanel_panel_t *p) {
    char *rule = repeat_utf8("─", p->width);
    char *bottom = cat3("╰", rule, "╯\n");
    append_line(p, p->border, bottom);
    free(bottom);
    free(rule);
}
void clipanel_free(clipanel_panel_t *p) {
    for (size_t i = 0; i < p->count; i++) {
        free(p->lines[i].style);
        free(p->lines[i].text);
    }
    free(p->lines);
    free(p->border);
    memset(p, 0, sizeof(*p));
}
