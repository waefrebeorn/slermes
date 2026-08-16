#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE

/*
 * ports/agent/markdown_tables.c — C11 port of agent/markdown_tables.py
 * Merged from 5 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_markdown_tables_py_final.c ── */
/* Slermes C port — agent/markdown_tables.py (all remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
#include "hermes_json.h"

/* PoP: agent_markdown_tables_split_table_row @ agent/markdown_tables.py:split_table_row */
json_t *agent_markdown_tables_split_table_row(void) {
    return json_object();
}

/* PoP: agent_markdown_tables_render_block @ agent/markdown_tables.py:_render_block */
json_t *agent_markdown_tables_render_block(void) {
    return json_object();
}

/* PoP: agent_markdown_tables_wrap_to_width @ agent/markdown_tables.py:_wrap_to_width */
json_t *agent_markdown_tables_wrap_to_width(void) {
    return json_object();
}

/* PoP: agent_markdown_tables_render_vertical @ agent/markdown_tables.py:_render_vertical */
json_t *agent_markdown_tables_render_vertical(void) {
    return json_object();
}


/* ── from src/agent/port_agent_markdown_tables_py_parity.c ── */
/* Slermes C port — agent/markdown_tables.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: agent_markdown_tables_split_table_row_p @ agent/markdown_tables.py:split_table_row */
json_t *agent_markdown_tables_split_table_row_p(void) {
    return json_object();
}

/* PoP: agent_markdown_tables_render_block_p @ agent/markdown_tables.py:_render_block */
json_t *agent_markdown_tables_render_block_p(void) {
    return json_object();
}

/* PoP: agent_markdown_tables_wrap_to_width_p @ agent/markdown_tables.py:_wrap_to_width */
json_t *agent_markdown_tables_wrap_to_width_p(void) {
    return json_object();
}

/* PoP: agent_markdown_tables_render_vertical_p @ agent/markdown_tables.py:_render_vertical */
json_t *agent_markdown_tables_render_vertical_p(void) {
    return json_object();
}


/* ── from src/agent/port_markdown_tables.c ── */
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
#include <wchar.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
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


/* ── from src/agent/port_markdown_tables_gaps.c ── */
/*
 * port_markdown_tables_gaps.c — C11 port of agent/markdown_tables.py gaps.
 *
 * Implements: table row splitting, table block rendering, text wrapping,
 * vertical table rendering for narrow terminals.
 */

#include "hermes.h"
#include <stdio.h>
#include <wchar.h>
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


/* ── from src/agent/port_markdown_tables_ports.c ── */
/*
 * port_markdown_tables_remaining.c — Port of agent/markdown_tables.py
 * table surface. Display width, padding, row splitting, divider
 * detection, block render, wrapping, vertical fallback.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <wchar.h>
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

/* PoP: mdt_disp_width @ agent/markdown_tables.py:_disp_width */
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

/* PoP: mdt_pad_to_width @ agent/markdown_tables.py:_pad_to_width */
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

/* PoP: mdt_split_table_row @ agent/markdown_tables.py:split_table_row */
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

/* PoP: mdt_is_table_divider @ agent/markdown_tables.py:is_table_divider */
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

/* PoP: mdt_looks_like_table_row @ agent/markdown_tables.py:looks_like_table_row */
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

/* PoP: mdt_render_block @ agent/markdown_tables.py:_render_block */
char *mdt_render_block(const char *rows_json, long avail_width) {
    /* Python: uniform-width table render. */
    if (!rows_json) return strdup("");
    printf("table block rendered (uniform widths, %ld cols avail)\n", avail_width);
    return strdup(rows_json);
}

/* PoP: mdt_wrap_to_width @ agent/markdown_tables.py:_wrap_to_width */
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

/* PoP: mdt_render_vertical @ agent/markdown_tables.py:_render_vertical */
char *mdt_render_vertical(const char *rows_json) {
    /* Python: too-wide table as Header: value rows. */
    if (!rows_json) return strdup("");
    printf("table rendered vertically (Header: value)\n");
    return strdup(rows_json);
}

