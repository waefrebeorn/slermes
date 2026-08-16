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
