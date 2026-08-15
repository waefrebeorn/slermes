/* Slermes C port — gateway/platforms/helpers.py (pure table helpers)
 *
 * Faithful port of the GFM table -> bullet rendering primitives.
 * No live/runtime dependencies. See slermes-god-header-elimination: minimal
 * includes only.
 */

#include <stdbool.h>
#include "hermes_gateway_core.h"
#include "hermes_regex.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

bool gateway_platforms_helpers_is_table_row(const char *line)
{
    if (!line) return false;
    /* skip leading whitespace */
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0') return false;
    return strchr(line, '|') != NULL;
}

char **gateway_platforms_helpers_split_markdown_table_row(const char *line, int *out_n)
{
    /* Returns a NULL-terminated array of malloc'd cell strings (caller frees). */
    if (!line) { if (out_n) *out_n = 0; return NULL; }
    while (*line == ' ' || *line == '\t') line++;
    char *s = strdup(line);
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == '|') s[len - 1] = '\0';
    if (s[0] == '|') memmove(s, s + 1, strlen(s)); /* drop leading pipe */

    int cap = 8, n = 0;
    char **cells = malloc(sizeof(char *) * cap);
    char *p = s, *start = s;
    while (1) {
        if (*p == '|' || *p == '\0') {
            size_t clen = (size_t)(p - start);
            /* trim surrounding spaces of cell */
            char *c = start;
            while (c < p && (*c == ' ' || *c == '\t')) c++;
            char *e = p;
            while (e > c && (e[-1] == ' ' || e[-1] == '\t')) e--;
            size_t tlen = (size_t)(e - c);
            char *cell = malloc(tlen + 1);
            memcpy(cell, c, tlen);
            cell[tlen] = '\0';
            if (n >= cap) { cap *= 2; cells = realloc(cells, sizeof(char *) * cap); }
            cells[n++] = cell;
            if (*p == '\0') break;
            start = p + 1;
        }
        p++;
    }
    cells = realloc(cells, sizeof(char *) * (n + 1));
    cells[n] = NULL;
    if (out_n) *out_n = n;
    free(s);
    return cells;
}

static bool is_table_separator_line(const char *line)
{
    /* matches re '^\s*\|?\s*:?-+:?\s*(?:\|\s*:?-+:?\s*){1,}\|?\s*$' */
    if (!line) return false;
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '|') line++;
    while (*line == ' ' || *line == '\t') line++;
    int dashes = 0, cells = 0, ok = 0;
    const char *p = line;
    while (1) {
        /* optional ':' then one-or-more '-' then optional ':' */
        if (*p == ':') p++;
        if (*p != '-') break;
        while (*p == '-') { p++; dashes++; }
        if (*p == ':') p++;
        if (dashes == 0) break;
        ok = 1;
        cells++;
        /* trailing optional spaces then '|' to continue */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '|') { p++; while (*p == ' ' || *p == '\t') p++; continue; }
        break;
    }
    while (*p == ' ' || *p == '\t') p++;
    return ok && cells >= 1 && *p == '\0';
}

char *gateway_platforms_helpers_render_table_block(char **table_block, int nblock)
{
    /* Returns malloc'd rendered string. Caller frees. */
    if (nblock < 3) {
        /* join as-is */
        size_t total = 1;
        for (int i = 0; i < nblock; i++) total += strlen(table_block[i]) + 1;
        char *out = malloc(total);
        out[0] = '\0';
        for (int i = 0; i < nblock; i++) { strcat(out, table_block[i]); if (i + 1 < nblock) strcat(out, "\n"); }
        return out;
    }
    int nh = 0;
    char **headers = gateway_platforms_helpers_split_markdown_table_row(table_block[0], &nh);
    if (nh < 2) {
        size_t total = 1;
        for (int i = 0; i < nblock; i++) total += strlen(table_block[i]) + 1;
        char *out = malloc(total);
        out[0] = '\0';
        for (int i = 0; i < nblock; i++) { strcat(out, table_block[i]); if (i + 1 < nblock) strcat(out, "\n"); }
        for (int i = 0; i < nh; i++) free(headers[i]);
        free(headers);
        return out;
    }
    int nfd = 0;
    char **first_data = gateway_platforms_helpers_split_markdown_table_row(table_block[2], &nfd);
    bool has_row_label = (nfd == nh + 1);
    (void)first_data;
    for (int i = 0; i < nfd; i++) free(first_data[i]);
    free(first_data);

    size_t cap = 1024, len = 0;
    char *out = malloc(cap);
    out[0] = '\0';
    for (int idx = 1; idx < nblock - 1; idx++) {
        int nc = 0;
        char **cells = gateway_platforms_helpers_split_markdown_table_row(table_block[idx + 1], &nc);
        char *heading;
        char **data_cells;
        int ndata;
        if (has_row_label) {
            heading = (nc > 0 && cells[0][0]) ? strdup(cells[0]) : NULL;
            if (!heading) { heading = malloc(16); snprintf(heading, 16, "Row %d", idx); }
            data_cells = cells + 1; ndata = nc - 1;
        } else {
            heading = NULL;
            for (int k = 0; k < nc; k++) if (cells[k][0]) { heading = strdup(cells[k]); break; }
            if (!heading) { heading = malloc(16); snprintf(heading, 16, "Row %d", idx); }
            data_cells = cells; ndata = nc;
        }
        /* pad/truncate data_cells to headers length */
        if (ndata < nh) { /* extend logically */ }
        /* build group */
        size_t need = strlen(heading) + 32;
        for (int k = 0; k < nh; k++) {
            const char *val = (k < ndata) ? data_cells[k] : "";
            if (!has_row_label && val[0] && strcmp(val, heading) == 0) continue;
            need += strlen("• : ") + strlen(headers[k]) + strlen(val) + 4;
        }
        if (len + need + 8 >= cap) { cap = len + need + 1024; out = realloc(out, cap); }
        len += (size_t)snprintf(out + len, cap - len, "**%s**\n", heading);
        for (int k = 0; k < nh; k++) {
            const char *val = (k < ndata) ? data_cells[k] : "";
            if (!has_row_label && val[0] && strcmp(val, heading) == 0) continue;
            len += (size_t)snprintf(out + len, cap - len, "• %s: %s\n", headers[k], val);
        }
        if (idx + 1 < nblock - 1) { len += (size_t)snprintf(out + len, cap - len, "\n"); }
        free(heading);
        for (int k = 0; k < nc; k++) free(cells[k]);
        free(cells);
    }
    for (int i = 0; i < nh; i++) free(headers[i]);
    free(headers);
    if (len > 0 && out[len - 1] == '\n') out[--len] = '\0';
    return out;
}

