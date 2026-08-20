/*
 * shell_heredoc.c — Conservative heredoc masking for shell-command scanners.
 * PoP: shell_heredoc @ tools/shell_heredoc.py
 * Faithful port of tools/shell_heredoc.py.
 */

#include "shell_heredoc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

/* Non-shell interpreters whose (quoted, inert) heredoc bodies are safe to
 * mask. Faithful port of _INERT_HEREDOC_CONSUMER_RE. Implemented as a direct
 * scanner (POSIX ERE cannot express this pattern without catastrophic
 * backtracking in glibc's regexec). */
static bool inert_consumer_match(const char *s) {
    size_t i = 0;
    while (s[i]) {
        if (s[i] == ' ' || s[i] == '\t') { i++; continue; }
        /* optional VAR=val assignments: [A-Z_][A-Z0-9_]*=non-space then space */
        if (s[i] >= 'A' && s[i] <= 'Z') {
            size_t j = i + 1;
            while (s[j] && ((s[j] >= 'A' && s[j] <= 'Z') || (s[j] >= '0' && s[j] <= '9') || s[j] == '_')) j++;
            if (s[j] == '=' && s[j+1] && s[j+1] != ' ' && s[j+1] != '\t' && s[j+1] != '\n') {
                /* consume value up to whitespace */
                j++;
                while (s[j] && s[j] != ' ' && s[j] != '\t' && s[j] != '\n') j++;
                if (s[j] == ' ' || s[j] == '\t') { i = j + 1; continue; }
            }
            /* not an assignment; fall through to consumer check */
        }
        /* optional env prefix */
        if (strncmp(s + i, "env", 3) == 0 && (s[i+3] == ' ' || s[i+3] == '\t')) { i += 4; continue; }
        /* optional path prefix token/ */
        if (strchr(s + i, '/')) {
            const char *slash = strchr(s + i, '/');
            i = (size_t)(slash - s) + 1;
            continue;
        }
        /* consumer token */
        size_t tok_start = i;
        while (s[i] && s[i] != ' ' && s[i] != '\t' && s[i] != '\n') i++;
        size_t tok_len = i - tok_start;
        const char *tok = s + tok_start;
        bool is_consumer = false;
        if (tok_len >= 3 && strncmp(tok, "cat", tok_len) == 0) is_consumer = true;
        else if (tok_len >= 8 && strncmp(tok, "osascript", tok_len) == 0) is_consumer = true;
        else if (tok_len >= 6 && strncmp(tok, "python", tok_len) == 0) is_consumer = true;
        else if (tok_len >= 7 && strncmp(tok, "python3", 7) == 0) {
            /* python3 or python3.N */
            if (tok_len == 7) is_consumer = true;
            else if (tok[7] == '.' && tok_len > 8) {
                bool ok = true;
                for (size_t k = 8; k < tok_len; k++) if (s[k] < '0' || s[k] > '9') { ok = false; break; }
                is_consumer = ok;
            }
        }
        if (!is_consumer) return false;
        /* must be followed by whitespace or end */
        return (s[i] == ' ' || s[i] == '\t' || s[i] == '\0' || s[i] == '\n');
    }
    return false;
}

/* local strdup (avoids feature-macro ordering issues) */
static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *d = malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n + 1);
    return d;
}

/* ---- helpers ---- */

static bool str_has(const char *s, const char *needle) {
    return s && strstr(s, needle) != NULL;
}

/* PoP: _mask_simple_quotes @ tools/shell_heredoc.py:_mask_simple_quotes */
/* ---- _mask_simple_quotes ---- */
static char *mask_simple_quotes(const char *command) {
    size_t n = strlen(command);
    /* worst-case expansion: result never longer than 2*n + small */
    char *result = malloc(n * 2 + 1);
    if (!result) return NULL;
    size_t ri = 0;
    size_t cursor = 0;
    while (cursor < n) {
        char ch = command[cursor];
        if (ch == '\'') {
            size_t closing = cursor + 1;
            while (closing < n && command[closing] != '\'') closing++;
            if (closing >= n) { /* unterminated */
                memcpy(result + ri, command + cursor, n - cursor);
                ri += n - cursor;
                break;
            }
            result[ri++] = '\'';
            result[ri++] = '\'';
            cursor = closing + 1;
            continue;
        }
        if (ch == '"') {
            size_t end = cursor + 1;
            while (end < n) {
                if (command[end] == '\\' && end + 1 < n) { end += 2; continue; }
                if (command[end] == '"') { end++; break; }
                end++;
            }
            if (end > n || command[end - 1] != '"') { /* unterminated */
                memcpy(result + ri, command + cursor, n - cursor);
                ri += n - cursor;
                break;
            }
            size_t seglen = end - cursor;
            char *segment = malloc(seglen + 1);
            if (!segment) { free(result); return NULL; }
            memcpy(segment, command + cursor, seglen);
            segment[seglen] = '\0';
            if (str_has(segment, "$(") || str_has(segment, "`")) {
                memcpy(result + ri, segment, seglen);
                ri += seglen;
            } else {
                result[ri++] = '"';
                result[ri++] = '"';
            }
            free(segment);
            cursor = end;
            continue;
        }
        if (ch == '`') {
            size_t end = cursor + 1;
            while (end < n) {
                if (command[end] == '\\' && end + 1 < n) { end += 2; continue; }
                if (command[end] == '`') { end++; break; }
                end++;
            }
            size_t seglen = end - cursor;
            memcpy(result + ri, command + cursor, seglen);
            ri += seglen;
            cursor = end;
            continue;
        }
        result[ri++] = ch;
        cursor++;
    }
    result[ri] = '\0';
    return result;
}

