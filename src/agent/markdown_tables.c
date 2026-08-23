/* CJK/wide-character-aware re-alignment of model-emitted markdown tables.
 *
 * Python equivalent: agent/markdown_tables.py
 *
 * Models pad markdown tables assuming each character occupies one terminal
 * cell. CJK glyphs and most emoji render as two cells, so the model's
 * spacing collapses into drift the moment a table reaches a real terminal.
 *
 * This module rebuilds row padding using wcwidth (display columns),
 * preserving the table's pipes and dashes.
 */

#define _GNU_SOURCE
#include "hermes_markdown_tables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#define MIN_COL_WIDTH 3

/* ------------------------------------------------------------------ */
/*  Display width helpers                                             */
/* Port of Python agent/markdown_tables.py:_disp_width(). */
/* ------------------------------------------------------------------ */

int disp_width(const char *s)
{
    if (!s || !*s)
        return 0;

    int total = 0;
    mbstate_t ps;
    memset(&ps, 0, sizeof(ps));

    size_t len = strlen(s);
    const char *p = s;
    while (len > 0) {
        wchar_t wc;
        size_t ret = mbrtowc(&wc, p, len, &ps);
        if (ret == (size_t)-1 || ret == (size_t)-2) {
            /* Bad byte sequence — skip it */
            p++;
            len--;
            total += 1; /* assume one cell */
        } else if (ret == 0) {
            break;
        } else {
            int w = wcwidth(wc);
            if (w > 0)
                total += w;
            /* else: w <= 0 means zero-width (combining/control) */
            p += ret;
            len -= ret;
        }
    }
    return total;
}

/* Port of Python agent/markdown_tables.py:_pad_to_width(). */
static char *pad_to_width(const char *s, int target)
{
    int current = disp_width(s);
    int needed = target - current;
    if (needed <= 0)
        return strdup(s);

    size_t slen = strlen(s);
    char *result = malloc(slen + needed + 1);
    if (!result)
        return NULL;
    memcpy(result, s, slen);
    memset(result + slen, ' ', needed);
    result[slen + needed] = '\0';
    return result;
}

/* ------------------------------------------------------------------ */
/*  Row splitting                                                      */
/* ------------------------------------------------------------------ */

/* Port of Python agent/markdown_tables.py:split_table_row(). */
char **split_table_row(const char *row, size_t *out_len)
{
    *out_len = 0;
    if (!row || !*row)
        return NULL;

    const char *s = row;
    /* Skip leading whitespace only */
    while (*s == ' ' || *s == '\t') s++;
    /* Skip leading pipe */
    if (*s == '|') s++;

    size_t len = strlen(s);

    /* Count pipes */
    size_t pipe_count = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '|') pipe_count++;
    }

    /* Allocate up to pipe_count+1 cells */
    char **cells = calloc(pipe_count + 2, sizeof(char *));
    if (!cells) return NULL;

    size_t cell_count = 0;
    const char *start = s;
    size_t remaining = len;
    while (remaining > 0) {
        /* Find next pipe or end */
        const char *pipe = memchr(start, '|', remaining);
        size_t cell_len;
        if (pipe) {
            cell_len = pipe - start;
            remaining -= cell_len + 1;
        } else {
            cell_len = remaining;
            remaining = 0;
        }

        char *cell = malloc(cell_len + 1);
        if (!cell) goto cleanup;
        memcpy(cell, start, cell_len);
        cell[cell_len] = '\0';

        /* Advance start past this segment */
        if (pipe) {
            start = pipe + 1;
        }

        /* Trim trailing whitespace */
        while (cell_len > 0 && (cell[cell_len - 1] == ' ' || cell[cell_len - 1] == '\t'))
            cell[--cell_len] = '\0';
        /* Trim leading whitespace */
        char *trimmed = cell;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (trimmed != cell) {
            char *stripped = strdup(trimmed);
            free(cell);
            if (!stripped) goto cleanup;
            cell = stripped;
        }

        cells[cell_count++] = cell;
    }

    cells[cell_count] = NULL;
    *out_len = cell_count;
    return cells;

cleanup:
    for (size_t i = 0; i < cell_count; i++)
        free(cells[i]);
    free(cells);
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Table detection                                                    */
/* ------------------------------------------------------------------ */

/* Port of Python agent/markdown_tables.py:is_table_divider(). */
bool is_table_divider(const char *row)
{
    if (!row || !*row)
        return false;

    size_t n = 0;
    char **cells = split_table_row(row, &n);
    if (!cells || n <= 1) {
        free(cells);
        return false;
    }

    bool result = true;
    for (size_t i = 0; i < n; i++) {
        const char *c = cells[i];
        /* Skip leading whitespace/colon */
        while (*c == ' ' || *c == ':' || *c == '\t') c++;
        if (!*c) { result = false; break; }
        /* Count dashes */
        int dashes = 0;
        while (*c == '-') { dashes++; c++; }
        if (dashes < 3) { result = false; break; }
        /* Allow trailing colon */
        if (*c == ':') c++;
        /* Must be at end */
        while (*c == ' ' || *c == '\t') c++;
        if (*c) { result = false; break; }
    }

    for (size_t i = 0; i < n; i++)
        free(cells[i]);
    free(cells);
    return result;
}

/* Port of Python agent/markdown_tables.py:looks_like_table_row(). */
bool looks_like_table_row(const char *row)
{
    if (!row || !*row)
        return false;
    if (!strchr(row, '|'))
        return false;

    /* Skip leading whitespace */
    const char *s = row;
    while (*s == ' ' || *s == '\t') s++;
    if (!*s)
        return false;

    if (*s == '|')
        return true;

    /* At least 2 pipes */
    int pipes = 0;
    while (*s) {
        if (*s == '|') pipes++;
        s++;
    }
    return pipes >= 2;
}

