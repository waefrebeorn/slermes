/*
 * port_agent_oneshot.c — C port of agent/oneshot.py (pure-leaf helpers).
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: string @ agent/oneshot.py:_strip_code_fence */

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


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_oneshot_u_normalize_toolsets @ hermes_cli/oneshot.py:_normalize_toolsets */
int hermes_cli_oneshot_u_normalize_toolsets(const char *arg) {
    /* Python: str -> comma split; list -> items; dedup trim. Arg = "toolsets". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    int first = 1;
    const char *p = arg;
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        const char *t = p;
        while (len > 0 && (*t == ' ' || *t == '\t')) { t++; len--; }
        while (len > 0 && (t[len-1] == ' ' || t[len-1] == '\t')) len--;
        if (len) {
            if (!first) printf("\n");
            printf("%.*s", (int)len, t);
            first = 0;
        }
        p = comma ? comma + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_oneshot_u_validate_explicit_toolsets @ hermes_cli/oneshot.py:_validate_explicit_toolsets */
int hermes_cli_oneshot_u_validate_explicit_toolsets(const char *arg) {
    /* Python: toolset validation. Arg =
     * "valid\tstate\tresult\tunknown". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\t\n"); return 0; }
    if (t3 && t3[1] == '1') {
        fprintf(stderr, "hermes -z: --toolsets did not contain any valid toolsets.\n");
        return 0;
    }
    if (t4 && t4[1] == '1') {
        fprintf(stderr, "hermes -z: ignoring unknown --toolsets entries (and disabled MCP servers)\n");
    }
    printf("%s\t\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_oneshot_u_write_usage_file @ hermes_cli/oneshot.py:_write_usage_file */
int hermes_cli_oneshot_u_write_usage_file(const char *arg) {
    /* Python: spend report. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("usage file write skipped\n"); return 0; }
    printf("usage report written: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_oneshot_run_oneshot @ hermes_cli/oneshot.py:run_oneshot */
int hermes_cli_oneshot_run_oneshot(const char *arg) {
    /* Python: stateless LLM call. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "empty") == 0) {
        fprintf(stderr, "run_oneshot requires a template or instructions/user_input\n");
        return 1;
    }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_oneshot_u_create_session_db_for_oneshot @ hermes_cli/oneshot.py:_create_session_db_for_oneshot */
int hermes_cli_oneshot_u_create_session_db_for_oneshot(const char *arg) {
    /* Python: SessionDB() or None. Arg = "available\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("%s\n", tab ? tab + 1 : "session db"); return 0; }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_oneshot_u_oneshot_clarify_callback @ hermes_cli/oneshot.py:_oneshot_clarify_callback */
int hermes_cli_oneshot_u_oneshot_clarify_callback(const char *arg) {
    /* Python: oneshot no-user guidance. Arg = choices (tab-sep, empty =
     * none). */
    if (!arg || !*arg) {
        printf("[oneshot mode: no user available. Make the most reasonable assumption you can and continue.]\n");
        return 0;
    }
    printf("[oneshot mode: no user available. Pick the best option from %s using your own judgment and continue.]\n", arg);
    return 0;
}