/* PoP: _contains_nested_shell_scope @ tools/shell_heredoc.py:_contains_nested_shell_scope */
/* ---- _contains_nested_shell_scope ---- */
static bool contains_nested_shell_scope(const char *masked_opener) {
    return str_has(masked_opener, "$(") || str_has(masked_opener, "`") ||
           str_has(masked_opener, "<(") || str_has(masked_opener, ">(");
}

typedef struct {
    char  *delimiter;   /* heap-allocated */
    bool   strip_tabs;
    bool   quoted;
} heredoc_spec_t;

/* PoP: parse_heredoc_operator @ tools/shell_heredoc.py:_parse_heredoc_operator */
/* ---- _parse_heredoc_operator ----
 * Returns true and fills *out_end, *out_delimiter (heap), *out_strip_tabs,
 * *out_quoted on success; false when not a well-formed opener. */
static bool parse_heredoc_operator(const char *command, size_t index,
                                    size_t *out_end, char **out_delimiter,
                                    bool *out_strip_tabs, bool *out_quoted) {
    size_t n = strlen(command);
    if (index + 2 > n) return false;
    if (strncmp(command + index, "<<", 2) != 0) return false;
    if (index + 3 <= n && strncmp(command + index, "<<<", 3) == 0) return false;

    size_t cursor = index + 2;
    bool strip_tabs = false;
    if (cursor < n && command[cursor] == '-') { strip_tabs = true; cursor++; }
    while (cursor < n && (command[cursor] == ' ' || command[cursor] == '\t')) cursor++;
    if (cursor >= n || command[cursor] == '\r' || command[cursor] == '\n') return false;

    char *delimiter = malloc(n + 1);
    if (!delimiter) return false;
    size_t dlen = 0;
    bool quoted = false;

    while (cursor < n) {
        char ch = command[cursor];
        if (isspace((unsigned char)ch) || ch == ';' || ch == '|' ||
            ch == '<' || ch == '>' || ch == '(' || ch == ')') {
            break;
        }
        if (ch == '\\') {
            if (cursor + 1 >= n || command[cursor + 1] == '\r' || command[cursor + 1] == '\n') {
                free(delimiter); return false;
            }
            quoted = true;
            delimiter[dlen++] = command[cursor + 1];
            cursor += 2;
            continue;
        }
        if (ch == '\'' || ch == '"') {
            quoted = true;
            char quote = ch;
            cursor++;
            while (cursor < n && command[cursor] != quote) {
                if (quote == '"' && command[cursor] == '\\') {
                    if (cursor + 1 >= n) { free(delimiter); return false; }
                    char following = command[cursor + 1];
                    if (following == '$' || following == '`' || following == '"' ||
                        following == '\\' || following == '\n') {
                        delimiter[dlen++] = following;
                        cursor += 2;
                        continue;
                    }
                    delimiter[dlen++] = '\\';
                    cursor++;
                    continue;
                }
                if (command[cursor] == '\r' || command[cursor] == '\n') {
                    free(delimiter); return false;
                }
                delimiter[dlen++] = command[cursor];
                cursor++;
            }
            if (cursor >= n) { free(delimiter); return false; }
            cursor++;
            continue;
        }
        delimiter[dlen++] = ch;
        cursor++;
    }

    if (dlen == 0 && !quoted) { free(delimiter); return false; }
    delimiter[dlen] = '\0';
    *out_end = cursor;
    *out_delimiter = delimiter;
    *out_strip_tabs = strip_tabs;
    *out_quoted = quoted;
    return true;
}