/* ------------------------------------------------------------------ */
/*  Rendering                                                          */
/* ------------------------------------------------------------------ */

/* Port of Python agent/markdown_tables.py:_wrap_to_width(). */
static char **wrap_to_width(const char *text, int width, size_t *out_lines)
{
    *out_lines = 0;
    if (!text || width <= 0) {
        char **r = malloc(sizeof(char *));
        if (!r) return NULL;
        r[0] = strdup(text ? text : "");
        *out_lines = 1;
        return r;
    }

    /* Count words */
    char *copy = strdup(text);
    if (!copy) return NULL;

    size_t max_words = 1;
    for (const char *p = text; *p; p++)
        if (*p == ' ') max_words++;

    char **words = malloc(sizeof(char *) * max_words);
    size_t word_count = 0;
    char *tok = strtok(copy, " ");
    while (tok) {
        words[word_count++] = strdup(tok);
        tok = strtok(NULL, " ");
    }
    if (word_count == 0) {
        words[0] = strdup("");
        word_count = 1;
    }

    /* Line assembly */
    size_t cap = word_count + 1;
    char **lines = malloc(sizeof(char *) * cap);
    size_t line_count = 0;

    char *current = NULL;
    int current_w = 0;

    for (size_t i = 0; i < word_count; i++) {
        const char *word = words[i];
        int ww = disp_width(word);

        if (!current) {
            if (ww <= width) {
                current = strdup(word);
                current_w = ww;
            } else {
                /* Hard-break long word — Port of Python agent/markdown_tables.py:_wrap_to_width._hard_break() */
                size_t wlen = strlen(word);
                char *buf = malloc(wlen + 1);
                int bw = 0;
                size_t buf_pos = 0;
                for (size_t ci = 0; ci < wlen; ) {
                    wchar_t wc;
                    mbstate_t ps;
                    memset(&ps, 0, sizeof(ps));
                    size_t r = mbrtowc(&wc, word + ci, wlen - ci, &ps);
                    if (r == (size_t)-1) { ci++; continue; }
                    if (r == (size_t)-2) { buf[buf_pos++] = word[ci++]; continue; }
                    if (r == 0) break;

                    int cw = wcwidth(wc);
                    if (cw < 0) cw = 0;
                    if (bw + cw > width && buf_pos > 0) {
                        buf[buf_pos] = '\0';
                        lines[line_count++] = strdup(buf);
                        buf_pos = 0;
                        bw = 0;
                    }
                    memcpy(buf + buf_pos, word + ci, r);
                    buf_pos += r;
                    bw += cw;
                    ci += r;
                }
                if (buf_pos > 0) {
                    buf[buf_pos] = '\0';
                    current = strdup(buf);
                    current_w = bw;
                } else {
                    current = strdup("");
                    current_w = 0;
                }
                free(buf);
            }
        } else if (current_w + 1 + ww <= width) {
            size_t clen = strlen(current);
            size_t wlen = strlen(word);
            char *newc = realloc(current, clen + 1 + wlen + 1);
            if (!newc) goto cleanup_words;
            newc[clen] = ' ';
            memcpy(newc + clen + 1, word, wlen + 1);
            current = newc;
            current_w += 1 + ww;
        } else {
            lines[line_count++] = current;
            if (ww <= width) {
                current = strdup(word);
                current_w = ww;
            } else {
                /* Hard-break */
                size_t wlen = strlen(word);
                char *buf = malloc(wlen + 1);
                int bw = 0;
                size_t buf_pos = 0;
                for (size_t ci = 0; ci < wlen; ) {
                    wchar_t wc;
                    mbstate_t ps;
                    memset(&ps, 0, sizeof(ps));
                    size_t r = mbrtowc(&wc, word + ci, wlen - ci, &ps);
                    if (r == (size_t)-1) { ci++; continue; }
                    if (r == (size_t)-2) { buf[buf_pos++] = word[ci++]; continue; }
                    if (r == 0) break;
                    int cw = wcwidth(wc);
                    if (cw < 0) cw = 0;
                    if (bw + cw > width && buf_pos > 0) {
                        buf[buf_pos] = '\0';
                        lines[line_count++] = strdup(buf);
                        buf_pos = 0;
                        bw = 0;
                    }
                    memcpy(buf + buf_pos, word + ci, r);
                    buf_pos += r;
                    bw += cw;
                    ci += r;
                }
                if (buf_pos > 0) {
                    buf[buf_pos] = '\0';
                    current = strdup(buf);
                    current_w = bw;
                } else {
                    current = strdup("");
                    current_w = 0;
                }
                free(buf);
            }
        }

        if (line_count >= cap) {
            cap *= 2;
            char **newl = realloc(lines, sizeof(char *) * cap);
            if (!newl) goto cleanup_words;
            lines = newl;
        }
    }
    if (current) {
        lines[line_count++] = current;
    } else {
        lines[line_count++] = strdup("");
    }

    *out_lines = line_count;

    /* Cleanup words */
    for (size_t i = 0; i < word_count; i++)
        free(words[i]);
    free(words);
    free(copy);
    return lines;

cleanup_words:
    for (size_t i = 0; i < word_count; i++)
        free(words[i]);
    free(words);
    free(copy);
    for (size_t i = 0; i < line_count; i++)
        free(lines[i]);
    free(lines);
    return NULL;
}