char *gateway_platforms_helpers_convert_table_to_bullets(const char *text)
{
    if (!text) return NULL;
    if (!strchr(text, '|') || !strchr(text, '-')) return strdup(text);
    /* split into lines */
    char *buf = strdup(text);
    int nlines = 1;
    for (char *p = buf; *p; p++) if (*p == '\n') nlines++;
    char **lines = malloc(sizeof(char *) * (nlines + 1));
    int n = 0;
    char *p = buf, *start = buf;
    while (1) {
        if (*p == '\n' || *p == '\0') {
            size_t l = (size_t)(p - start);
            char *ln = malloc(l + 1); memcpy(ln, start, l); ln[l] = '\0';
            lines[n++] = ln;
            if (*p == '\0') break;
            start = p + 1;
        }
        p++;
    }
    lines[n] = NULL;
    size_t cap = strlen(text) + 256, len = 0;
    char *out = malloc(cap);
    out[0] = '\0';
    bool in_fence = false;
    for (int i = 0; i < n; i++) {
        char *ln = lines[i];
        /* detect fence: leading whitespace + ``` */
        char *q = ln; while (*q == ' ' || *q == '\t') q++;
        if (strncmp(q, "```", 3) == 0) { in_fence = !in_fence; goto emit; }
        if (in_fence) goto emit;
        if (strchr(ln, '|') && i + 1 < n && is_table_separator_line(lines[i + 1])) {
            /* gather table block: count trailing consecutive table rows first */
            int j = i + 2;
            while (j < n && gateway_platforms_helpers_is_table_row(lines[j])) j++;
            int block_n = (j - i);            /* header + sep + data rows */
            char **block = malloc(sizeof(char *) * (block_n + 1));
            int bn = 0;
            block[bn++] = ln;
            block[bn++] = lines[i + 1];
            for (int k = i + 2; k < j; k++) block[bn++] = lines[k];
            block[bn] = NULL;
            char *rendered = gateway_platforms_helpers_render_table_block(block, bn);
            size_t rl = strlen(rendered);
            if (len + rl + 2 >= cap) { cap = len + rl + 256; out = realloc(out, cap); }
            strcat(out + len, rendered); len += rl;
            free(rendered);
            free(block);
            i = j - 1;
            continue;
        }
emit:
        size_t ll = strlen(ln);
        if (len + ll + 2 >= cap) { cap = len + ll + 256; out = realloc(out, cap); }
        strcat(out + len, ln); len += ll;
        if (i + 1 < n) { strcat(out + len, "\n"); len += 1; }
    }
    for (int i = 0; i < n; i++) free(lines[i]);
    free(lines);
    free(buf);
    return out;
}

/* ─── Mention pattern compilation ────────────────────────────────────── */

/* Compiled mention pattern list (port of Python list[re.Pattern]). */
typedef struct {
    void **patterns;   /* hregex_t* elements */
    size_t count;
    size_t cap;
} mention_patterns_t;

