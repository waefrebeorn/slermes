/* port_auxiliary_client.h -- pure helpers ported from agent/auxiliary_client.py.
 *
 * Model-slug and provider-route predicates plus the per-model compression
 * threshold. No network, no client objects, no registry.
 */
#ifndef SLERMES_PORT_AUXILIARY_CLIENT_H
#define SLERMES_PORT_AUXILIARY_CLIENT_H

#include <stddef.h>

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

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_PORT_AUXILIARY_CLIENT_H */