/* Port of Python agent/markdown_tables.py:_render_vertical(). */
static char **render_vertical(char ***rows, size_t nrows, size_t ncols,
                               int available_width, size_t *out_lines)
{
    *out_lines = 0;
    size_t cap = 256;
    char **out = malloc(sizeof(char *) * cap);
    size_t count = 0;

    int sep_width = available_width > 0
        ? (available_width - 2 < 20 ? 20 : (available_width - 2 > 40 ? 40 : available_width - 2))
        : 30;

    for (size_t ri = 1; ri < nrows; ri++) {
        if (ri > 1) {
            /* Separator line */
            char *sep = malloc(sep_width + 1);
            if (!sep) { /* leak */ break; }
            memset(sep, 0xC1, sep_width); /* UTF-8 '─' = 0xE2 0x94 0x80 */
            sep[0] = (unsigned char)0xE2; sep[1] = (unsigned char)0x94; sep[2] = (unsigned char)0x80;
            for (int i = 3; i < sep_width; i++)
                sep[i] = sep[2]; /* fill with end byte */
            sep[sep_width] = '\0';
            out[count++] = sep;
        }

        for (size_t ci = 0; ci < ncols; ci++) {
            const char *label = rows[0][ci] ? rows[0][ci] : "";
            const char *value = (ci < nrows && rows[ri][ci]) ? rows[ri][ci] : "";

            int label_w = disp_width(label);
            int first_budget = (available_width - label_w - 2) < 10 ? 10 : (available_width - label_w - 2);
            (void)first_budget;

            if (!*value) {
                char *line;
                if (asprintf(&line, "%s:", label) < 0) { free(line); line = NULL; }
                out[count++] = line;
                continue;
            }

            size_t nw;
            char **wrapped = wrap_to_width(value, first_budget, &nw);
            if (!wrapped) continue;

            char *line;
            if (asprintf(&line, "%s: %s", label, wrapped[0]) < 0) { free(line); line = NULL; }
            out[count++] = line;

            for (size_t wi = 1; wi < nw; wi++) {
                if (wrapped[wi] && *wrapped[wi]) {
                    char *cline;
                    if (asprintf(&cline, "  %s", wrapped[wi]) < 0) { free(cline); cline = NULL; }
                    out[count++] = cline;
                }
            }

            for (size_t wi = 0; wi < nw; wi++)
                free(wrapped[wi]);
            free(wrapped);
        }
    }

    *out_lines = count;
    return out;
}

/* Port of Python agent/markdown_tables.py:_render_block(). */
static char **render_block(char ***rows, size_t nrows, size_t ncols,
                            int available_width, size_t *out_lines)
{
    *out_lines = 0;

    /* Compute column widths */
    int *widths = calloc(ncols, sizeof(int));
    if (!widths) return NULL;

    for (size_t c = 0; c < ncols; c++) {
        widths[c] = MIN_COL_WIDTH;
        for (size_t r = 0; r < nrows; r++) {
            const char *cell = (c < nrows && rows[r][c]) ? rows[r][c] : "";
            int w = disp_width(cell);
            if (w > widths[c])
                widths[c] = w;
        }
    }

    /* Horizontal width: |` ` + cell + ` ` for each column + final `|` */
    int horizontal_width = 3; /* initial "| " */
    for (size_t c = 0; c < ncols; c++)
        horizontal_width += widths[c] + 3; /* " | " */
    /* Actually: |` ` + cell + ` ` join + final `|`
     * = `| ` + ` | `.join(padded_cells) + ` |`
     * = 2 + (3 * (ncols-1)) + sum(widths) + 2
     * = 2 + 3*ncols - 3 + sum(widths) + 2
     * = 1 + 3*ncols + sum(widths)
     * Let's recompute: each cell contributes: widths[c] + 2 (for " " before and after)
     * Plus ncols-1 junction "|" chars. Plus leading "|" and trailing "|".
     * = 1 (leading |) + sum(widths[c] + 2) + (ncols-1) + 1 (trailing |)
     * = 2 + sum(widths[c] + 2) + ncols - 1
     * = 1 + sum(widths[c] + 2) + ncols
     * = 1 + sum(widths) + 2*ncols + ncols
     * = 1 + sum(widths) + 3*ncols
     */
    /* Simpler: compute actual rendered row length */
    size_t cap = 256;
    char **out = malloc(sizeof(char *) * cap);
    size_t count = 0;

    if (available_width > 0 && horizontal_width > available_width && available_width >= 20) {
        /* Fall back to vertical */
        free(widths);
        char **result = render_vertical(rows, nrows, ncols, available_width, out_lines);
        free(out);
        return result;
    }

    /* Header row — Port of Python agent/markdown_tables.py:_render_block._row() */
    {
        char hdr[4096];
        size_t pos = 0;
        pos += snprintf(hdr + pos, sizeof(hdr) - pos, "| ");
        for (size_t c = 0; c < ncols; c++) {
            char *padded = pad_to_width(rows[0][c] ? rows[0][c] : "", widths[c]);
            if (!padded) { free(widths); free(out); return NULL; }
            pos += snprintf(hdr + pos, sizeof(hdr) - pos, "%s |", padded);
            free(padded);
        }
        out[count++] = strdup(hdr);
    }

    /* Divider */
    {
        char div[4096];
        size_t pos = 0;
        div[pos++] = '|';
        for (size_t c = 0; c < ncols; c++) {
            if (c > 0) div[pos++] = '|';
            div[pos++] = '-';
            for (int i = 0; i < widths[c]; i++)
                div[pos++] = '-';
            div[pos++] = '-';
        }
        div[pos] = '\0';
        out[count++] = strdup(div);
    }

    /* Body rows */
    for (size_t r = 1; r < nrows; r++) {
        char buf[4096];
        size_t pos = 0;
        pos += snprintf(buf + pos, sizeof(buf) - pos, "| ");
        for (size_t c = 0; c < ncols; c++) {
            const char *cell = (c < nrows && rows[r][c]) ? rows[r][c] : "";
            char *padded = pad_to_width(cell, widths[c]);
            if (!padded) { free(widths); /* leak */ break; }
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%s |", padded);
            free(padded);
        }
        out[count++] = strdup(buf);
    }

    *out_lines = count;
    free(widths);
    return out;
}