/* PoP: scan_heredoc_command_unit @ tools/shell_heredoc.py:_scan_heredoc_command_unit */
/* ---- _scan_heredoc_command_unit ----
 * Scans one logical shell command, ignoring markers in quotes/comments.
 * Fills *out_end, specs array, *out_unknown_operator, *out_has_list_operator.
 * specs is heap-allocated (caller frees via free_specs). */
static void free_specs(heredoc_spec_t *specs, size_t count) {
    for (size_t i = 0; i < count; i++) free(specs[i].delimiter);
    free(specs);
}

static void scan_heredoc_command_unit(const char *command, size_t start,
                                       size_t *out_end, heredoc_spec_t **out_specs,
                                       size_t *out_spec_count, bool *out_unknown,
                                       bool *out_has_list) {
    size_t n = strlen(command);
    size_t cursor = start;
    int quote = 0;        /* 0 = none, else the quote char */
    bool comment = false;
    heredoc_spec_t *specs = NULL;
    size_t spec_cap = 0, spec_count = 0;
    bool unknown = false, has_list = false;

    while (cursor < n) {
        char ch = command[cursor];
        if (comment) {
            if (ch == '\n') { *out_end = cursor; goto done; }
            cursor++; continue;
        }
        if (quote != 0) {
            if (quote != '\'' && ch == '\\' && cursor + 1 < n) { cursor += 2; continue; }
            if (ch == quote) quote = 0;
            cursor++; continue;
        }
        if (ch == '\\' && cursor + 1 < n) { cursor += 2; continue; }
        if (ch == '\'' || ch == '"' || ch == '`') { quote = ch; cursor++; continue; }
        if (ch == '#') {
            char prev = (cursor > start) ? command[cursor - 1] : '\0';
            if (cursor == start || isspace((unsigned char)prev) || prev == ';' ||
                prev == '|' || prev == '(' || prev == ')') {
                comment = true; cursor++; continue;
            }
        }
        if (ch == '\n') { *out_end = cursor; goto done; }
        if (cursor + 3 <= n && strncmp(command + cursor, "<<<", 3) == 0) { cursor += 3; continue; }
        if (cursor + 2 <= n && strncmp(command + cursor, "<<", 2) == 0) {
            size_t end; char *delim; bool st, q;
            if (!parse_heredoc_operator(command, cursor, &end, &delim, &st, &q)) {
                unknown = true;
                cursor += 2;
                continue;
            }
            if (spec_count == spec_cap) {
                size_t nc = spec_cap ? spec_cap * 2 : 4;
                heredoc_spec_t *ns = realloc(specs, nc * sizeof(*ns));
                if (!ns) { free_specs(specs, spec_count); *out_specs = NULL; *out_spec_count = 0; return; }
                specs = ns; spec_cap = nc;
            }
            specs[spec_count].delimiter = delim;
            specs[spec_count].strip_tabs = st;
            specs[spec_count].quoted = q;
            spec_count++;
            cursor = end;
            continue;
        }
        if (ch == ';' || ch == '|' || ch == '&') has_list = true;
        cursor++;
    }
    *out_end = n;
    goto done;

done:
    *out_specs = specs;
    *out_spec_count = spec_count;
    *out_unknown = unknown;
    *out_has_list = has_list;
}

/* PoP: find_heredoc_close @ tools/shell_heredoc.py:_find_heredoc_close */
/* ---- _find_heredoc_close ----
 * Returns position after an exact terminator line, or SIZE_MAX if unterminated. */
static size_t find_heredoc_close(const char *command, size_t body_start,
                                 const char *delimiter, bool strip_tabs) {
    size_t n = strlen(command);
    size_t cursor = body_start;
    while (1) {
        size_t newline = cursor;
        while (newline < n && command[newline] != '\n') newline++;
        size_t after;
        size_t line_end = (newline < n) ? newline : n;
        /* line = command[cursor:line_end] */
        size_t line_len = line_end - cursor;
        char *line = malloc(line_len + 1);
        if (!line) return SIZE_MAX;
        memcpy(line, command + cursor, line_len);
        line[line_len] = '\0';
        if (line_len > 0 && line[line_len - 1] == '\r') line[line_len - 1] = '\0';
        char *candidate = line;
        if (strip_tabs) {
            while (*candidate == '\t') candidate++;
        }
        bool match = (strcmp(candidate, delimiter) == 0);
        after = (newline < n) ? (newline + 1) : n;
        free(line);
        if (match) return after;
        if (newline >= n) return SIZE_MAX;  /* unterminated */
        cursor = after;
    }
}

