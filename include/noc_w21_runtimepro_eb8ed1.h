#ifndef NOC_W21_RUNTIMEPRO_EB8ED1_H
#define NOC_W21_RUNTIMEPRO_EB8ED1_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/runtime_provider_missing_key.py (gate w21_runtimepro_eb8ed1):
 * a provider key the active profile's secret scope does not have, said before
 * any request.
 *
 * Custom resolution substitutes `no-key-required` for an empty key so keyless
 * local servers work. When the endpoint's config DECLARES a key variable and it
 * resolves to nothing under a routed (named) profile, the request can only be
 * refused. The error names the provider, the variable, the profile and the
 * command that puts the key where it is read.
 *
 * Seams, all upstream state the module reaches for:
 *   - `agent.custom_entry_pool` name/URL matching runs over the compatible
 *     custom-provider view (`entries`, `get_compatible_custom_providers`);
 *   - `agent.secret_scope.get_secret_str` answers from `secrets`;
 *   - `hermes_constants` profile/home/selector and `serves_routed_profile`
 *     answer from `env`; the process environment's own copy of the variable is
 *     `env.vars`. */

/* PoP: w21_runtimepro_eb8ed1_declared_key_env @ hermes_cli/runtime_provider_missing_key.py:_declared_key_env
 * `str(entry.get("key_env") or entry.get("api_key_env") or "").strip()`; a
 * non-dict entry (or absent) reads as "". Caller frees. */
char *w21_runtimepro_eb8ed1_declared_key_env(const json_t *entry);

/* PoP: w21_runtimepro_eb8ed1_declared_custom_key @ hermes_cli/runtime_provider_missing_key.py:_declared_custom_key
 * `(label, key_env)` of the config block that declared a key variable for this
 * custom endpoint, else ("", ""): the owning custom entry (by name when the
 * runtime came from a named entry, else by URL), then `model.key_env` when
 * `model.base_url` is this endpoint.
 *   {"label":..., "key_env":...}
 * `entries` is the compatible custom-provider view (dicts with name,
 * base_url, key_env / api_key_env, provider_key). */
json_t *w21_runtimepro_eb8ed1_declared_custom_key(const char *requested_provider,
                                                   const json_t *runtime,
                                                   const json_t *model_cfg,
                                                   const json_t *entries);

/* PoP: w21_runtimepro_eb8ed1_where_to_set_key @ hermes_cli/runtime_provider_missing_key.py:where_to_set_key
 * The sentence naming the profile whose `.env` must hold `env_var` and the
 * command that writes it there; it leads with the "set in this server's
 * environment only" note when the variable is exported here and a routed
 * profile is served.
 *   profile       -- `profile_name_for_home(get_hermes_home()) or "default"`
 *   display_home  -- `display_hermes_home()`
 *   cli_selector  -- `profile_cli_selector()` ("" or "--profile <name> ")
 *   serves_routed -- `serves_routed_profile()`
 *   env_has_var   -- `os.environ.get(env_var)` truthiness
 * Caller frees. */
char *w21_runtimepro_eb8ed1_where_to_set_key(const char *env_var, const char *profile,
                                             const char *display_home, const char *cli_selector,
                                             int serves_routed, int env_has_var);

/* PoP: w21_runtimepro_eb8ed1_named_profile_key_hint @ hermes_cli/runtime_provider_missing_key.py:named_profile_key_hint
 * `" <where_to_set_key>"` for a registry provider's missing key while a NAMED
 * profile is active, else "": the default profile also reads the process
 * environment, so the plain "Set X." already says everything.
 *   env_vars -- the provider's declared variable names
 *   profile  -- the active profile name, or NULL/"default" for the default one
 * Caller frees. */
char *w21_runtimepro_eb8ed1_named_profile_key_hint(const json_t *env_vars, const char *profile,
                                                   const char *display_home, const char *cli_selector,
                                                   int serves_routed, int env_has_var);

/* PoP: w21_runtimepro_eb8ed1_raise_if_declared_key_missing @ hermes_cli/runtime_provider_missing_key.py:raise_if_declared_key_missing
 * The refusal, without the exception: refuse a custom runtime carrying the
 * placeholder when its config declared a key variable that resolves to nothing
 * under a routed profile. A caller-supplied key, a transport that never sends
 * the key (`codex_app_server`, `bedrock_converse`), an unrouted profile and a
 * block that declares no key variable all pass.
 *   {"status":"ok"} | {"status":"error","type":"AuthError",
 *    "message":..., "provider":..., "code":"missing_api_key"}
 * `secrets` maps a variable name to what `get_secret_str(name, "")` returns;
 * `env` carries serves_routed/profile/display_home/cli_selector/vars. */
json_t *w21_runtimepro_eb8ed1_raise_if_declared_key_missing(const char *requested_provider,
                                                            const json_t *runtime,
                                                            const json_t *model_cfg,
                                                            const json_t *entries,
                                                            const json_t *secrets,
                                                            const json_t *explicit_api_key,
                                                            const json_t *env);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_RUNTIMEPRO_EB8ED1_H */
