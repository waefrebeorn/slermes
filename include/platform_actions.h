/*
 * platform_actions.h — Port of hermes_cli/platform_actions.py.
 *
 * Capability-gated platform action facade for plugins (#64176, action half).
 * Every verb checks the "gateway.platform_actions" capability at call time
 * (fail closed, default OFF), validates required string args, resolves the
 * live gateway adapter, dispatches per platform and audits every action.
 * Results are structured dicts — {"ok": true, ...} or
 * {"ok": false, "error": <code>, "detail": <str>} — and never raise.
 *
 * The gateway runner / adapters / consent registry are live systems: the C
 * port keeps the full decision plumbing faithful and routes the side effects
 * through injectable hooks (capability check, runner probe, adapter vtable),
 * exactly the shape Python's imports provide at runtime.
 */
#ifndef PLATFORM_ACTIONS_H
#define PLATFORM_ACTIONS_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLATFORM_ACTIONS_CONTRACT_VERSION 1
#define PLATFORM_ACTIONS_CAPABILITY_ID "gateway.platform_actions"

/* PoP: adapter (gateway platform adapter) — the vtable the verbs dispatch
 * through. `platform_value` mirrors adapter.platform.value ("telegram",
 * "discord", ...). Negative returns model raised exceptions; the verbs
 * convert them to {"ok":false,"error":"action_failed",...}. */
typedef struct pa_adapter {
    const char *platform_value;
    int is_connected;
    void *impl;
    /* telegram: set_message_reaction (sets/replaces, returns truthy/falsy) */
    int (*set_reaction)(void *impl, const char *chat_id,
                        const char *message_id, const char *emoji);
    /* telegram: edit_forum_topic — chat-scoped */
    int (*rename_dm_topic)(void *impl, const char *chat_id, long thread_id,
                           const char *title);
    /* discord: thread rename (returns truthy/falsy) */
    int (*rename_thread)(void *impl, const char *thread_id,
                         const char *title);
    /* discord client (adapter._client) — NULL models client unavailable */
    void *client;
    void *(*get_channel)(void *client, long channel_id);
    void *(*fetch_channel)(void *client, long channel_id); /* NULL = raised */
    void *(*fetch_message)(void *channel_or_client, void *channel, long msg_id);
    int (*add_reaction)(void *channel_or_client, void *message,
                        const char *emoji);
} pa_adapter_t;

/* PoP: PlatformActions facade. Instances are cheap and hold only the owning
 * plugin id; the gateway runner and adapters resolve at call time. */
typedef struct platform_actions {
    const char *plugin_id;
    /* PoP: hermes_cli.plugin_capabilities.plugin_capability_granted */
    int (*capability_granted)(const char *plugin_id, const char *cap_id,
                              void *ctx);
    void *cap_ctx;
    /* PoP: gateway.run._gateway_runner_ref — NULL return = no active runner */
    void *(*get_runner)(void *ctx);
    void *runner_ctx;
    /* PoP: runner.adapters[Platform(value)] — NULL = not registered.
     * `platform_value` is the normalized (stripped, lowercased) name. */
    const pa_adapter_t *(*runner_adapter)(void *runner,
                                          const char *platform_value);
    void *adapter_ctx;
    /* PoP: gateway.config.Platform enum membership (stripped/lowercased
     * names that are valid enum values). */
    const char *const *known_platforms;
    size_t n_known_platforms;
} platform_actions_t;

platform_actions_t *platform_actions_new(const char *plugin_id);
void platform_actions_free(platform_actions_t *pa);

/* PoP: PlatformActions._capability_granted — failure to read consent state
 * means NOT granted. */
int platform_actions_capability_granted(const platform_actions_t *pa);

/* PoP: PlatformActions._err / _ok — structured result dicts (json_t*, caller
 * frees with json_free). */
json_t *platform_actions_err(const char *code, const char *detail);
json_t *platform_actions_ok_verb(const char *verb);

/* PoP: PlatformActions._resolve_adapter — returns (adapter, error_dict);
 * exactly one is non-NULL. *out_err is a json_t* the caller frees. */
const pa_adapter_t *platform_actions_resolve_adapter(
    const platform_actions_t *pa, const char *platform, json_t **out_err);

/* PoP: PlatformActions._gate — capability + required-string checks +
 * adapter resolution. */
const pa_adapter_t *platform_actions_gate(const platform_actions_t *pa,
                                          const char *platform,
                                          const char *const *required_names,
                                          const char *const *required_values,
                                          size_t n_required, json_t **out_err);

/* PoP: PlatformActions.add_reaction — v1 verb. Returns a structured result
 * dict (json_t*, caller frees); never raises. */
json_t *platform_actions_add_reaction(const platform_actions_t *pa,
                                      const char *platform,
                                      const char *chat_id,
                                      const char *message_id,
                                      const char *emoji);

/* PoP: PlatformActions.set_thread_title — v1 verb. */
json_t *platform_actions_set_thread_title(const platform_actions_t *pa,
                                          const char *platform,
                                          const char *chat_id,
                                          const char *thread_id,
                                          const char *title);

/* PoP: PlatformActions._discord_add_reaction (static helper). */
json_t *platform_actions_discord_add_reaction(const pa_adapter_t *adapter,
                                              const char *chat_id,
                                              const char *message_id,
                                              const char *emoji);

/* PoP: PlatformActions._audit — every platform action is logged (the
 * #64176 'all actions logged' rule). Returns the malloc'd log line
 * (caller frees). */
char *platform_actions_audit(const platform_actions_t *pa, const char *verb,
                             const char *platform, json_t *result);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_ACTIONS_H */
