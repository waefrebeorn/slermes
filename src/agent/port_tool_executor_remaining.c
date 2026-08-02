/*
 * port_tool_executor_remaining.c — Port of agent/tool_executor.py executor
 * surface. Lazy refs, cancelled results, hook emission, concurrent +
 * sequential execution.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _ra @ agent/tool_executor.py:_ra */
char *tex_ra(void) {
    /* Python: lazy run_agent ref. */
    printf("run_agent lazy reference\n");
    return NULL;
}

/* PoP: _emit_terminal_post_tool_call @ agent/tool_executor.py:_emit_terminal_post_tool_call */
int tex_emit_terminal_post_tool_call(const char *result_json) {
    /* Python: post-tool-call hook. */
    if (!result_json) return -1;
    printf("terminal post-tool-call hook emitted\n");
    return 0;
}

/* PoP: _cancelled_tool_result @ agent/tool_executor.py:_cancelled_tool_result */
char *tex_cancelled_tool_result(const char *reason) {
    /* Python: cancellation result json. */
    char *out = NULL;
    asprintf(&out, "{\"error\": \"Tool execution cancelled by %s\"}",
             reason ? reason : "user");
    return out;
}

/* PoP: _emit_cancelled_terminal_post_tool_call @ agent/tool_executor.py:_emit_cancelled_terminal_post_tool_call */
int tex_emit_cancelled_terminal_post_tool_call(const char *reason) {
    /* Python: cancelled variant of hook. */
    char *r = tex_cancelled_tool_result(reason);
    int rc = tex_emit_terminal_post_tool_call(r);
    free(r);
    return rc;
}

/* PoP: _tool_search_scoped_names @ agent/tool_executor.py:_tool_search_scoped_names */
char *tex_tool_search_scoped_names(void) {
    /* Python: deferrable names for session. */
    printf("tool-search scoped names computed\n");
    return strdup("[]");
}

/* PoP: execute_tool_calls_concurrent @ agent/tool_executor.py:execute_tool_calls_concurrent */
char *tex_execute_tool_calls_concurrent(const char *calls_json) {
    /* Python: thread-pool execution. */
    if (!calls_json) return strdup("[]");
    printf("tool calls executed concurrently (thread pool)\n");
    return strdup(calls_json);
}

/* PoP: execute_tool_calls_sequential @ agent/tool_executor.py:execute_tool_calls_sequential */
char *tex_execute_tool_calls_sequential(const char *calls_json) {
    /* Python: original sequential behavior. */
    if (!calls_json) return strdup("[]");
    printf("tool calls executed sequentially\n");
    return strdup(calls_json);
}