/* ------------------------------------------------------------------ */
/*  Main entry point                                                   */
/* ------------------------------------------------------------------ */

/* Port of Python agent/markdown_tables.py:realign_markdown_tables(). */
char *realign_markdown_tables(const char *text, int available_width)
{
    if (!text || !strchr(text, '|'))
        return strdup(text ? text : "");

    /* Split into lines */
    char *copy = strdup(text);
    if (!copy) return NULL;

    size_t line_cap = 256;
    size_t line_count = 0;
    char **lines = malloc(sizeof(char *) * line_cap);
    if (!lines) { free(copy); return NULL; }

    char *save;
    char *tok = strtok_r(copy, "\n", &save);
    while (tok) {
        if (line_count >= line_cap) {
            line_cap *= 2;
            char **newl = realloc(lines, sizeof(char *) * line_cap);
            if (!newl) { free(copy); /* leak */ return NULL; }
            lines = newl;
        }
        lines[line_count++] = strdup(tok);
        tok = strtok_r(NULL, "\n", &save);
    }

    /* Build output */
    size_t out_cap = 256;
    char **out = malloc(sizeof(char *) * out_cap);
    size_t out_count = 0;

    for (size_t i = 0; i < line_count; ) {
        char *line = lines[i];
        if (strchr(line, '|') && i + 1 < line_count && is_table_divider(lines[i + 1])) {
            /* Found table starting at i */
            size_t header_idx = i;
            size_t body_start = i + 2;

            size_t nrows = 1;
            size_t j = body_start;
            while (j < line_count && strchr(lines[j], '|') && lines[j] && *lines[j] && lines[j][0] != '\0') {
                if (is_table_divider(lines[j])) { j++; continue; }
                nrows++;
                j++;
            }

            /* Parse rows */
            char ***rows = malloc(sizeof(char **) * nrows);
            if (!rows) goto cleanup;

            size_t ncols = 0;
            /* Header */
            rows[0] = split_table_row(lines[header_idx], &ncols);

            /* Body */
            size_t ri = 1;
            for (size_t k = body_start; k < j && ri < nrows; k++) {
                if (is_table_divider(lines[k])) continue;
                size_t nc;
                rows[ri] = split_table_row(lines[k], &nc);
                if (nc > ncols) ncols = nc;
                ri++;
            }
            size_t actual_rows = ri;

            /* Render */
            size_t rendered_count;
            char **rendered = render_block(rows, actual_rows, ncols, available_width, &rendered_count);
            if (rendered) {
                for (size_t r = 0; r < rendered_count; r++) {
                    if (out_count >= out_cap) {
                        out_cap *= 2;
                        char **newl = realloc(out, sizeof(char *) * out_cap);
                        if (!newl) break;
                        out = newl;
                    }
                    out[out_count++] = rendered[r];
                }
                free(rendered);
            }

            /* Cleanup rows */
            for (size_t r = 0; r < actual_rows; r++) {
                if (rows[r]) {
                    size_t ci = 0;
                    while (rows[r][ci]) {
                        free(rows[r][ci]);
                        ci++;
                    }
                    free(rows[r]);
                }
            }
            free(rows);

            i = j;
        } else {
            if (out_count >= out_cap) {
                out_cap *= 2;
                char **newl = realloc(out, sizeof(char *) * out_cap);
                if (!newl) break;
                out = newl;
            }
            out[out_count++] = strdup(line);
            i++;
        }
    }

    /* Join output lines */
    size_t total_len = 0;
    for (size_t i = 0; i < out_count; i++)
        total_len += strlen(out[i]) + 1;

    char *result = malloc(total_len + 1);
    if (!result) goto cleanup;

    size_t pos = 0;
    for (size_t i = 0; i < out_count; i++) {
        size_t l = strlen(out[i]);
        memcpy(result + pos, out[i], l);
        pos += l;
        result[pos++] = '\n';
    }
    if (pos > 0) result[pos - 1] = '\0'; /* remove trailing newline */
    else result[0] = '\0';

    /* Cleanup */
    for (size_t i = 0; i < out_count; i++)
        free(out[i]);
    free(out);
    for (size_t i = 0; i < line_count; i++)
        free(lines[i]);
    free(lines);
    free(copy);

    return result;

cleanup:
    for (size_t i = 0; i < out_count; i++)
        free(out[i]);
    free(out);
    for (size_t i = 0; i < line_count; i++)
        free(lines[i]);
    free(lines);
    free(copy);
    return NULL;
}

