/*
 * port_prompt_caching_strip.h — Port of agent/prompt_caching.py strip helpers.
 */

#ifndef PORT_PROMPT_CACHING_STRIP_H
#define PORT_PROMPT_CACHING_STRIP_H

#include <json.h>
#include <stdbool.h>

/* PoP: strip_anthropic_cache_control @ agent/prompt_caching.py:strip_anthropic_cache_control
 * Mutates api_messages (a json_t array) in place: strips cache_control from
 * each message + its content parts, and flattens decoration-shaped content
 * lists back to a plain string.  Returns no value (Python returns the same
 * list).  The caller owns the array. */
void pca_strip_anthropic_cache_control(json_t *api_messages);

/* PoP: strip_anthropic_tool_cache_control @ agent/prompt_caching.py:strip_anthropic_tool_cache_control
 * Returns a deep copy of `tools` with per-tool cache_control removed (or an
 * empty array when tools is NULL/None).  Caller frees with json_free. */
json_t *pca_strip_anthropic_tool_cache_control(const json_t *tools);

#endif /* PORT_PROMPT_CACHING_STRIP_H */
