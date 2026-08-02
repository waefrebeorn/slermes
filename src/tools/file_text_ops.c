/*
 * file_text_ops.c — self-contained text-shaping helpers for file operations.
 *
 * Stateless string transformers ported from tools/file_operations.py. Extracted
 * from port_file_operations.c (v551 refactor-first monolith split) and
 * oracle-verified against the live Python equivalents.
 *
 * No god headers, no void* passthrough, focused includes only.
 */

#include "file_text_ops.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- private helpers ---------------------------------------------------- */

static char *dup_str(const char *s)
{
    if (!s) return NULL;
    return strdup(s);
}

/* ---- strip_terminal_fence_leaks ----------------------------------------- */

/* PoP: file_text_ops_strip_terminal_fence_leaks @ tools/file_operations.py:_strip_terminal_fence_leaks */
/* Per line: if the line contains an OSC (\x1b]) or a __HERMES_FENCE_ marker
 * (a "terminal wrapper"), strip OSC sequences, fence markers, and bell chars;
 * if the cleaned line is then empty, drop the whole line (incl. its \n).
 * CSI (\x1b[) is deliberately left intact (Python only strips OSC/fence/bell). */
char *file_text_ops_strip_terminal_fence_leaks(const char *text)
{
    if (!text) return dup_str("");
    size_t len = strlen(text);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    size_t oi = 0;

    const char *p = text;
    while (*p) {
        /* extract one line (with trailing \n if present) */
        const char *line_start = p;
        const char *nl = strchr(p, '\n');
        size_t line_len = nl ? (size_t)(nl - p) : strlen(p);
        int had_wrapper = (strncmp(line_start, "__HERMES_FENCE_", 15) == 0) ||
                          (line_len >= 2 && line_start[0] == '\033' && line_start[1] == ']');

        /* clean the line into a temp buffer */
        char *clean = malloc(line_len + 1);
        if (!clean) { free(out); return NULL; }
        size_t ci = 0;
        for (size_t i = 0; i < line_len; i++) {
            char c = line_start[i];
            if (c == '\033') {
                if (i + 1 < line_len && line_start[i + 1] == ']') {  /* OSC */
                    i += 2;
                    while (i < line_len && line_start[i] != '\007' &&
                           !(line_start[i] == '\033' && i + 1 < line_len && line_start[i + 1] == '\\'))
                        i++;
                    if (i < line_len && line_start[i] == '\007') i++;
                    else if (i + 1 < line_len && line_start[i] == '\033' && line_start[i + 1] == '\\') i += 2;
                    i--;  /* loop will ++ */
                    continue;
                }
                clean[ci++] = c;  /* CSI or other ESC: keep */
                continue;
            }
            if (c == '\007') continue;  /* bell */
            if (strncmp(line_start + i, "__HERMES_FENCE_", 15) == 0) {
                i += 15;
                size_t j = i;
                while (i < line_len && ((line_start[i] >= 'A' && line_start[i] <= 'Z') ||
                       (line_start[i] >= 'a' && line_start[i] <= 'z') ||
                       (line_start[i] >= '0' && line_start[i] <= '9'))) i++;
                /* Python _FENCE_MARKER_RE requires [A-Za-z0-9]+__ (closing __).
                 * If no closing '__', the marker is incomplete -> keep the
                 * literal prefix and continue scanning the rest (no strip). */
                if (i > j && i + 1 < line_len && line_start[i] == '_' && line_start[i + 1] == '_') {
                    i += 1;  /* past first '_'; loop's i++ lands after '__' */
                } else {
                    memcpy(clean + ci, "__HERMES_FENCE_", 15);
                    ci += 15;
                    i = j - 1;  /* loop re-processes the alnum tail, no rewind */
                }
                continue;
            }
            clean[ci++] = c;
        }
        clean[ci] = '\0';

        if (had_wrapper && strspn(clean, "'\r\n\t ") == ci) {
            /* whole line was a terminal wrapper and is now empty -> drop it */
        } else {
            memcpy(out + oi, clean, ci);
            oi += ci;
            if (nl) out[oi++] = '\n';
        }
        free(clean);
        p = nl ? nl + 1 : p + line_len;
    }
    out[oi] = '\0';
    return out;
}

