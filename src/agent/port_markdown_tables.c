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

/* PoP: realign_markdown_tables @ agent/markdown_tables.py:realign_markdown_tables */
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
