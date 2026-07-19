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
#include "cron_scheduler_helpers.h"

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

/* ── cron failure summarization ─────────────────────────────────────────────
 * PoP: _summarize_cron_failure_for_delivery @ cron/scheduler.py
 * Returns a malloc'd compact one-line failure message for chat delivery.
 * job_name may be NULL (falls back to "cron job"); error may be NULL. */

static const char *CRON_WARN = "\xe2\x9a\xa0\xef\xb8\x8f"; /* ⚠️ */

/* case-insensitive substring test (needle assumed already lowercased) */
static int ci_str(const char *hay, const char *needle)
{
    if (!hay || !needle) return 0;
    size_t nlen = strlen(needle);
    if (nlen == 0) return 1;
    size_t hlen = strlen(hay);
    if (nlen > hlen) return 0;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        size_t j = 0;
        for (; j < nlen; j++) {
            char hc = hay[i + j];
            if (hc >= 'A' && hc <= 'Z') hc = (char)(hc - 'A' + 'a');
            if (hc != needle[j]) break;
        }
        if (j == nlen) return 1;
    }
    return 0;
}

/* whole-token match for "401"/"403" (word-boundary: not letter/digit around) */
static int contains_whole_status(const char *text, const char *tok)
{
    size_t tlen = strlen(tok);
    size_t len = strlen(text);
    for (size_t i = 0; i + tlen <= len; i++) {
        if (i > 0) {
            char b = text[i - 1];
            if ((b >= '0' && b <= '9') || (b >= 'a' && b <= 'z') ||
                (b >= 'A' && b <= 'Z')) continue;
        }
        if (strncmp(text + i, tok, tlen) != 0) continue;
        char a = text[i + tlen];
        if ((a >= '0' && a <= '9') || (a >= 'a' && a <= 'z') ||
            (a >= 'A' && a <= 'Z')) continue;
        return 1;
    }
    return 0;
}

/* lower-case a copy into a malloc'd buffer (caller frees) */
static char *strdup_lower(const char *s)
{
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    for (size_t i = 0; i <= len; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        out[i] = c;
    }
    return out;
}

/* collapse internal whitespace runs to a single space + trim (in place). */
static void collapse_ws(char *s)
{
    size_t j = 0, k = 0;
    int prev_ws = 0;
    while (s[k]) {
        char c = s[k++];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!prev_ws && j > 0) { s[j++] = ' '; prev_ws = 1; }
            continue;
        }
        s[j++] = c; prev_ws = 0;
    }
    if (j > 0 && s[j - 1] == ' ') j--;
    s[j] = '\0';
}

char *scheduler_summarize_cron_failure(const char *job_name, const char *error)
{
    const char *jn = (job_name && job_name[0]) ? job_name : "cron job";
    const char *text = (error && error[0]) ? error : "unknown error";

    char *low = strdup_lower(text);

    /* Provider/API failures -> short reason. */
    if (strstr(text, "429") != NULL ||
        ci_str(low, "rate limit") ||
        ci_str(low, "usage limit"))
    {
        const char *reason = "rate limit";
        if (ci_str(low, "weekly usage limit")) reason = "weekly usage limit";
        else if (ci_str(low, "quota")) reason = "quota limit";
        size_t need = (size_t)snprintf(NULL, 0,
                 "%s Cron '%s' failed: provider %s. "
                 "Fallback chain was exhausted or unavailable. "
                 "Full details saved in cron output.",
                 CRON_WARN, jn, reason) + 1;
        char *out = malloc(need);
        snprintf(out, need,
                 "%s Cron '%s' failed: provider %s. "
                 "Fallback chain was exhausted or unavailable. "
                 "Full details saved in cron output.",
                 CRON_WARN, jn, reason);
        free(low);
        return out;
    }

    if (ci_str(low, "readtimeout") || ci_str(low, "timed out") ||
        ci_str(low, "timeout"))
    {
        size_t need = (size_t)snprintf(NULL, 0,
                 "%s Cron '%s' failed: provider timeout. "
                 "Fallback chain was exhausted or unavailable. "
                 "Full details saved in cron output.",
                 CRON_WARN, jn) + 1;
        char *out = malloc(need);
        snprintf(out, need,
                 "%s Cron '%s' failed: provider timeout. "
                 "Fallback chain was exhausted or unavailable. "
                 "Full details saved in cron output.",
                 CRON_WARN, jn);
        free(low);
        return out;
    }

    if (ci_str(low, "authenticat") || ci_str(low, "authoriz") ||
        contains_whole_status(text, "401") ||
        contains_whole_status(text, "403"))
    {
        size_t need = (size_t)snprintf(NULL, 0,
                 "%s Cron '%s' failed: provider authentication error. "
                 "Full details saved in cron output.",
                 CRON_WARN, jn) + 1;
        char *out = malloc(need);
        snprintf(out, need,
                 "%s Cron '%s' failed: provider authentication error. "
                 "Full details saved in cron output.",
                 CRON_WARN, jn);
        free(low);
        return out;
    }

    /* Generic path: strip exception wrappers, collapse whitespace, bound size. */
    char *work = strdup(text);
    /* bound input to 2000 chars like Python text[:2000] */
    size_t wlen = strlen(work);
    if (wlen > 2000) work[2000] = '\0';
    /* strip leading "ExceptionType: " */
    static const char *WRAPS[] = {
        "RuntimeError: ", "Exception: ", "ValueError: ", "HTTPStatusError: ", NULL
    };
    for (int i = 0; WRAPS[i]; i++) {
        size_t wl = strlen(WRAPS[i]);
        if (strncmp(work, WRAPS[i], wl) == 0) {
            memmove(work, work + wl, strlen(work + wl) + 1);
            break;
        }
    }
    collapse_ws(work);
    if (strlen(work) > 180) {
        work[177] = '\0';
        /* rstrip trailing space */
        size_t l = strlen(work);
        while (l > 0 && (work[l - 1] == ' ' || work[l - 1] == '\t')) work[--l] = '\0';
        size_t tneed = l + 4;
        char *tr = malloc(tneed);
        snprintf(tr, tneed, "%s...", work);
        free(work);
        work = tr;
    }
    size_t need = (size_t)snprintf(NULL, 0, "%s Cron '%s' failed: %s",
                                   CRON_WARN, jn, work) + 1;
    char *out = malloc(need);
    snprintf(out, need, "%s Cron '%s' failed: %s", CRON_WARN, jn, work);
    free(work);
    free(low);
    return out;
}