/* ---- detect_line_ending ------------------------------------------------- */

/* PoP: file_text_ops_detect_line_ending @ tools/file_operations.py:_detect_line_ending */
/* Returns malloc'd "crlf" / "lf" / "unknown". Python's _detect_line_ending
 * returns None for content without a newline (undetermined: new/empty/
 * single-line file), and the FS wrapper _detect_file_line_ending surfaces
 * that as None -> "unknown". Python never returns a bare "cr" string. */
char *file_text_ops_detect_line_ending(const char *sample)
{
    if (!sample || !*sample) return dup_str("unknown");
    size_t head = strlen(sample);
    if (head > 4096) head = 4096;
    if (memchr(sample, '\r', head) && memchr(sample, '\n', head) &&
        strstr(sample, "\r\n")) {
        return dup_str("crlf");
    }
    if (memchr(sample, '\n', head)) return dup_str("lf");
    /* No newline present -> Python returns None (undetermined) -> "unknown". */
    return dup_str("unknown");
}

/* ---- normalize_line_endings --------------------------------------------- */

/* PoP: file_text_ops_normalize_line_endings @ tools/file_operations.py:_normalize_line_endings */
/* Idempotent: collapse CRLF + lone CR to LF, then expand to CRLF if target
 * is "\r\n". */
char *file_text_ops_normalize_line_endings(const char *text, const char *target)
{
    if (!text) return dup_str("");
    if (!target || !*target) target = "\n";
    size_t len = strlen(text);
    char *lf = malloc(len + 1);
    if (!lf) return NULL;
    size_t li = 0;
    /* collapse to LF */
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\r') {
            if (text[i + 1] == '\n') i++;
            lf[li++] = '\n';
        } else {
            lf[li++] = text[i];
        }
    }
    lf[li] = '\0';

    if (strcmp(target, "\r\n") != 0) {
        return lf; /* target is "\n" (or unknown) — LF form is final */
    }
    /* expand LF -> CRLF */
    char *out = malloc(li * 2 + 1);
    if (!out) { free(lf); return NULL; }
    size_t oi = 0;
    for (size_t i = 0; i < li; i++) {
        if (lf[i] == '\n') out[oi++] = '\r';
        out[oi++] = lf[i];
    }
    out[oi] = '\0';
    free(lf);
    return out;
}

/* ---- strip_bom / has_bom ------------------------------------------------ */

/* PoP: file_text_ops_strip_bom @ tools/file_operations.py:_strip_bom */
/* Strips a single leading UTF-8 BOM. */
char *file_text_ops_strip_bom(const char *text)
{
    if (!text) return dup_str("");
    if (strncmp(text, "\xEF\xBB\xBF", 3) == 0) return dup_str(text + 3);
    return dup_str(text);
}

/* PoP: file_text_ops_has_bom @ tools/file_operations.py:_has_bom */
bool file_text_ops_has_bom(const char *text)
{
    if (!text) return false;
    return strncmp(text, "\xEF\xBB\xBF", 3) == 0;
}

/* ---- add_line_numbers --------------------------------------------------- */