/* ---- strip_inert_heredoc_bodies ---- */

typedef struct { size_t start; size_t end; } range_t;

char *strip_inert_heredoc_bodies(const char *command) {
    if (!command) return NULL;
    size_t n = strlen(command);
    if (!strstr(command, "<<")) {
        char *dup = malloc(n + 1);
        if (!dup) return NULL;
        memcpy(dup, command, n + 1);
        return dup;
    }
    /* last_opener_index */
    size_t last_opener = SIZE_MAX;
    for (size_t i = 0; i + 1 < n; i++) {
        if (command[i] == '<' && command[i + 1] == '<') last_opener = i;
    }

    range_t *ranges = NULL;
    size_t range_cap = 0, range_count = 0;
    size_t command_start = 0;

    while (command_start < n) {
        if (command_start > last_opener) break;
        size_t command_end; heredoc_spec_t *specs; size_t spec_count;
        bool unknown, has_list;
        scan_heredoc_command_unit(command, command_start, &command_end, &specs, &spec_count,
                                   &unknown, &has_list);
        if (unknown) { free_specs(specs, spec_count); free(ranges); return xstrdup(command); }
        if (spec_count == 0) {
            if (command_end >= n) break;
            command_start = command_end + 1;
            continue;
        }
        if (command_end >= n) { free_specs(specs, spec_count); free(ranges); return xstrdup(command); }

        size_t body_cursor = command_end + 1;
        bool unterminated = false;
        range_t *body_ranges = malloc((spec_count ? spec_count : 1) * sizeof(range_t));
        if (!body_ranges) { free_specs(specs, spec_count); free(ranges); return NULL; }
        size_t br_count = 0;
        for (size_t i = 0; i < spec_count; i++) {
            size_t close_end = find_heredoc_close(command, body_cursor,
                                                  specs[i].delimiter, specs[i].strip_tabs);
            if (close_end == SIZE_MAX) { unterminated = true; break; }
            body_ranges[br_count].start = body_cursor;
            body_ranges[br_count].end = close_end;
            br_count++;
            body_cursor = close_end;
        }
        if (unterminated) {
            free_specs(specs, spec_count);
            free(body_ranges);
            free(ranges);
            return xstrdup(command);
        }

        /* all quoted and no list operator? */
        bool all_quoted = true;
        for (size_t i = 0; i < spec_count; i++) if (!specs[i].quoted) { all_quoted = false; break; }
        if (all_quoted && !has_list) {
            /* Python masks only command[command_start:command_end] */
            size_t opener_len = command_end - command_start;
            char *opener_buf = malloc(opener_len + 1);
            if (!opener_buf) { free_specs(specs, spec_count); free(body_ranges); free(ranges); return NULL; }
            memcpy(opener_buf, command + command_start, opener_len);
            opener_buf[opener_len] = '\0';
            char *masked_opener = mask_simple_quotes(opener_buf);
            free(opener_buf);
            if (!masked_opener) { free_specs(specs, spec_count); free(body_ranges); free(ranges); return NULL; }
            bool ok = inert_consumer_match(masked_opener) &&
                      !contains_nested_shell_scope(masked_opener);
            free(masked_opener);
            if (ok) {
                for (size_t i = 0; i < br_count; i++) {
                    if (range_count == range_cap) {
                        size_t nc = range_cap ? range_cap * 2 : 8;
                        range_t *nr = realloc(ranges, nc * sizeof(*nr));
                        if (!nr) { free_specs(specs, spec_count); free(body_ranges); free(ranges); return NULL; }
                        ranges = nr; range_cap = nc;
                    }
                    ranges[range_count++] = body_ranges[i];
                }
            }
        }
        free_specs(specs, spec_count);
        free(body_ranges);
        command_start = body_cursor;
    }

    if (range_count == 0) {
        free(ranges);
        return xstrdup(command);
    }

    /* rebuild: replace each range with newline-preserving text */
    /* count newlines in each range */
    char *result = malloc(n + 1);
    if (!result) { free(ranges); return NULL; }
    size_t ri = 0, prev = 0;
    for (size_t i = 0; i < range_count; i++) {
        size_t s = ranges[i].start, e = ranges[i].end;
        memcpy(result + ri, command + prev, s - prev);
        ri += s - prev;
        size_t nl = 0;
        for (size_t k = s; k < e && k < n; k++) if (command[k] == '\n') nl++;
        for (size_t k = 0; k < nl; k++) result[ri++] = '\n';
        prev = e;
    }
    memcpy(result + ri, command + prev, n - prev);
    ri += n - prev;
    result[ri] = '\0';
    free(ranges);
    return result;
}
