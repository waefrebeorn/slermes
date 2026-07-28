/*
 * provider_profile.h — public API for providers/base.py + providers/__init__.py
 * and the bundled model-provider plugin profiles.
 *
 * A ProviderProfile declares everything about an inference provider in one
 * place: auth, endpoints, client quirks, request-time quirks. The transport
 * reads this instead of receiving 20+ boolean flags.
 *
 * Faithful C mapping of the Python dataclass + hook design:
 *   - dataclass fields        -> struct fields (owned strings/arrays)
 *   - overrideable hooks      -> function pointers in a vtable with
 *                                default implementations (NULL = default)
 *   - OMIT_TEMPERATURE object -> PROFILE_OMIT_TEMPERATURE sentinel
 *   - register_provider()     -> provider_profile_register()
 *   - get_provider_profile()  -> provider_profile_get() (name or alias)
 *   - list_providers()        -> provider_profile_list()
 *   - bundled plugin discovery-> provider_profiles_register_builtin()
 *     (plugins/model-providers/<name>/__init__.py each self-register at
 *      import; in C the bundled set registers via one init call)
 *
 * PoP: provider_profile @ providers/base.py:ProviderProfile
 * PoP: provider_profile @ providers/__init__.py:register_provider
 * PoP: provider_profile @ providers/__init__.py:get_provider_profile
 * PoP: provider_profile @ providers/__init__.py:list_providers
 */
#ifndef PROVIDER_PROFILE_H
#define PROVIDER_PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sentinel for "omit temperature entirely" (Kimi: server manages it).
 * Python: OMIT_TEMPERATURE = object(); fixed_temperature is Any.
 * C: fixed_temperature_mode selects between the three states. */
typedef enum {
    PROFILE_TEMP_DEFAULT = 0,   /* None: use caller's temperature */
    PROFILE_TEMP_FIXED,         /* send fixed_temperature value */
    PROFILE_TEMP_OMIT           /* OMIT_TEMPERATURE: don't send at all */
} profile_temp_mode_t;

typedef struct provider_profile provider_profile_t;

/* Hook: provider-specific message preprocessing.
 * messages_json is a JSON array string; return a malloc'd JSON array string
 * (caller frees) or NULL for pass-through. */
typedef char *(*profile_prepare_messages_fn)(provider_profile_t *p,
                                             const char *messages_json);

/* Hook: provider-specific extra_body fields.
 * ctx_json carries {session_id, provider_preferences, model, base_url,
 * reasoning_config, openrouter_min_coding_score, ...}. Returns malloc'd
 * JSON object string (caller frees) or NULL for {}. */
typedef char *(*profile_build_extra_body_fn)(provider_profile_t *p,
                                             const char *ctx_json);

/* Hook: provider-specific kwargs split between extra_body and top-level.
 * ctx_json carries {reasoning_config, supports_reasoning, model, base_url,
 * qwen_session_metadata, ollama_num_ctx, session_id}. On return,
 * *out_extra_body and *out_top_level are malloc'd JSON object strings
 * (either may be NULL for {}). */
typedef void (*profile_build_api_kwargs_extras_fn)(provider_profile_t *p,
                                                   const char *ctx_json,
                                                   char **out_extra_body,
                                                   char **out_top_level);

/* Hook: per-model default max_tokens cap. Return <=0 for "no cap"
 * (falls back to default_max_tokens). */
typedef int (*profile_get_max_tokens_fn)(provider_profile_t *p,
                                         const char *model);

/* Hook: default vision model id (malloc'd, caller frees) or NULL. */
typedef char *(*profile_default_vision_model_fn)(provider_profile_t *p);

/* Hook: fetch live model list. Returns malloc'd JSON array of id strings
 * (caller frees) or NULL. Default impl GETs {models_url|base_url/models}
 * with Bearer auth + default_headers. */
typedef char *(*profile_fetch_models_fn)(provider_profile_t *p,
                                         const char *api_key,
                                         const char *base_url,
                                         double timeout);

struct provider_profile {
    /* ── Identity ─────────────────────────────────────────── */
    char *name;
    char *api_mode;              /* default "chat_completions" */
    char **aliases;              /* NULL-terminated */

