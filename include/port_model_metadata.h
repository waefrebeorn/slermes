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

/* ---- batch 2: pure gap ports ------------------------------------------- */

/* _parse_base_url -- urlparse of the normalized URL (scheme prepended when
 * absent, default "http"); returns 0 when Python returns None (empty URL).
 * Fixed-buffer result. Empty hostname means None; username_none /
 * password_none mirror Python's None; port -1 means None and port_invalid
 * marks a .port that would raise ValueError. */
typedef struct {
    char scheme[32];
    char netloc[512];
    char path[1024];
    char params[512];
    char query[1024];
    char fragment[512];
    char hostname[256];
    char username[256];
    char password[256];
    int username_none;
    int password_none;
    long port;
    int port_invalid;
} mm_parsed_url;

int mm_parse_base_url(const char *base_url, const char *scheme,
                      mm_parsed_url *out);

/* _strip_openrouter_routing_variant -- strip :nitro/:floor/:exacto/:online
 * only when the route is OpenRouter (provider arg or URL inference).
 * Caller frees; NULL model -> NULL, empty model -> empty string. */
char *mm_strip_openrouter_routing_variant(const char *model,
                                          const char *base_url,
                                          const char *provider);

/* _is_codex_route -- native openai-codex provider or a custom entry declaring
 * api_mode/transport codex_responses. The custom-provider list is passed
 * explicitly (pure slice); NULL is treated as an empty list -- the live
 * function's config load for None is I/O and out of scope. */
int mm_is_codex_route(const char *provider, const char *base_url,
                      const json_t *custom_providers);

/* _extract_flat_context_length / _context_length_from_model_payload --
 * top-level window keys (coerced to 1024..10M ints), then max_tokens for the
 * payload variant. Return 1 and set *out when a value is found, else 0. */
int mm_extract_flat_context_length(const json_t *payload, long long *out);
int mm_context_length_from_model_payload(const json_t *payload, long long *out);

/* _normalize_token_rates -- rescale token rates to per-token Python float
 * strings in place (explicit unit wins, else the >= $0.001/token heuristic);
 * returns the same pricing object. */
json_t *mm_normalize_token_rates(json_t *pricing, const json_t *unit);

/* _completion_split_budget -- window minus measured prompt from the OpenAI
 * parenthetical split; 1 + *out when >= 1, else 0 (None). */
int mm_completion_split_budget(const char *error_lower, long long *out);

/* _sglang_window_and_input -- (window, input) from SGLang's wording;
 * returns 1 when both figures are present, else 0 (None). */
int mm_sglang_window_and_input(const char *error_lower, long long *window,
                               long long *input);

/* _any_phrase_group -- any group whose phrases ALL occur in text. groups is
 * a JSON array of arrays of strings. */
int mm_any_phrase_group(const char *text, const json_t *groups);

/* _model_name_suggests_minimax -- minimax-prefixed or minimaxai/ ids. */
int mm_model_name_suggests_minimax(const char *model);

/* _model_name_suggests_stale_32k_underreport -- Kimi or MiniMax family. */
int mm_model_name_suggests_stale_32k_underreport(const char *model);

/* _positive_int -- int(value) for truthy JSON numbers/bools (True -> 1,
 * floats truncate toward zero); 1 + *out when found, else 0 (None). */
int mm_positive_int(const json_t *value, long long *out);

/* _bare_codex_slug -- (model or "").strip().lower().rsplit("/", 1)[-1].
 * Caller frees. */
char *mm_bare_codex_slug(const char *model);

/* _codex_variant_base -- eligible base of a valid -900k variant, else NULL.
 * Caller frees. */
char *mm_codex_variant_base(const char *model);

/* _is_bedrock_context -- provider == "bedrock" or a bedrock-runtime.*
 * hostname under amazonaws.com. */
int mm_is_bedrock_context(const char *base_url, const char *provider);

/* _codex_catalog_key -- exact key then case-insensitive match; keys is a
 * JSON object (its keys) or array of strings. Caller frees; NULL on miss. */
char *mm_codex_catalog_key(const json_t *keys, const char *slug);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MODEL_METADATA_H */
