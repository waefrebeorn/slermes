/*
 * port_model_metadata.h -- pure helpers ported from agent/model_metadata.py.
 *
 * URL/host classification, provider-family name predicates and the catalog
 * key matcher. No network, no filesystem, no provider registry.
 */
#ifndef PORT_MODEL_METADATA_H
#define PORT_MODEL_METADATA_H

#include <stddef.h>

#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _normalize_base_url -- (base_url or "").strip().rstrip("/"). Caller frees. */
char *mm_normalize_base_url(const char *base_url);

/* grok_supports_reasoning_effort -- allowlist of Grok effort-capable families. */
int mm_grok_supports_reasoning_effort(const char *model);

/* openai_model_rejects_reasoning -- gpt-3.5 / gpt-4 / chatgpt- ids (and ft:). */
int mm_openai_model_rejects_reasoning(const char *model);

/* is_grok_46_family -- exact "grok-4.6" or a "grok-4.6-" variant. */
int mm_is_grok_46_family(const char *model);

/* is_anthropic_fast_mode_model -- the exact fast-mode ids, with vendor prefix,
 * dotted form and dated snapshot spellings folded. */
int mm_is_anthropic_fast_mode_model(const char *model);

/* _catalog_key_matches -- substring match with version separators normalised,
 * plus the boundary-matched (solar-) family rule. */
int mm_catalog_key_matches(const char *key, const char *model_lower);

/* _longest_key_match -- first (key, value) whose key matches, longest key
 * first; ties keep table order. Returns the matched value, and copies the key
 * into `key_out` (may be NULL). NULL when nothing matches. */
json_t *mm_longest_key_match(const json_t *table, const char *model_lower,
                             char *key_out, size_t key_cap);

/* is_local_endpoint -- loopback, container DNS, unqualified hosts, RFC-1918,
 * link-local and Tailscale CGNAT. */
int mm_is_local_endpoint(const char *base_url);

/* _localhost_to_ipv4 -- scheme-anchored localhost -> 127.0.0.1. */
char *mm_localhost_to_ipv4(const char *url);

/* _server_root -- IPv4-resolved, /v1 suffix stripped. */
char *mm_server_root(const char *base_url);

/* _lmstudio_server_root -- strips /api/v1, /api or /v1. */
char *mm_lmstudio_server_root(const char *base_url);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MODEL_METADATA_H */
