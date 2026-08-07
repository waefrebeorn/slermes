/*
 * context_breakdown.h — Live session context-window breakdown.
 *
 * Port of Python agent/context_breakdown.py:compute_session_context_breakdown()
 * Estimates how the next provider request is composed: system prompt tiers,
 * tool schemas, and conversation history.
 */

#ifndef HERMES_CONTEXT_BREAKDOWN_H
#define HERMES_CONTEXT_BREAKDOWN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque agent handle; mirrors Python agent object. */
typedef struct context_breakdown_agent_s context_breakdown_agent_t;

/* Breakdown category IDs. Keep in sync with Python _CATEGORY_COLORS keys. */
#define CONTEXT_BREAKDOWN_SYSTEM_PROMPT   "system_prompt"
#define CONTEXT_BREAKDOWN_TOOL_DEFS       "tool_definitions"
#define CONTEXT_BREAKDOWN_RULES           "rules"
#define CONTEXT_BREAKDOWN_SKILLS          "skills"
#define CONTEXT_BREAKDOWN_MCP             "mcp"
#define CONTEXT_BREAKDOWN_SUBAGENTS       "subagent_definitions"
#define CONTEXT_BREAKDOWN_MEMORY          "memory"
#define CONTEXT_BREAKDOWN_CONVERSATION    "conversation"

/* Single category entry. */
typedef struct {
    const char *id;       /* category id, never NULL */
    const char *label;    /* display label, never NULL */
    int64_t tokens;       /* estimated token count */
} context_breakdown_category_t;

/* Top-level breakdown result. */
typedef struct {
    context_breakdown_category_t *categories; /* malloc'd array */
    size_t category_count;           /* number of categories */
    int64_t context_max;             /* provider context limit */
    int64_t context_used;            /* measured or estimated tokens */
    double context_percent;          /* 0..100 */
    int64_t estimated_total;         /* sum of category tokens */
    char model[256];                 /* model name */
} context_breakdown_result_t;

/**
 * compute_session_context_breakdown — Return a Cursor-style context usage
 * breakdown for one live agent.
 *
 * @param agent      Opaque agent handle. Must have been initialized via
 *                   context_breakdown_agent_init() or equivalent.
 * @param messages   Conversation messages array, or NULL for none.
 * @param count      Number of entries in @p messages.
 * @param out        Output result. Caller must free() the returned
 *                   categories array with context_breakdown_result_free().
 * @return 0 on success, -1 on invalid args.
 */
int compute_session_context_breakdown(context_breakdown_agent_t *agent,
                                      const void *messages, size_t count,
                                      context_breakdown_result_t *out);

void context_breakdown_result_free(context_breakdown_result_t *res);
context_breakdown_agent_t *context_breakdown_agent_create(void);
void context_breakdown_agent_destroy(context_breakdown_agent_t *agent);

/* ── Pure renderers (ported from agent/context_breakdown.py) ───────────────
 * All take/return malloc'd JSON strings / line arrays (caller frees). */

/* PoP: _bytes_to_tokens @ agent/context_breakdown.py:_bytes_to_tokens */
long context_breakdown_bytes_to_tokens(long size);

/* PoP: render_context_grid @ agent/context_breakdown.py:render_context_grid */
char **context_breakdown_render_grid(const char *payload_json, size_t *out_lines);

/* PoP: render_context_category_lines @ agent/context_breakdown.py:render_context_category_lines */
char **context_breakdown_render_category_lines(const char *payload_json, size_t *out_lines);

/* PoP: render_context_details_lines @ agent/context_breakdown.py:render_context_details_lines */
char **context_breakdown_render_details_lines(const char *details_json, size_t *out_lines);

/* PoP: render_context_breakdown_lines @ agent/context_breakdown.py:render_context_breakdown_lines */
char **context_breakdown_render_lines(const char *payload_json, const char *details_json,
                                      int grid, size_t *out_lines);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CONTEXT_BREAKDOWN_H */
