/*
 * port_prompt_caching_plan.h — Port of agent/prompt_caching.py cache-plan
 * builder + helpers.  json_t*-based (no string round-trip).
 */

#ifndef PORT_PROMPT_CACHING_PLAN_H
#define PORT_PROMPT_CACHING_PLAN_H

#include <json.h>
#include <stdbool.h>

/* PoP: apply_anthropic_cache_control @ agent/prompt_caching.py:apply_anthropic_cache_control
 * Shallow-copy messages with cache markers applied.  Caller frees with json_free. */
json_t *pca_apply_anthropic_cache_control(const json_t *api_messages,
                                          const char *cache_ttl,
                                          bool native_anthropic,
                                          const char *static_system_prefix);

/* PoP: build_prompt_cache_plan @ agent/prompt_caching.py:build_prompt_cache_plan
 * Returns {"messages":<array>,"tools":<array>} plan.  Caller frees with json_free.
 *   cache_ttl (NULL → "5m"), static_system_prefix (NULL = None). */
json_t *pca_build_prompt_cache_plan(const json_t *api_messages,
                                    const json_t *tools,
                                    const char *cache_ttl,
                                    bool native_anthropic,
                                    const char *static_system_prefix,
                                    bool direct_native_tool_cache);

/* PoP: plan_cache_sections_for_destination @ agent/agent_runtime_helpers.py:plan_cache_sections_for_destination
 * Returns {"messages":<array>,"tools":<array>} plan (request-local copies).
 * cache_ttl (NULL → "5m"); cache_disabled threads the operator disable flag. */
json_t *pca_plan_cache_sections_for_destination(const json_t *messages,
                                                const json_t *tools,
                                                const char *provider,
                                                const char *base_url,
                                                const char *api_mode,
                                                const char *model,
                                                const char *cache_ttl,
                                                bool cache_disabled);

/* PoP: _count_cache_markers / marker_count (PromptCachePlan property) */
int pca_count_cache_markers(const json_t *messages, const json_t *tools);
int pca_prompt_cache_plan_marker_count(const json_t *plan);

/* PoP: anthropic_prompt_cache_policy @ agent/agent_runtime_helpers.py:anthropic_prompt_cache_policy
 * Resolves (should_cache, use_native_layout).  cache_disabled threads disable. */
void pca_anthropic_prompt_cache_policy(const char *provider,
                                        const char *base_url,
                                        const char *api_mode,
                                        const char *model,
                                        bool cache_disabled,
                                        bool *out_should_cache,
                                        bool *out_native_layout);

#endif /* PORT_PROMPT_CACHING_PLAN_H */
