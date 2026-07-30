/*
 * transport_common.h — public API for agent/transports/types.py + base.py.
 *
 * Normalized provider-response types (ToolCall / Usage / NormalizedResponse)
 * and the ProviderTransport base + transport registry. This is the dependency
 * ROOT for every agent/transports/* port: adapters register themselves here
 * and the agent loop resolves one by api_mode at runtime (no static dispatch).
 *
 * Opaque-where-possible; only the shared normalized surface is exported.
 * Network I/O is NOT owned here — adapters build kwargs; the provider client
 * (provider_*.c) performs the call. This matches the Python design:
 *   convert_messages -> convert_tools -> build_kwargs -> normalize_response
 */
#ifndef SLERMES_TRANSPORT_COMMON_H
#define SLERMES_TRANSPORT_COMMON_H

#include <stddef.h>
#include <stdbool.h>

/* Forward opaque registry. */
typedef struct transport_registry transport_registry_t;

/* ── NormalizedResponse building blocks (port of transports/types.py) ─────── */

/* A normalized tool call from any provider. */
typedef struct tool_call {
    char *id;            /* canonical id (tool_call_id / tool_use_id); may be NULL */
    char *name;          /* function name */
    char *arguments;     /* JSON string */
    char *provider_data; /* JSON object of per-call protocol metadata, or NULL */
} tool_call_t;

/* Token usage from an API response. */
typedef struct usage {
    long prompt_tokens;
    long completion_tokens;
    long total_tokens;
    long cached_tokens;
} usage_t;

/* Normalized API response from any provider. */
typedef struct normalized_response {
    char *content;            /* text content, or NULL */
    tool_call_t **tool_calls; /* NULL-terminated array, or NULL */
    size_t tool_call_count;
    char *finish_reason;      /* "stop" | "tool_calls" | "length" | "content_filter" */
    char *reasoning;          /* extended-thinking text, or NULL */
    usage_t *usage;           /* NULL if absent */
    char *provider_data;      /* JSON object of response-level protocol state, or NULL */
} normalized_response_t;

/* Factories (port of build_tool_call / map_finish_reason). */
tool_call_t *tool_call_create(const char *id, const char *name, const char *arguments_json,
                              const char *provider_data_json);
void tool_call_free(tool_call_t *tc);

normalized_response_t *normalized_response_create(void);
void normalized_response_free(normalized_response_t *nr);

/* map_finish_reason: translate a provider stop reason to the normalized set,
 * falling back to "stop" for unknown/empty. mapping is NULL-terminated
 * array of {raw, normalized} pairs (or NULL to pass through). */
char *map_finish_reason(const char *raw_reason, const char *const *mapping);

/* ── ProviderTransport base + registry (port of transports/base.py) ────────── */

/* Abstract transport vtable. Each adapter implements these. */
typedef struct provider_transport provider_transport_t;

struct provider_transport {
    const char *api_mode;          /* e.g. "anthropic_messages" */
    /* Returns provider-native messages structure as a JSON string. */
    char *(*convert_messages)(provider_transport_t *self, const char *messages_json);
    /* Returns provider-native tools structure as a JSON string. */
    char *(*convert_tools)(provider_transport_t *self, const char *tools_json);
    /* Build the complete API-call kwargs dict (JSON string). */
    char *(*build_kwargs)(provider_transport_t *self, const char *model,
                          const char *messages_json, const char *tools_json);
    /* Normalize a raw provider response (JSON string) -> normalized_response_t*. */
    normalized_response_t *(*normalize_response)(provider_transport_t *self,
                                                 const char *raw_response_json);
    /* Optional: return true if structurally valid. Default: true. */
    bool (*validate_response)(provider_transport_t *self, const char *raw_response_json);
    /* Optional: extract {cached_tokens, creation_tokens} as JSON, or NULL. */
    char *(*extract_cache_stats)(provider_transport_t *self, const char *raw_response_json);
    /* Optional: map provider stop reason -> normalized. Default: pass through. */
    char *(*map_finish_reason)(provider_transport_t *self, const char *raw_reason);
    void *impl;                    /* adapter-private state */
};

/* Registry: adapters register by api_mode; consumers resolve at runtime. */
transport_registry_t *transport_registry_create(void);
void transport_registry_free(transport_registry_t *reg);
/* Register (takes ownership of the vtable struct; name must outlive registry). */
int transport_registry_register(transport_registry_t *reg, provider_transport_t *t);
/* Resolve a transport by api_mode (NULL if not registered). Borrowed ref. */
provider_transport_t *transport_registry_get(transport_registry_t *reg, const char *api_mode);
/* Snapshot registered api_modes as a NULL-terminated array (caller frees). */
char **transport_registry_list(transport_registry_t *reg);

/* The global registry (lazily built by transport_registry_init). */
transport_registry_t *transport_registry_init(void);
void transport_registry_shutdown(void);

#endif /* SLERMES_TRANSPORT_COMMON_H */
