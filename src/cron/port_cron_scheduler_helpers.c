/*
 * port_cron_scheduler_helpers.c
 *
 * Pure, portable helpers ported from cron/scheduler.py. These are the string
 * classification/normalization helpers that do NOT touch the file lock,
 * subprocess, asyncio loop, or delivery adapters:
 *   - _is_cron_silence_response  (detect [SILENT]/NO_REPLY sentinels)
 *   - _normalize_deliver_value   (flatten list/tuple -> csv string, ""->local)
 *
 * The IO/process-coupled functions (tick, run_job, _build_job_prompt,
 * _deliver_result, _summarize_cron_failure_for_delivery which writes a JSON
 * blob) are NOT ported here.
 *
 * Module prefix used by the scanner for cron/scheduler.py is "scheduler_".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Sentinel tokens (UPPER, space-collapsed): matches Python frozenset. */
static const char *CRON_SILENCE_TOKENS[] = {
    "[SILENT]", "SILENT", "NO_REPLY", "NO REPLY", NULL
};

/* token test: upper-case, whitespace-collapsed line in the token set */
static int cron_token_is_silent(const char *line)
{
    /* collapse whitespace + upper */
    char buf[256];
    size_t j = 0;
    int prev_ws = 0;
    for (size_t i = 0; line[i] && j + 1 < sizeof(buf); i++) {
        char c = line[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!prev_ws && j > 0) buf[j++] = ' ';
            prev_ws = 1;
            continue;
        }
        prev_ws = 0;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        buf[j++] = c;
    }
    if (j > 0 && buf[j-1] == ' ') j--; /* trim trailing space */
    buf[j] = '\0';
    for (int k = 0; CRON_SILENCE_TOKENS[k]; k++) {
        if (strcmp(buf, CRON_SILENCE_TOKENS[k]) == 0) return 1;
    }
    return 0;
}

/* PoP: _is_cron_silence_response @ cron/scheduler.py:_is_cron_silence_response */
int scheduler_is_cron_silence_response(const char *text)
{
    if (!text) return 0;
    /* strip */
    while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') text++;
    if (!*text) return 0;

    /* whole response is a token */
    if (cron_token_is_silent(text)) return 1;

    /* first/last non-empty line token */
    /* tokenize into lines, find first & last non-empty (after strip) */
    char *copy = strdup(text);
    char *lines[512];
    int nlines = 0;
    char *p = copy;
    char *start = p;
    while (*p) {
        if (*p == '\n') {
            *p = '\0';
            if (start[0]) { lines[nlines++] = start; }
            p++;
            start = p;
        } else {
            p++;
        }
    }
    if (start[0] && nlines < 512) lines[nlines++] = start;
    if (nlines > 0) {
        if (cron_token_is_silent(lines[0])) { free(copy); return 1; }
        if (cron_token_is_silent(lines[nlines - 1])) { free(copy); return 1; }
    }
    free(copy);

    /* bracketed prefix [SILENT] (case-insensitive) */
    /* check upper-prefix */
    char up[16];
    int k = 0;
    const char *q = text;
    while (*q && k < 15) {
        char c = *q++;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        up[k++] = c;
    }
    up[k] = '\0';
    if (strncmp(up, "[SILENT]", 8) == 0) return 1;
    return 0;
}

/* PoP: _normalize_deliver_value @ cron/scheduler.py:_normalize_deliver_value */
/*
 * Normalize a deliver value to its canonical string form.
 *  - None / "" -> "local"
 *  - list/tuple of non-empty strs -> comma-joined; empty -> "local"
 *  - any other scalar -> str(value)
 * The Python version also handles list/tuple inputs; C receives an already
 * flattened string OR a comma-joined list via a helper. The scalar entry
 * point below matches Python's str/None contract (the documented 99% case);
 * scheduler_normalize_deliver_value_list handles the list/tuple flatten form.
 * Result is malloc'd (caller frees).
 */
char *scheduler_normalize_deliver_value(const char *deliver)
{
    if (!deliver || !*deliver) return strdup("local");
    return strdup(deliver);
}

/* list form: parts[] (count=n), each a C-string; NULL/whitespace-only entries
 * skipped (mirrors Python str(p).strip() filter). */
char *scheduler_normalize_deliver_value_list(const char **parts, int n)
{
    /* filter empties, comma-join */
    size_t cap = 1;
    int cnt = 0;
    for (int i = 0; i < n; i++) {
        if (!parts[i]) continue;
        const char *p = parts[i];
        while (*p == ' ' || *p == '\t') p++;
        if (*p) { cap += strlen(p) + 1; cnt++; }
    }
    if (cnt == 0) return strdup("local");
    char *out = malloc(cap);
    out[0] = '\0';
    int written = 0;
    for (int i = 0; i < n; i++) {
        if (!parts[i]) continue;
        const char *p = parts[i];
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) continue;
        if (written) { strcat(out, ","); }
        strcat(out, p);
        written = 1;
    }
    return out;
}