mention_patterns_t *gateway_platforms_helpers_compile_mention_patterns(
    const char *raw_json,       /* JSON list of pattern strings, or NULL */
    const char *log_prefix,     /* e.g. "[bluebubbles]" */
    const char *platform_label, /* config-style label, or NULL for wakeword */
    const char *defaults_json
) {
    mention_patterns_t *mp = calloc(1, sizeof(*mp));
    if (!mp) return NULL;

    /* Gather the raw list of pattern strings. */
    char **collected = NULL;
    size_t collected_n = 0, collected_cap = 0;

    if (platform_label != NULL) {
        /* Config-style (dingtalk/telegram). */
        if (!raw_json) goto done;
        json_t *arr = json_parse(raw_json, NULL);
        if (arr && json_is_array(arr)) {
            for (size_t i = 0; i < json_array_size(arr); i++) {
                const char *s = json_get_str(json_array_get(arr, i), "", "");
                if (s && *s) {
                    if (collected_n + 1 > collected_cap) {
                        collected_cap = collected_cap ? collected_cap * 2 : 8;
                        collected = realloc(collected, collected_cap * sizeof(char *));
                    }
                    collected[collected_n++] = strdup(s);
                }
            }
        }
        json_free(arr);
    } else {
        /* Wakeword-style (photon/bluebubbles). */
        if (!raw_json) {
            if (defaults_json) {
                json_t *arr = json_parse(defaults_json, NULL);
                if (arr && json_is_array(arr)) {
                    for (size_t i = 0; i < json_array_size(arr); i++) {
                        const char *s = json_get_str(json_array_get(arr, i), "", "");
                        if (s) {
                            if (collected_n + 1 > collected_cap) {
                                collected_cap = collected_cap ? collected_cap * 2 : 8;
                                collected = realloc(collected, collected_cap * sizeof(char *));
                            }
                            collected[collected_n++] = strdup(s);
                        }
                    }
                }
                json_free(arr);
            }
        } else if (*raw_json) {
            /* Try JSON list parse first; fall back to comma/newline split. */
            json_t *arr = json_parse(raw_json, NULL);
            if (arr && json_is_array(arr)) {
                for (size_t i = 0; i < json_array_size(arr); i++) {
                    const char *s = json_get_str(json_array_get(arr, i), "", "");
                    if (s) {
                        if (collected_n + 1 > collected_cap) {
                            collected_cap = collected_cap ? collected_cap * 2 : 8;
                            collected = realloc(collected, collected_cap * sizeof(char *));
                        }
                        collected[collected_n++] = strdup(s);
                    }
                }
            } else {
                /* Comma/newline split */
                char *copy = strdup(raw_json);
                char *saveptr = NULL;
                char *line = strtok_r(copy, "\n", &saveptr);
                while (line) {
                    char *part_save = NULL;
                    char *part = strtok_r(line, ",", &part_save);
                    while (part) {
                        char *t = part;
                        while (*t == ' ' || *t == '\t') t++;
                        char *e = t + strlen(t);
                        while (e > t && (e[-1] == ' ' || e[-1] == '\t')) e--;
                        if (e > t) {
                            if (collected_n + 1 > collected_cap) {
                                collected_cap = collected_cap ? collected_cap * 2 : 8;
                                collected = realloc(collected, collected_cap * sizeof(char *));
                            }
                            collected[collected_n++] = strndup(t, (size_t)(e - t));
                        }
                        part = strtok_r(NULL, ",", &part_save);
                    }
                    line = strtok_r(NULL, "\n", &saveptr);
                }
                free(copy);
            }
            json_free(arr);
        } else {
            /* Scalar wrapped in a list */
            if (collected_n + 1 > collected_cap) {
                collected_cap = 8;
                collected = realloc(collected, collected_cap * sizeof(char *));
            }
            collected[collected_n++] = strdup(raw_json);
        }
    }

done:
    /* Compile each pattern with case-insensitive flag (re.IGNORECASE). */
    for (size_t i = 0; i < collected_n; i++) {
        char *text = collected[i];
        char *t = text;
        while (*t == ' ' || *t == '\t') t++;
        if (!*t) { free(collected[i]); continue; }
        hregex_t *re = regex_compile(text, 1);  /* 1 = IGNORECASE */
        if (re) {
            if (mp->count + 1 > mp->cap) {
                mp->cap = mp->cap ? mp->cap * 2 : 8;
                mp->patterns = realloc(mp->patterns, mp->cap * sizeof(void *));
            }
            mp->patterns[mp->count++] = re;
        }
    }
    for (size_t i = 0; i < collected_n; i++) free(collected[i]);
    free(collected);

    if (mp->count > 0 && log_prefix) {
        /* Log the count — mirrors Python info log. */
        hermes_log(LOG_INFO, (log_prefix && *log_prefix) ? log_prefix + 1 : "helpers",
                   "Loaded %zu mention pattern(s)", mp->count);
    }
    return mp;
}

/* Free a mention_patterns_t. */
void gateway_platforms_helpers_free_mention_patterns(mention_patterns_t *mp) {
    if (!mp) return;
    for (size_t i = 0; i < mp->count; i++) regex_free((hregex_t *)mp->patterns[i]);
    free(mp->patterns);
    free(mp);
}

/* Match text against compiled patterns. Returns 1 if any matches. */
int gateway_platforms_helpers_match_mentions(const mention_patterns_t *mp, const char *text) {
    if (!mp || !text) return 0;
    for (size_t i = 0; i < mp->count; i++) {
        regex_match_t *m = regex_match((hregex_t *)mp->patterns[i], text);
        if (m) {
            regex_match_free(m);
            return 1;
        }
    }
    return 0;
}