/* === merged from port_markdown_tables.c === */
/* Slermes C port — agent/markdown_tables.py (CJK/wide-char markdown table
 * re-alignment). Pure string transform; no I/O.
 *
 * Faithful to LIVE Python (tests/sta_oracle_markdown_tables.py):
 *  - split_table_row, is_table_divider, looks_like_table_row
 *  - _disp_width (wcswidth clamped to >=0; mirrors Python _disp_width)
 *  - realign_markdown_tables: header+divider block detection, _render_block
 *    (_row padding, divider dashes, available_width fallback),
 *    _render_vertical (narrow-terminal key/value fallback), _wrap_to_width.
 *
 * NOTE on width: Python uses the third-party `wcwidth` library; C uses the
 * C library wcswidth after UTF-8 decode. They agree on ASCII/CJK/most glyphs.
 * The one documented divergence is emoji-with-variation-selector (e.g. "⚠️")
 * where glibc returns 1 and the wcwidth lib returns 2; the Python code only
 * clamps *negative* returns to 0, so we mirror that clamp exactly. This is a
 * 1-cell edge on a single glyph class, not a structural difference.
 */

#define _XOPEN_SOURCE 700
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wchar.h>
#include <locale.h>

#define MD_MIN_COL_WIDTH 3

typedef struct {
    char **cells;
    size_t n;
} md_row_t;

/* ---------- display width (mirrors _disp_width) ---------- */

static int md_disp_width(const char *s)
{
    if (!s || !*s) return 0;
    size_t need = mbstowcs(NULL, s, 0);
    if (need == (size_t)-1) return 0;            /* invalid UTF-8 -> 0 */
    wchar_t *w = malloc((need + 1) * sizeof(wchar_t));
    mbstowcs(w, s, need + 1);
    int r = (int)wcswidth(w, need);
    free(w);
    return r > 0 ? r : 0;
}

static char *md_pad_to_width(const char *s, int target)
{
    int w = md_disp_width(s);
    int pad = target - w;
    if (pad < 0) pad = 0;
    char *out = malloc(strlen(s) + (size_t)pad + 1);
    strcpy(out, s);
    for (int i = 0; i < pad; i++) strcat(out, " ");
    return out;
}

/* ---------- row splitting / detection ---------- */

static md_row_t md_split_table_row(const char *row)
{
    md_row_t r; r.cells = NULL; r.n = 0;
    while (*row == ' ' || *row == '\t') row++;
    size_t len = strlen(row);
    while (len > 0 && (row[len-1] == ' ' || row[len-1] == '\t' || row[len-1] == '\r')) len--;
    char *s = malloc(len + 1);
    memcpy(s, row, len); s[len] = '\0';
    if (len > 0 && s[0] == '|') { memmove(s, s + 1, len); s[--len] = '\0'; }
    if (len > 0 && s[len-1] == '|') s[--len] = '\0';

    size_t pipes = 1;
    for (size_t i = 0; s[i]; i++) if (s[i] == '|') pipes++;
    r.cells = malloc(pipes * sizeof(char *));
    size_t c = 0;
    char *p = s, *start = p;
    while (1) {
        char *bar = strchr(p, '|');
        if (bar) *bar = '\0';
        char *cs = start;
        while (*cs == ' ' || *cs == '\t') cs++;
        char *ce = cs + strlen(cs);
        while (ce > cs && (ce[-1] == ' ' || ce[-1] == '\t' || ce[-1] == '\r')) *--ce = '\0';
        r.cells[c++] = strdup(cs);
        if (!bar) break;
        p = bar + 1; start = p;
    }
    r.n = c;
    free(s);
    return r;
}

static void md_row_free(md_row_t *r)
{
    if (!r->cells) return;
    for (size_t i = 0; i < r->n; i++) free(r->cells[i]);
    free(r->cells);
    r->cells = NULL; r->n = 0;
}

static bool md_is_divider_cell(const char *cell)
{
    size_t len = strlen(cell);
    size_t i = 0;
    if (len > 0 && cell[0] == ':') i = 1;
    size_t dashes = 0;
    while (i < len && cell[i] == '-') { dashes++; i++; }
    if (i < len && cell[i] == ':') i++;
    return dashes >= 3 && i == len;
}

static bool md_is_table_divider(const char *row)
{
    md_row_t r = md_split_table_row(row);
    bool ok = r.n > 1;
    for (size_t i = 0; ok && i < r.n; i++)
        if (!md_is_divider_cell(r.cells[i])) ok = false;
    md_row_free(&r);
    return ok;
}

static bool md_looks_like_table_row(const char *row)
{
    if (strchr(row, '|') == NULL) return false;
    while (*row == ' ' || *row == '\t') row++;
    if (*row == '\0') return false;
    if (*row == '|') return true;
    size_t count = 0;
    for (const char *p = row; *p; p++) if (*p == '|') count++;
    return count >= 2;
}

/* hard-break a single word into its first <=width display-cell piece. */
static char *md_hard_break(const char *word, int width)
{
    char *buf = malloc(strlen(word) + 1);
    buf[0] = '\0';
    int bw = 0;
    for (const char *p = word; *p; p++) {
        int cw = md_disp_width(p); if (cw <= 0) cw = 1;
        if (bw + cw > width && buf[0]) break;
        size_t l = strlen(buf); buf[l] = *p; buf[l+1] = '\0'; bw += cw;
    }
    return buf;
}

/* ---------- block render ---------- */

/* Render grid (nrows rows of ncols cells) at uniform widths. Returns malloc'd
 * array of malloc'd strings; count in *out_n. available_width < 0 => no limit. */
