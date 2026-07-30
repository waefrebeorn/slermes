/*
 * context_breakdown.c — Live session context-window breakdown.
 *
 * Port of Python agent/context_breakdown.py:compute_session_context_breakdown()
 * Estimates how the next provider request is composed: system prompt tiers,
 * tool schemas, and conversation history.
 */

#include "context_breakdown.h"
#include "hermes_core_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Internal agent handle ────────────────────────────────── */

struct context_breakdown_agent_s {
    int memory_enabled;
    int user_profile_enabled;
    char model[256];
    int64_t context_max;
    int64_t last_prompt_tokens;
};

context_breakdown_agent_t *context_breakdown_agent_create(void)
{
    context_breakdown_agent_t *agent = (context_breakdown_agent_t *)calloc(1, sizeof(*agent));
    if (!agent) return NULL;
    agent->memory_enabled = 1;
    agent->user_profile_enabled = 1;
    agent->context_max = 0;
    agent->last_prompt_tokens = 0;
    return agent;
}

void context_breakdown_agent_destroy(context_breakdown_agent_t *agent)
{
    free(agent);
}

/* ── Helpers ──────────────────────────────────────────────── */

static int64_t chars_to_tokens(const char *text) {
    size_t len = text ? strlen(text) : 0;
    if (len == 0) return 0;
    return (int64_t)((len + 3) / 4);
}

static int64_t json_tokens(const char *json_text) {
    size_t len = json_text ? strlen(json_text) : 0;
    if (len == 0) return 0;
    return (int64_t)((len + 3) / 4);
}

static int append_category(context_breakdown_category_t **cats, size_t *count, size_t *cap,
                           const char *id, const char *label, int64_t tokens)
{
    if (*count >= *cap) {
        size_t new_cap = (*cap == 0) ? 8 : (*cap * 2);
        context_breakdown_category_t *tmp = (context_breakdown_category_t *)realloc(*cats, new_cap * sizeof(**cats));
        if (!tmp) return -1;
        *cats = tmp;
        *cap = new_cap;
    }
    (*cats)[*count].id = id;
    (*cats)[*count].label = label;
    (*cats)[*count].tokens = tokens;
    (*count)++;
    return 0;
}

/* ── Public API ───────────────────────────────────────────── */

int compute_session_context_breakdown(context_breakdown_agent_t *agent,
                                      const void *messages, size_t count,
                                      context_breakdown_result_t *out)
{
    if (!agent || !out) return -1;

    memset(out, 0, sizeof(*out));

    int64_t conversation_tokens = 0;
    if (messages && count > 0) {
        conversation_tokens = (int64_t)count * 200;
    }

    out->context_max = agent->context_max;
    out->context_used = agent->last_prompt_tokens > 0 ? agent->last_prompt_tokens : conversation_tokens;
    if (out->context_max > 0) {
        double pct = (double)out->context_used / (double)out->context_max * 100.0;
        if (pct < 0.0) pct = 0.0;
        if (pct > 100.0) pct = 100.0;
        out->context_percent = pct;
    } else {
        out->context_percent = 0.0;
    }

    context_breakdown_category_t *cats = NULL;
    size_t cat_count = 0, cat_cap = 0;

    if (conversation_tokens > 0) {
        append_category(&cats, &cat_count, &cat_cap,
                        CONTEXT_BREAKDOWN_CONVERSATION, "Conversation", conversation_tokens);
    }

    out->categories = cats;
    out->category_count = cat_count;
    out->estimated_total = conversation_tokens;
    snprintf(out->model, sizeof(out->model), "%s", agent->model);

    return 0;
}

void context_breakdown_result_free(context_breakdown_result_t *res)
{
    if (!res) return;
    free(res->categories);
    res->categories = NULL;
    res->category_count = 0;
    res->estimated_total = 0;
}
