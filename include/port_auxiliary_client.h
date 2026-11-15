/* port_auxiliary_client.h -- pure helpers ported from agent/auxiliary_client.py.
 *
 * Model-slug and provider-route predicates plus the per-model compression
 * threshold. No network, no client objects, no registry.
 */
#ifndef SLERMES_PORT_AUXILIARY_CLIENT_H
#define SLERMES_PORT_AUXILIARY_CLIENT_H

#include <stddef.h>

#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _bare_model -- lowercased slug with any vendor/ prefix stripped. Caller frees. */
char *ac_bare_model(const char *model);

/* _is_kimi_model -- Kimi / Moonshot manage temperature server-side. */
int ac_is_kimi_model(const char *model);

/* _is_arcee_trinity_thinking -- Arcee Trinity Large Thinking, direct or OpenRouter. */
int ac_is_arcee_trinity_thinking(const char *model);

/* _codex_route_bare_model -- bare slug only on the Codex OAuth route, else NULL.
 * Caller frees. */
char *ac_codex_route_bare_model(const char *model, const char *provider);

/* _is_codex_spark -- gpt-5.3-codex-spark on the Codex OAuth route only. */
int ac_is_codex_spark(const char *model, const char *provider);

/* _is_openai_default_temperature_only -- reasoning families that 400 on any
 * non-default temperature (gpt-5.x except gpt-5-chat, o1/o3/o4). */
int ac_is_openai_default_temperature_only(const char *model);

/* _is_codex_gpt54_or_gpt55 -- the 272K-capped families, Codex route only,
 * excluding -900k context variants. */
int ac_is_codex_gpt54_or_gpt55(const char *model, const char *provider);

/* _compression_threshold_for_model -- fraction of context, or -1.0 for none.
 * allow_codex_gpt55_autoraise gates the gpt-5.4/5.5/5.6 family. */
double ac_compression_threshold_for_model(const char *model,
                                          const char *provider,
                                          int allow_codex_gpt55_autoraise);

/* _model_recency_key -- natural-sort key: digit runs compare numerically.
 * Emits a canonical "|"-joined representation ("t:<text>" / "n:<number>") so a
 * C string and the Python tuple sort identically. Caller buffer. */
void ac_model_recency_key(const char *model_id, char *out, size_t cap);

/* _ai_gateway_headers -- Vercel AI Gateway attribution.
 * `base_version` is passed in because the C version helper takes it explicitly
 * rather than walking git at request time. Builds a JSON object; caller frees. */
json_t *ac_ai_gateway_headers(const char *base_version);

/* base_url_hostname / base_url_host_matches, ported from utils.py: exact-host
 * comparison so `evil.com/moonshot.ai` cannot masquerade as a provider. */
char *ac_base_url_hostname(const char *base_url);
int ac_base_url_host_matches(const char *base_url, const char *domain);

/* build_nvidia_nim_headers: cloud attribution applies only to the NVIDIA host. */
int ac_nvidia_nim_host(const char *base_url);

/* _anthropic_event_has_content -- whether a stream event carries a payload.
 * `event` is the JSON object; returns 0/1. */
int ac_anthropic_event_has_content(const json_t *event);

/* build_or_headers -- OpenRouter headers plus the response-cache pair.
 * Inputs stand in for os.environ and the openrouter config block:
 *   env_cache  "1"/"true"/"yes"/"on" enables; empty means "use cfg_cache"
 *   env_ttl    digits 1..86400; empty means "use cfg_ttl"
 * Returns a JSON object; caller frees. */
json_t *ac_build_or_headers(const char *env_cache, int cfg_cache,
                            const char *env_ttl, double cfg_ttl);

/* _fast_model_from_catalog: the pure selection half (the fetch is network).
 * Sorts `ids` newest-first by _model_recency_key, then walks the fast-model
 * families and returns the first id containing a family and no exclude.
 * Empty when nothing qualifies. */
void ac_fast_model_select(const char **ids, int n, char *out, size_t cap);

/* _API_KEY_PROVIDER_AUX_MODELS_FALLBACK lookup. */
void ac_aux_model_fallback(const char *provider, char *out, size_t cap);

/* _FAST_MODEL_TASKS membership. */
int ac_task_can_prefer_fast(const char *task);

/* _resolve_provider_vision_default: the static table half (the profile lookup
 * needs the provider registry). Returns 1 when a static entry matched. */
int ac_provider_vision_static(const char *provider, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_PORT_AUXILIARY_CLIENT_H */
