/*
 * port_agent_oneshot.c — C port of agent/oneshot.py (pure-leaf helpers).
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_oneshot__strip_code_fence @ agent/oneshot.py:_strip_code_fence */

/* Port of Python agent/oneshot.py:_strip_code_fence.
 * Drop a single wrapping ``` fence the model may have added.
 * Returns malloc'd string (caller frees). */
char *cli_agent_oneshot__strip_code_fence(const char *text)
{
    if (!text) return strdup("");

    /* Not starting with a fence -> unchanged. */
    if (strncmp(text, "```", 3) != 0) {
        return strdup(text);
    }

    /* Match Python str.splitlines(): a trailing newline does NOT create a
     * final empty line; line terminators are dropped (incl. \r\n). */
    size_t text_len = strlen(text);

    char **lines = (char **)calloc((size_t)text_len + 1, sizeof(char *));
    if (!lines) return strdup(text);
    int n = 0;
    const char *start = text;
    for (const char *p = text; ; p++) {
        if (*p == '\n' || *p == '\0') {
            size_t len = (size_t)(p - start);
            if (len > 0 && start[len - 1] == '\r') len--;  /* drop \r of \r\n */
            /* skip the trailing empty slice produced by a final \n / \0 */
            if (!(len == 0 && (*p == '\0' || p == text + text_len - 1 && *p == '\n'))) {
                char *ln = (char *)malloc(len + 1);
                if (ln) { memcpy(ln, start, len); ln[len] = '\0'; lines[n++] = ln; }
            }
            start = p + 1;
            if (*p == '\0') break;
        }
    }

    /* Need >= 2 lines: first starts with ```, last (stripped) == ``` */
    int result_is_stripped = 0;
    if (n >= 2) {
        /* first line starts with ``` (already true since text starts with it) */
        char *last = lines[n - 1];
        /* strip trailing whitespace from last line for comparison */
        size_t L = strlen(last);
        while (L > 0 && (last[L - 1] == ' ' || last[L - 1] == '\t' || last[L - 1] == '\r')) L--;
        if (L == 3 && strncmp(last, "```", 3) == 0) {
            result_is_stripped = 1;
        }
    }

    if (result_is_stripped) {
        /* join lines[1..n-2] with "\n", then strip() (leading/trailing ws). */
        size_t cap = 1;
        for (int i = 1; i < n - 1; i++) cap += strlen(lines[i]) + 1;
        char *out = (char *)malloc(cap);
        out[0] = '\0';
        for (int i = 1; i < n - 1; i++) {
            if (i > 1) strcat(out, "\n");
            strcat(out, lines[i]);
        }
        /* strip leading/trailing whitespace */
        char *b = out;
        while (*b == ' ' || *b == '\t' || *b == '\n' || *b == '\r') b++;
        size_t R = strlen(b);
        while (R > 0 && (b[R - 1] == ' ' || b[R - 1] == '\t' || b[R - 1] == '\n' || b[R - 1] == '\r')) b[--R] = '\0';
        /* move to a fresh buffer at b (in place is fine) */
        char *res = strdup(b);
        /* free lines */
        for (int i = 0; i < n; i++) free(lines[i]);
        free(lines);
        return res;
    }

    for (int i = 0; i < n; i++) free(lines[i]);
    free(lines);
    return strdup(text);
}
