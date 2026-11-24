/*
 * cli_backtrack_browse.c — C home for upstream Hermes hermes_cli/cli_backtrack_browse.py.
 * PoP: w26_clibacktra_0c7253_scroll @ hermes_cli/cli_backtrack_browse.py:scroll_into_view
 * PoP: w26_clibacktra_0c7253_geometry @ hermes_cli/cli_backtrack_browse.py:pager_geometry
 * PoP: w26_clibacktra_0c7253_fit_hint @ hermes_cli/cli_backtrack_browse.py:fit_browse_hint
 * PoP: w26_clibacktra_0c7253_constants @ hermes_cli/cli_backtrack_browse.py:<style constants>
 *
 * The pager owns a panel of its own because prompt_toolkit cannot scroll the
 * scrollback.  What is ported here is the arithmetic behind that panel: how a
 * scroll offset is nudged to show the selected prompt, how the panel is sized
 * to the terminal, and which hints survive as the footer narrows.  The
 * terminal size and prompt_toolkit's cell widths are seams -- the size arrives
 * as numbers and a string's width is its code-point count (exact for the
 * ASCII/arrow footer the module builds).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_clibacktra_0c7253.h"

/* Module-level literal constants mirrored from the Python source. */
static const char *const CLIBACKTRA_PROMPT_STYLE = "class:backtrack-prompt bold";
static const char *const CLIBACKTRA_SELECTED_STYLE = "class:backtrack-prompt-selected reverse";
static const char *const CLIBACKTRA_REPLY_STYLE = "class:backtrack-reply";
static const char *const CLIBACKTRA_BORDER = "class:clarify-border";

#define CB_MIDDOT "\x20\xC2\xB7\x20" /* " · " */

/* Terminal cells of a UTF-8 string: one per code point. */
static size_t cb_cells(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (; *s; s++)
        if (((unsigned char)*s & 0xC0) != 0x80) n++;
    return n;
}

/* ``text.split(" · ")``: fills up to `max` (ptr,len) parts, returns the count. */
static int cb_split_middot(const char *s, const char **start, size_t *len, int max) {
    const char *p = s ? s : "";
    const char *hit;
    int i = 0;
    while (i + 1 < max && (hit = strstr(p, CB_MIDDOT)) != NULL) {
        start[i] = p;
        len[i] = (size_t)(hit - p);
        i++;
        p = hit + 4;
    }
    start[i] = p;
    len[i] = strlen(p);
    return i + 1;
}

/* ``part.split(" ")[0]`` over a slice; malloc'd. */
static char *cb_first_token(const char *start, size_t len) {
    size_t n = 0;
    char *out;
    while (n < len && start[n] != ' ') n++;
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, start, n);
    out[n] = '\0';
    return out;
}

typedef struct {
    const char *ptr;
    size_t len;
} cb_slice;

json_t *w26_clibacktra_0c7253_scroll(int offset, const json_t *span, int visible, int total) {
    int first = 0, end = 0, limit;
    if (span && span->type == JSON_ARRAY && json_len(span) >= 2) {
        const json_t *a = json_get(span, 0);
        const json_t *b = json_get(span, 1);
        if (a && a->type == JSON_NUMBER) first = (int)a->num_val;
        if (b && b->type == JSON_NUMBER) end = (int)b->num_val;
    }
    if (first < offset) {
        offset = first;
    } else if (end > offset + visible) {
        int cand = end - visible;
        offset = cand < first ? cand : first;
    }
    if (offset < 0) offset = 0;
    limit = total - visible;
    if (limit < 0) limit = 0;
    if (offset > limit) offset = limit;
    return json_number((double)offset);
}

json_t *w26_clibacktra_0c7253_geometry(int columns, int lines, int reserved_below, int hint_rows) {
    int inner = columns - 5;
    int body = lines - reserved_below - 2 - hint_rows;
    json_t *out;
    if (inner < 20) inner = 20;
    if (body < 3) body = 3;
    out = json_array();
    json_append(out, json_number((double)inner));
    json_append(out, json_number((double)body));
    return out;
}

json_t *w26_clibacktra_0c7253_fit_hint(const char *hint, int width) {
    const char *start[8];
    size_t len[8];
    int parts, r;
    char *nav, *label, *line;
    size_t nl, navlen;
    cb_slice rungs[4][4];
    int rung_len[4] = {4, 3, 2, 1};
    static const cb_slice MIDDOT = {CB_MIDDOT, 4};

    if (!hint) hint = "";
    parts = cb_split_middot(hint, start, len, 8);
    if (parts != 6) return json_string(hint);
    if ((int)cb_cells(hint) <= width) return json_string(hint);

    {
        char *scroll = cb_first_token(start[1], len[1]);
        char *prompts = cb_first_token(start[2], len[2]);
        if (!scroll || !prompts) { free(scroll); free(prompts); return json_string(hint); }
        navlen = strlen(scroll) + 1 + strlen(prompts);
        nav = (char *)malloc(navlen + 1);
        label = (char *)malloc(len[0] + 1);
        if (!nav || !label) {
            free(scroll); free(prompts); free(nav); free(label);
            return json_string(hint);
        }
        memcpy(nav, scroll, strlen(scroll));
        nav[strlen(scroll)] = ' ';
        strcpy(nav + strlen(scroll) + 1, prompts);
        memcpy(label, start[0], len[0]);
        label[len[0]] = '\0';
        free(scroll);
        free(prompts);
    }
    nl = strlen(label);

    rungs[0][0] = (cb_slice){nav, navlen};
    rungs[0][1] = (cb_slice){start[3], len[3]};
    rungs[0][2] = (cb_slice){start[4], len[4]};
    rungs[0][3] = (cb_slice){start[5], len[5]};
    rungs[1][0] = (cb_slice){nav, navlen};
    rungs[1][1] = (cb_slice){start[3], len[3]};
    rungs[1][2] = (cb_slice){start[5], len[5]};
    rungs[2][0] = (cb_slice){start[3], len[3]};
    rungs[2][1] = (cb_slice){start[5], len[5]};
    rungs[3][0] = (cb_slice){start[5], len[5]};

    for (r = 0; r < 4; r++) {
        size_t total = nl, i;
        char *p;
        for (i = 0; i < (size_t)rung_len[r]; i++) total += MIDDOT.len + rungs[r][i].len;
        line = (char *)malloc(total + 1);
        if (!line) break;
        p = line;
        memcpy(p, label, nl); p += nl;
        for (i = 0; i < (size_t)rung_len[r]; i++) {
            memcpy(p, MIDDOT.ptr, MIDDOT.len); p += MIDDOT.len;
            memcpy(p, rungs[r][i].ptr, rungs[r][i].len); p += rungs[r][i].len;
        }
        *p = '\0';
        if ((int)cb_cells(line) <= width) {
            json_t *out = json_string(line);
            free(line); free(nav); free(label);
            return out;
        }
        free(line);
    }
    {
        json_t *out = json_string(label);
        free(nav); free(label);
        return out;
    }
}

json_t *w26_clibacktra_0c7253_constants(void) {
    json_t *o = json_object();
    json_set(o, "prompt_style", json_string(CLIBACKTRA_PROMPT_STYLE));
    json_set(o, "selected_style", json_string(CLIBACKTRA_SELECTED_STYLE));
    json_set(o, "reply_style", json_string(CLIBACKTRA_REPLY_STYLE));
    json_set(o, "border", json_string(CLIBACKTRA_BORDER));
    return o;
}