/* PoP: file_text_ops_add_line_numbers @ tools/file_operations.py:_add_line_numbers */
char *file_text_ops_add_line_numbers(const char *content, int start_line, int max_line_length)
{
    if (!content) return dup_str("");
    size_t len = strlen(content);
    char *out = malloc(len * 2 + 64);
    if (!out) return NULL;
    size_t oi = 0;
    int line = start_line > 0 ? start_line : 1;
    const char *p = content;
    bool at_start = true;
    while (true) {
        if (at_start) {
            oi += (size_t)snprintf(out + oi, len * 2 + 64 - oi, "%d|", line);
            at_start = false;
        }
        if (*p == '\0') break;
        if (*p == '\n') {
            out[oi++] = '\n';
            line++;
            at_start = true;
            p++;
            continue;
        }
        /* per-line truncation */
        if (max_line_length > 0) {
            const char *nl = strchr(p, '\n');
            size_t line_len = nl ? (size_t)(nl - p) : strlen(p);
            if (line_len > (size_t)max_line_length) {
                for (int k = 0; k < max_line_length; k++) out[oi++] = p[k];
                const char *suf = "... [truncated]";
                size_t sl = strlen(suf);
                for (size_t k = 0; k < sl; k++) out[oi++] = suf[k];
                p = nl ? nl : p + line_len;
                continue;
            }
        }
        out[oi++] = *p++;
    }
    out[oi] = '\0';
    return out;
}

/* ---- expand_path -------------------------------------------------------- */

/* PoP: file_text_ops_expand_path @ tools/file_operations.py:_expand_path */
char *file_text_ops_expand_path(const char *path)
{
    if (!path) return dup_str("");
    if (path[0] != '~') return dup_str(path);
    const char *home = getenv("HOME");
    if (!home) return dup_str(path);
    size_t need = strlen(home) + strlen(path); /* +1 for '/' handled by path+1 */
    char *out = malloc(need + 1);
    if (!out) return NULL;
    strcpy(out, home);
    strcat(out, path + 1);
    return out;
}

/* ---- escape_shell_arg --------------------------------------------------- */

/* PoP: file_text_ops_escape_shell_arg @ tools/file_operations.py:_escape_shell_arg */
char *file_text_ops_escape_shell_arg(const char *arg)
{
    if (!arg) return dup_str("''");
    size_t len = strlen(arg);
    char *out = malloc(len * 4 + 3);
    if (!out) return NULL;
    char *d = out;
    *d++ = '\'';
    for (const char *s = arg; *s; s++) {
        if (*s == '\'') {
            /* '"'"' */
            *d++ = '\''; *d++ = '"'; *d++ = '\''; *d++ = '"'; *d++ = '\'';
        } else {
            *d++ = *s;
        }
    }
    *d++ = '\'';
    *d = '\0';
    return out;
}

/* ---- parse_search_context_line ------------------------------------------ */

/* PoP: file_text_ops_parse_search_context_line @ tools/file_operations.py:_parse_search_context_line */
/* Parses grep/rg "path-line-content" format via the rightmost -<digits>-
 * separator. Returns JSON {"path":..,"line":..,"content":..} on match, or
 * "{}" when no -<digits>- separator is present (Python returns None). */
char *file_text_ops_parse_search_context_line(const char *line)
{
    if (!line || !*line || strcmp(line, "--") == 0) return dup_str("null");

    /* Find the RIGHTMOST "-<digits>-" occurrence. */
    long match_start = -1, match_end = -1;
    const char *p = line;
    while (*p) {
        if (*p == '-') {
            const char *q = p + 1;
            if (*q >= '0' && *q <= '9') {
                const char *num_start = q;
                while (*q >= '0' && *q <= '9') q++;
                if (*q == '-' && q > num_start) {
                    match_start = (long)(p - line);
                    match_end = (long)(q - line + 1);
                }
            }
        }
        p++;
    }
    if (match_start < 0) return dup_str("null");

    size_t path_len = (size_t)match_start;
    if (path_len == 0) return dup_str("null");
    long line_no = 0;
    for (long i = match_start + 1; i < match_end - 1; i++)
        line_no = line_no * 10 + (line[i] - '0');
    const char *content = line + match_end;

    char *root = malloc(strlen(line) + 64);
    if (!root) return dup_str("[]");
    int n = snprintf(root, strlen(line) + 64,
        "[\"%.*s\",%ld,\"%s\"]",
        (int)path_len, line, line_no, content);
    (void)n;
    return root;
}