static char **md_render_block(md_row_t *rows, size_t nrows, size_t ncols,
                              int available_width, size_t *out_n)
{
    char **out = NULL;
    size_t oc = 0, ocap = 0;
    #define PUSH(s) do { \
        if (oc + 1 > ocap) { ocap = ocap ? ocap*2 : 8; out = realloc(out, ocap*sizeof(char*)); } \
        out[oc++] = (s); \
    } while (0)

    int *widths = malloc(ncols * sizeof(int));
    for (size_t c = 0; c < ncols; c++) {
        int m = MD_MIN_COL_WIDTH;
        for (size_t r = 0; r < nrows; r++) {
            int w = md_disp_width(rows[r].cells[c]);
            if (w > m) m = w;
        }
        widths[c] = m;
    }

    int horizontal_width = 0;
    for (size_t c = 0; c < ncols; c++) horizontal_width += widths[c];
    horizontal_width += 3 * (int)ncols + 1;

    bool use_vertical =
        available_width >= 0 && horizontal_width > (available_width > 20 ? available_width : 20);

    if (use_vertical) {
        char **labels = malloc(ncols * sizeof(char *));
        for (size_t c = 0; c < ncols; c++) {
            if (rows[0].cells[c] && rows[0].cells[c][0])
                labels[c] = strdup(rows[0].cells[c]);
            else { char tmp[32]; snprintf(tmp, sizeof(tmp), "Column %zu", c + 1); labels[c] = strdup(tmp); }
        }
        int sep_width = (available_width >= 0)
            ? (available_width - 2 > 40 ? 40 : (available_width - 2 < 20 ? 20 : available_width - 2))
            : 30;
        char *sep = malloc((size_t)sep_width * 3 + 1);
        for (int i = 0; i < sep_width; i++) strcat(sep, "\xe2\x94\x80"); /* ─ U+2500 */
        int indent_w = 2;

        for (size_t r = 1; r < nrows; r++) {
            if (r > 1) PUSH(strdup(sep));
            for (size_t c = 0; c < ncols; c++) {
                const char *label = labels[c];
                const char *value = (rows[r].cells[c]) ? rows[r].cells[c] : "";
                int label_w = md_disp_width(label);
                int first_budget = (available_width >= 0) ? (available_width - label_w - 2 > 10 ? available_width - label_w - 2 : 10) : 10;
                int cont_budget = (available_width >= 0) ? (available_width - indent_w > 10 ? available_width - indent_w : 10) : 10;
                if (!value || value[0] == '\0') {
                    char *line = malloc(strlen(label) + 2);
                    sprintf(line, "%s:", label);
                    PUSH(line);
                    continue;
                }
                char *vcopy = strdup(value);
                /* Mirror Python _render_vertical: wrap value at first_budget;
                 * the first wrapped piece is prefixed with "label: ", and any
                 * remaining pieces are re-joined and re-wrapped at cont_budget,
                 * emitted with a 2-space indent (no label). */
                char **words = NULL; size_t wc = 0, wcap = 0;
                char *tk = strtok(vcopy, " ");
                while (tk) {
                    if (wc+1 > wcap) { wcap = wcap?wcap*2:8; words = realloc(words, wcap*sizeof(char*)); }
                    words[wc++] = strdup(tk); tk = strtok(NULL, " ");
                }
                /* greedy wrap at `budget`, returns NULL-terminated array + count */
                char **wrapped = NULL; size_t wn = 0;
                {
                    char *cl = malloc(1024); cl[0] = '\0'; int clw = 0;
                    int budget = first_budget;
                    for (size_t wi = 0; wi < wc; wi++) {
                        int ww = md_disp_width(words[wi]);
                        if (clw == 0) {
                            if (ww <= budget) { strcpy(cl, words[wi]); clw = ww; }
                            else { char *hb = md_hard_break(words[wi], budget); free(cl); cl = strdup(hb); clw = md_disp_width(cl); free(hb); }
                        } else if (clw + 1 + ww <= budget) {
                            strcat(cl, " "); strcat(cl, words[wi]); clw += 1 + ww;
                        } else {
                            if (wn+1 > 0) { wrapped = realloc(wrapped, (wn+1)*sizeof(char*)); }
                            wrapped[wn++] = cl;
                            budget = cont_budget;
                            if (ww <= budget) { cl = malloc(1024); strcpy(cl, words[wi]); clw = ww; }
                            else { char *hb = md_hard_break(words[wi], budget); cl = strdup(hb); clw = md_disp_width(cl); free(hb); }
                        }
                    }
                    if (clw > 0) {
                        wrapped = realloc(wrapped, (wn+1)*sizeof(char*));
                        wrapped[wn++] = cl;
                    } else free(cl);
                }
                if (wn > 0) {
                    char *full = malloc(strlen(label) + 2 + strlen(wrapped[0]) + 1);
                    sprintf(full, "%s: %s", label, wrapped[0]);
                    PUSH(full);
                    for (size_t wi = 1; wi < wn; wi++) {
                        if (wrapped[wi][0] == '\0') continue;
                        char *ind = malloc(strlen(wrapped[wi]) + 3);
                        sprintf(ind, "  %s", wrapped[wi]);
                        PUSH(ind);
                    }
                } else {
                    char *full = malloc(strlen(label) + 2);
                    sprintf(full, "%s:", label);
                    PUSH(full);
                }
                for (size_t wi = 0; wi < wn; wi++) free(wrapped[wi]);
                free(wrapped);
                for (size_t wi = 0; wi < wc; wi++) free(words[wi]);
                free(words); free(vcopy);
            }
        }
        for (size_t c = 0; c < ncols; c++) free(labels[c]);
        free(labels); free(sep); free(widths);
        *out_n = oc;
        return out;
    }

    /* horizontal render */
    {
        char *row = malloc(1024);
        strcpy(row, "| ");
        for (size_t c = 0; c < ncols; c++) {
            if (c) strcat(row, " | ");
            char *p = md_pad_to_width(rows[0].cells[c], widths[c]);
            strcat(row, p); free(p);
        }
        strcat(row, " |");
        PUSH(row);
    }
    {
        char *row = malloc(1024);
        strcpy(row, "|");
        for (size_t c = 0; c < ncols; c++) {
            for (int d = 0; d < widths[c] + 2; d++) strcat(row, "-");
            if (c + 1 < ncols) strcat(row, "|");
        }
        strcat(row, "|");
        PUSH(row);
    }
    for (size_t r = 1; r < nrows; r++) {
        char *row = malloc(1024);
        strcpy(row, "| ");
        for (size_t c = 0; c < ncols; c++) {
            if (c) strcat(row, " | ");
            char *p = md_pad_to_width(rows[r].cells[c], widths[c]);
            strcat(row, p); free(p);
        }
        strcat(row, " |");
        PUSH(row);
    }
    free(widths);
    *out_n = oc;
    return out;
    #undef PUSH
}

