#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
/*
 * ports/agent/turn_context.c — C11 port of agent/turn_context.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_turn_context.c ── */
/* Slermes C port — agent/turn_context.py (pure helper) */

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"
#include "provider_metadata.h"

/* PoP: agent_turn_context_compression_made_progress @ agent/turn_context.py:compression_made_progress */
bool agent_turn_context_compression_made_progress(
    long orig_len, long new_len, long orig_tokens, long new_tokens)
{
    if (new_len < orig_len) return true;
    return orig_tokens > 0 && new_tokens < orig_tokens * 0.95;
}

/* PoP: the @ agent/turn_context.py:_should_run_preflight_estimate */
/* Cheap gate for the (expensive) full preflight token estimate.
 * Returns True when EITHER:
 *   (a) message count exceeds the protected ranges (historical gate), OR
 *   (b) a cheap char-based estimate already crosses the threshold (the
 *       few-but-huge case from #27405 that the count-only gate skips).
 * Branch (b) uses estimate_messages_tokens_rough so a single large base64
 * image isn't mistaken for ~250K tokens. */
bool agent_turn_context__should_run_preflight_estimate(
    const json_t *messages, long protect_first_n, long protect_last_n,
    long threshold_tokens)
{
    if (!messages) return false;
    long count = (long)json_len(messages);
    if (count > protect_first_n + protect_last_n + 1) return true;
    long est = estimate_messages_tokens_rough(messages);
    return est >= threshold_tokens;
}


/* ── from src/agent/port_agent_turn_context_py_parity.c ── */
/* Slermes C port — agent/turn_context.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: agent_turn_context_maybe_title_session_at_turn_start_p @ agent/turn_context.py:_maybe_title_session_at_turn_start */
json_t *agent_turn_context_maybe_title_session_at_turn_start_p(void) {
    return json_object();
}