    /* ── Human-readable metadata ──────────────────────────── */
    char *display_name;
    char *description;
    char *signup_url;

    /* ── Auth & endpoints ─────────────────────────────────── */
    char **env_vars;             /* NULL-terminated */
    char *base_url;
    char *models_url;
    char *auth_type;             /* default "api_key" */
    bool supports_health_check;  /* default true */

    /* ── Vision support ───────────────────────────────────── */
    bool supports_vision;                 /* default false */
    bool supports_vision_tool_messages;   /* default true */

    /* ── Model catalog ────────────────────────────────────── */
    char **fallback_models;      /* NULL-terminated */
    char *hostname;              /* derived from base_url when empty */

    /* ── Client-level quirks ──────────────────────────────── */
    char *default_headers_json;  /* JSON object string or NULL */

    /* ── Request-level quirks ─────────────────────────────── */
    profile_temp_mode_t fixed_temperature_mode;
    double fixed_temperature;    /* meaningful when mode==FIXED */
    int default_max_tokens;      /* <=0 = unset */
    char *default_aux_model;

    /* ── Hooks (NULL = base-class default) ────────────────── */
    profile_prepare_messages_fn        prepare_messages;
    profile_build_extra_body_fn        build_extra_body;
    profile_build_api_kwargs_extras_fn build_api_kwargs_extras;
    profile_get_max_tokens_fn          get_max_tokens;
    profile_default_vision_model_fn    default_vision_model;
    profile_fetch_models_fn            fetch_models;

    void *hook_state;            /* per-profile private state (owned) */
};

/* ── construction / registry ──────────────────────────────────────────── */

/* Allocate a zeroed profile with base-class defaults applied
 * (api_mode="chat_completions", auth_type="api_key",
 *  supports_health_check=true, supports_vision_tool_messages=true). */
provider_profile_t *provider_profile_new(const char *name);
void provider_profile_free(provider_profile_t *p);

/* Register by name + aliases (later registrations replace earlier — user
 * overrides win, mirroring register_provider()). Takes ownership. */
void provider_profile_register(provider_profile_t *p);

/* Look up by name or alias; NULL when the provider has no profile. */
provider_profile_t *provider_profile_get(const char *name);

/* All registered profiles (deduped). Returns malloc'd array of borrowed
 * pointers; caller frees the array only. *out_n receives the count. */
provider_profile_t **provider_profile_list(size_t *out_n);

/* Register the bundled profile set (plugins/model-providers/). Idempotent —
 * mirrors lazy _discover_providers(). Called automatically by
 * provider_profile_get()/list() on first use. */
void provider_profiles_register_builtin(void);

/* Marker used by provider_profile.h to flip the discovery latch. */
void provider_profile_mark_builtin_done(void);

/* ── base-class hook defaults (callable directly, used by subclasses) ─── */

/* get_hostname: self.hostname or urlparse(base_url).hostname. malloc'd. */
char *provider_profile_get_hostname(provider_profile_t *p);

/* Default fetch_models: GET {models_url | base_url/models} with Bearer auth,
 * Accept: application/json, hermes-cli UA, default_headers. Returns malloc'd
 * JSON array of model-id strings, or NULL. */
char *provider_profile_fetch_models_default(provider_profile_t *p,
                                            const char *api_key,
                                            const char *base_url,
                                            double timeout);

/* ── integration glue (provider_profile_apply.c) ───────────────────────── */
/* Apply a ProviderProfile's per-provider request quirks onto an already-built
 * chat-completions request body `root`. Called by provider_openai.c. Providers
 * without a registered profile are left untouched. `root` may be mutated. */
typedef struct provider_t provider_t;   /* forward decl */
void apply_provider_profile(const provider_t *p, struct json_t *root);

/* ── shared helper (chat_completions.py:_reasoning_config_for_model) ──── */
/* Returns malloc'd JSON of the wire-compatible reasoning config (or NULL
 * when input is NULL). gpt-5.6 + effort=ultra -> effort=max. */
char *profile_reasoning_config_for_model(const char *model,
                                         const char *reasoning_config_json);

#ifdef __cplusplus
}
#endif

#endif /* PROVIDER_PROFILE_H */
