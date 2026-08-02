/*
 * port_agent_tool_executor.c — Port of Python agent/tool_executor.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include "hermes_logger.h"


/* Port of Python: _budget_for_agent */
typedef struct {
    int max_chars;
    int max_tokens;
} budget_config_t;

/* PoP: budget_for_agent @ agent/tool_executor.py:_budget_for_agent */
budget_config_t budget_for_agent(int context_length) {
    budget_config_t budget = {100000, 4096}; /* DEFAULT_BUDGET */
    
    if (context_length <= 0) return budget;
    
    /* Scale budget for small context models */
    if (context_length <= 65000) {
        budget.max_chars = context_length * 10; /* ~1 char per token */
        budget.max_tokens = context_length / 4;
    } else if (context_length <= 131000) {
        budget.max_chars = 200000;
        budget.max_tokens = 8192;
    } else {
        budget.max_chars = 400000;
        budget.max_tokens = 16384;
    }
    return budget;
}

/* Keep this above the stock auxiliary.web_extract timeout (360s) so the batch
 * guard does not preempt a slow-but-valid summarization attempt. */
#define DEFAULT_CONCURRENT_TOOL_TIMEOUT_S 420.0
/* Maximum number of concurrent worker threads for parallel tool execution. */
#define MAX_TOOL_WORKERS 8

/* PoP: agent_tool_executor__resolve_concurrent_tool_timeout @ agent/tool_executor.py:_resolve_concurrent_tool_timeout */
/* Resolve the concurrent-tool timeout from HERMES_CONCURRENT_TOOL_TIMEOUT_S.
 * Writes the timeout (seconds) to *out and returns 1 when a finite timeout
 * applies, or returns 0 (no timeout / disabled) when the env var is <= 0.
 * An unparseable value logs a warning and falls back to the default. */
int agent_tool_executor__resolve_concurrent_tool_timeout(double *out)
{
    const char *raw = getenv("HERMES_CONCURRENT_TOOL_TIMEOUT_S");
    /* strip() semantics: treat leading/trailing whitespace as empty */
    char trimmed[64] = {0};
    if (raw) {
        const char *s = raw;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        size_t n = strlen(s);
        while (n > 0 && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\n'||s[n-1]=='\r')) n--;
        if (n >= sizeof(trimmed)) n = sizeof(trimmed) - 1;
        memcpy(trimmed, s, n);
        trimmed[n] = '\0';
    }
    if (!trimmed[0]) {
        if (out) *out = DEFAULT_CONCURRENT_TOOL_TIMEOUT_S;
        return 1;
    }
    char *end = NULL;
    double value = strtod(trimmed, &end);
    if (end == trimmed || (end && *end != '\0')) {
        hermes_log(LOG_WARNING, "tool_executor",
                   "invalid HERMES_CONCURRENT_TOOL_TIMEOUT_S=%s; using %.0fs",
                   trimmed, (double)DEFAULT_CONCURRENT_TOOL_TIMEOUT_S);
        if (out) *out = DEFAULT_CONCURRENT_TOOL_TIMEOUT_S;
        return 1;
    }
    if (value <= 0) {
        return 0;   /* Python returns None → no timeout */
    }
    if (out) *out = value;
    return 1;
}

/* PoP: agent_tool_executor__is_interpreter_shutdown_submit_error @ agent/tool_executor.py:_is_interpreter_shutdown_submit_error */
/* True when the runtime error is the "cannot schedule new futures after
 * interpreter shutdown" thread-pool submit failure. */
int agent_tool_executor__is_interpreter_shutdown_submit_error(const char *exc_message)
{
    if (!exc_message) return 0;
    return strstr(exc_message, "cannot schedule new futures after interpreter shutdown") != NULL;
}

/* Callback that performs the actual SessionDB flush of already-appended
 * messages. Returns 0 on success, non-zero on failure. */
typedef int (*flush_messages_fn)(void *agent, void *messages);

/* PoP: agent_tool_executor__flush_session_db_after_tool_progress @ agent/tool_executor.py:_flush_session_db_after_tool_progress */
/* Best-effort incremental SessionDB flush for tool-call progress. Invokes the
 * agent's flush callback and, on failure, logs a warning tagged with the stage
 * (mirroring Python's try/except-and-warn). Never propagates the error. */
void agent_tool_executor__flush_session_db_after_tool_progress(
    void *agent, void *messages, flush_messages_fn flush_cb, const char *stage)
{
    if (!flush_cb) return;
    int rc = flush_cb(agent, messages);
    if (rc != 0) {
        hermes_log(LOG_WARNING, "tool_executor",
                   "Incremental tool-call persistence failed after %s (rc=%d)",
                   stage ? stage : "?", rc);
    }
}

