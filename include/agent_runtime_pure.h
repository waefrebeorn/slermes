/*
 * agent_runtime_pure.h — Pure-logic cache-policy helpers ported from
 * agent/agent_runtime_helpers.py.  Self-contained (libjson + port_config).
 */

#ifndef HERMES_AGENT_RUNTIME_PURE_H
#define HERMES_AGENT_RUNTIME_PURE_H

#include <stddef.h>
#include <stdbool.h>
#include <json.h>

/* ── cache_ttl_means_disabled ───────────────────────────────────────────── */
/* Python: cache_ttl_means_disabled(ttl).  The C caller disambiguates the
 * None/False/str forms via is_bool/bool_val (None = ttl_str=NULL, is_bool=false). */
bool cache_ttl_means_disabled(const char *ttl_str, bool is_bool, bool bool_val);

/* ── prompt_caching_disabled_from_config ─────────────────────────────────── */
/* Reads prompt_caching.cache_ttl from readonly config; delegates to above. */
bool prompt_caching_disabled_from_config(void);

/* ── blank_cache_policy_stub / cache_policy_stub_t ─────────────────────────── */
/* Python SimpleNamespace(provider="", base_url="", api_mode="", model="",
 *  _cache_disabled=...).  Opaque struct. */
struct cache_policy_stub;
struct cache_policy_stub *blank_cache_policy_stub(bool cache_disabled);
struct cache_policy_stub *blank_cache_policy_stub_default(void);
void cache_policy_stub_free(struct cache_policy_stub *s);

/* Accessors for the stub fields (consumers should use these, not reach in). */
const char *cache_policy_stub_provider(struct cache_policy_stub *s);
const char *cache_policy_stub_base_url(struct cache_policy_stub *s);
const char *cache_policy_stub_api_mode(struct cache_policy_stub *s);
const char *cache_policy_stub_model(struct cache_policy_stub *s);
bool cache_policy_stub_disabled(struct cache_policy_stub *s);

/* ── _direct_native_anthropic_tool_cache_capability ──────────────────────── */
/* Python: _direct_native_anthropic_tool_cache_capability(agent, *, provider,
 *  base_url, api_mode, model).  C takes the resolved strings directly. */
bool direct_native_anthropic_tool_cache_capability(const char *provider,
                                                    const char *base_url,
                                                    const char *api_mode,
                                                    const char *model);

/* ── _msg_has_payload ────────────────────────────────────────────────────── */
/* Python: _msg_has_payload(msg).  msg is a json_t message object. */
bool msg_has_payload(const json_t *msg);

/* ── repair_empty_non_final_messages ─────────────────────────────────────── */
/* Returns a deep-copied, repaired message array (original untouched). */
json_t *repair_empty_non_final_messages(const json_t *messages);

#endif /* HERMES_AGENT_RUNTIME_PURE_H */
