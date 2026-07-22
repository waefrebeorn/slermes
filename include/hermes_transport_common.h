/*
 * hermes_transport_common.h — shared LLM-transport helper primitives.
 *
 * These implement the provider-agnostic logic that every transport adapter's
 * converters need (finish-reason normalization, role canonicalization, etc.)
 * so the per-provider converters can be thin ports instead of re-implementing
 * the same maps. Faithful port of agent/transports/{base,anthropic,bedrock,
 * codex,types}.py map_finish_reason logic.
 */
#ifndef HERMES_TRANSPORT_COMMON_H
#define HERMES_TRANSPORT_COMMON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Map a provider-specific stop/finish reason to the normalized OpenAI-style
 * set: "stop", "length", "tool_calls", "content_filter".
 *
 * provider is one of: "anthropic", "bedrock", "google", "codex", "openai",
 * or NULL/"" for an identity pass-through (base.py default).
 * Returns a static string (no free needed). Unknown reasons fall back to
 * "stop" (types.py / per-provider default).
 */
const char *transport_map_finish_reason(const char *provider, const char *raw_reason);

/*
 * Canonicalize an OpenAI-style chat role string. Returns one of
 * "system", "user", "assistant", "tool" (static strings). Unknown roles are
 * normalized leniently: "function" -> "tool"; anything missing/empty ->
 * "user". Mirrors the common role handling across convert_messages ports.
 */
const char *transport_normalize_role(const char *role);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TRANSPORT_COMMON_H */