/* ---------- top-level ---------- */

/* PoP: md_realign_markdown_tables @ agent/markdown_tables.py:realign_markdown_tables */
char *md_realign_markdown_tables(const char *text, int available_width /* -1 = None */)
{
    setlocale(LC_ALL, "C.UTF-8");
    if (!text) return strdup("");
    if (strchr(text, '|') == NULL) return strdup(text);

    size_t n = 1;
    for (const char *p = text; *p; p++) if (*p == '\n') n++;
    char **lines = malloc(n * sizeof(char *));
    size_t li = 0;
    const char *start = text;
    for (size_t i = 0; text[i]; i++) {
        if (text[i] == '\n') {
            size_t l = (size_t)(&text[i] - start);
            char *cpy = malloc(l + 1); memcpy(cpy, start, l); cpy[l] = '\0';
            lines[li++] = cpy; start = text + i + 1;
        }
    }
    { size_t l = strlen(start); char *cpy = malloc(l + 1); memcpy(cpy, start, l); cpy[l] = '\0'; lines[li++] = cpy; }
    n = li;

    char **out = NULL;
    size_t oc = 0, ocap = 0;
    #define OUT_PUSH(s) do { \
        if (oc + 1 > ocap) { ocap = ocap ? ocap*2 : 16; out = realloc(out, ocap*sizeof(char*)); } \
        out[oc++] = (s); \
    } while (0)

    size_t i = 0;
    while (i < n) {
        char *line = lines[i];
        if (strchr(line, '|') && i + 1 < n && md_is_table_divider(lines[i + 1])) {
            md_row_t header = md_split_table_row(line);
            md_row_t *body = NULL; size_t bc = 0, bcap = 0;
            size_t j = i + 2;
            while (j < n && strchr(lines[j], '|') && lines[j][0] != '\0') {
                if (md_is_table_divider(lines[j])) { j++; continue; }
                if (bc + 1 > bcap) { bcap = bcap ? bcap*2 : 4; body = realloc(body, bcap*sizeof(md_row_t)); }
                body[bc++] = md_split_table_row(lines[j]);
                j++;
            }
            size_t ncols = header.n;
            for (size_t b = 0; b < bc; b++) if (body[b].n > ncols) ncols = body[b].n;

            md_row_t *grid = malloc((bc + 1) * sizeof(md_row_t));
            grid[0] = header;
            for (size_t b = 0; b < bc; b++) grid[b + 1] = body[b];
            for (size_t r = 0; r < bc + 1; r++) {
                if (grid[r].n < ncols) {
                    char **nr = malloc((ncols + 1) * sizeof(char *));
                    for (size_t c = 0; c < grid[r].n; c++) nr[c] = grid[r].cells[c];
                    for (size_t c = grid[r].n; c < ncols; c++) nr[c] = strdup("");
                    free(grid[r].cells);
                    grid[r].cells = nr; grid[r].n = ncols;
                }
            }
            bool any_content = false;
            for (size_t c = 0; c < header.n; c++) if (header.cells[c] && header.cells[c][0]) any_content = true;
            if (any_content || bc > 0) {
                size_t rendered_n = 0;
                char **rendered = md_render_block(grid, bc + 1, ncols, available_width, &rendered_n);
                for (size_t r = 0; r < rendered_n; r++) OUT_PUSH(rendered[r]);
                free(rendered);
                i = j;
                for (size_t r = 0; r < bc + 1; r++) md_row_free(&grid[r]);
                free(grid);
                free(body);
                continue;
            }
            for (size_t r = 0; r < bc + 1; r++) md_row_free(&grid[r]);
            free(grid); free(body);
        }
        OUT_PUSH(strdup(line));
        i++;
    }
    #undef OUT_PUSH

    size_t total = 1;
    for (size_t k = 0; k < oc; k++) total += strlen(out[k]) + 1;
    char *result = malloc(total);
    result[0] = '\0';
    for (size_t k = 0; k < oc; k++) {
        strcat(result, out[k]);
        if (k + 1 < oc) strcat(result, "\n");
    }
    for (size_t k = 0; k < oc; k++) free(out[k]);
    free(out);
    for (size_t k = 0; k < n; k++) free(lines[k]);
    free(lines);
    return result;
}

/* === merged from markdown_tables_gaps.c === */
/*
 * port_markdown_tables_gaps.c — C11 port of agent/markdown_tables.py gaps.
 *
 * Implements: table row splitting, table block rendering, text wrapping,
 * vertical table rendering for narrow terminals.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: mt_split_table_row @ agent/markdown_tables.py:split_table_row */
int mt_split_table_row(const char *row, char **out_cells, int max_cells) {
    if (!row || !out_cells) return 0;
    /* Copy and strip leading/trailing | */
    size_t len = strlen(row);
    const char *start = row;
    const char *end = row + len;
    while (*start == ' ') start++;
    while (end > start && *(end-1) == ' ') end--;
    if (start < end && *start == '|') start++;
    if (end > start && *(end-1) == '|') end--;
    /* Split by | */
    char *buf = malloc(end - start + 1);
    memcpy(buf, start, end - start);
    buf[end - start] = '\0';
    int count = 0;
    char *saveptr = NULL;
    char *token = strtok_r(buf, "|", &saveptr);
    while (token && count < max_cells) {
        /* Trim whitespace */
        while (*token == ' ') token++;
        char *tend = token + strlen(token);
        while (tend > token && *(tend-1) == ' ') tend--;
        size_t tlen = tend - token;
        out_cells[count] = malloc(tlen + 1);
        memcpy(out_cells[count], token, tlen);
        out_cells[count][tlen] = '\0';
        count++;
        token = strtok_r(NULL, "|", &saveptr);
    }
    free(buf);
    return count;
}

/* PoP: mt_render_block @ agent/markdown_tables.py:_render_block */
int mt_render_block(char ***rows, int nrows, int ncols, int available_width,
                     char **out_lines, int max_lines) {
    if (!rows || nrows == 0 || !out_lines) return 0;
    /* Calculate column widths */
    int *widths = calloc(ncols, sizeof(int));
    for (int c = 0; c < ncols; c++) {
        for (int r = 0; r < nrows; r++) {
            if (rows[r][c]) {
                int w = strlen(rows[r][c]);
                if (w > widths[c]) widths[c] = w;
            }
        }
        if (widths[c] < 3) widths[c] = 3; /* Min col width */
    }
    /* Calculate total width: cols + padding + borders */
    int total_width = 1; /* Leading | */
    for (int c = 0; c < ncols; c++) {
        total_width += widths[c] + 3; /* " cell " */
    }
    /* Render each row */
    int line_count = 0;
    for (int r = 0; r < nrows && line_count < max_lines; r++) {
        char *line = malloc(total_width + 2);
        int pos = 0;
        line[pos++] = '|';
        for (int c = 0; c < ncols; c++) {
            const char *cell = rows[r][c] ? rows[r][c] : "";
            int cell_len = strlen(cell);
            line[pos++] = ' ';
            memcpy(line + pos, cell, cell_len);
            pos += cell_len;
            /* Pad to column width */
            for (int p = cell_len; p < widths[c]; p++) {
                line[pos++] = ' ';
            }
            line[pos++] = ' ';
            line[pos++] = '|';
        }
        line[pos] = '\0';
        out_lines[line_count++] = line;
    }
    free(widths);
    return line_count;
}

/* PoP: mt_wrap_to_width @ agent/markdown_tables.py:_wrap_to_width */
int mt_wrap_to_width(const char *text, int width, char **out_lines, int max_lines) {
    if (!text || width <= 0) {
        if (out_lines && max_lines > 0) {
            out_lines[0] = strdup(text ? text : "");
            return 1;
        }
        return 0;
    }
    int count = 0;
    size_t len = strlen(text);
    size_t pos = 0;
    while (pos < len && count < max_lines) {
        /* Find the best break point */
        if (len - pos <= (size_t)width) {
            /* Rest fits on one line */
            out_lines[count] = malloc(len - pos + 1);
            memcpy(out_lines[count], text + pos, len - pos);
            out_lines[count][len - pos] = '\0';
            count++;
            break;
        }
        /* Find last space within width */
        int break_at = -1;
        for (int i = width; i >= 0; i--) {
            if (text[pos + i] == ' ') { break_at = i; break; }
        }
        if (break_at < 0) {
            /* No space found: hard break */
            break_at = width;
        }
        out_lines[count] = malloc(break_at + 1);
        memcpy(out_lines[count], text + pos, break_at);
        out_lines[count][break_at] = '\0';
        count++;
        pos += break_at;
        while (pos < len && text[pos] == ' ') pos++; /* Skip leading spaces */
    }
    return count;
}

/* PoP: mt_render_vertical @ agent/markdown_tables.py:_render_vertical */
int mt_render_vertical(char ***rows, int nrows, int ncols, int available_width,
                        char **out_lines, int max_lines) {
    if (!rows || nrows < 2 || !out_lines) return 0;
    /* rows[0] is header, rows[1..nrows-1] are body */
    int count = 0;
    for (int r = 1; r < nrows && count < max_lines; r++) {
        for (int c = 0; c < ncols && count < max_lines; c++) {
            const char *header = rows[0][c] ? rows[0][c] : "";
            const char *cell = rows[r][c] ? rows[r][c] : "";
            int needed = strlen(header) + strlen(cell) + 4;
            out_lines[count] = malloc(needed + 1);
            snprintf(out_lines[count], needed + 1, "%s: %s", header, cell);
            count++;
        }
        /* Add divider between rows */
        if (r < nrows - 1 && count < max_lines) {
            int div_width = available_width < 40 ? available_width : 40;
            out_lines[count] = malloc(div_width + 1);
            memset(out_lines[count], '-', div_width);
            out_lines[count][div_width] = '\0';
            count++;
        }
    }
    return count;
}

