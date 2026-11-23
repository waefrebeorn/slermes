#ifndef NOC_W13_PROVIDERRO_DAA14D_H
#define NOC_W13_PROVIDERRO_DAA14D_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_providerro_daa14d_freeze @ agent/provider_route_hooks.py:_freeze
 * Canonical value freeze: dict -> sorted [[key, frozen], ...] array, list/tuple
 * -> frozen array, scalar -> itself. */
struct json_t *w13_providerro_daa14d_freeze(const struct json_t *value);

/* PoP: w13_providerro_daa14d_forces_max @ agent/provider_route_hooks.py:model_forces_max_completion_tokens
 * (model or "").strip().lower().rsplit("/", 1)[-1] starts with one of the
 * OpenAI families that reject max_tokens. */
int w13_providerro_daa14d_forces_max(const char *model);

/* PoP: w13_providerro_daa14d_route @ agent/provider_route_hooks.py:route_profiles
 * Profiles overriding `hook`, named then host-served, projected as an array of
 * profile names. `profiles` is an array of profile objects; `providers` maps
 * provider name -> profile name; `hosts` maps base_url -> profile name (the
 * profile_for_host seam). */
struct json_t *w13_providerro_daa14d_route(const char *hook, const char *provider,
                                           const char *base_url, const struct json_t *profiles,
                                           const struct json_t *providers,
                                           const struct json_t *hosts);

/* PoP: w13_providerro_daa14d_ask @ agent/provider_route_hooks.py:ask_profile
 * One profile hook answer with the sticky-failure policy. `scope` is null
 * (one-shot) or an object with `_provider_hook_failures` (array of [name,hook]
 * pairs); `answers` maps hook -> answer, `raises` lists hooks that raise.
 * Result is {ret, failures} with the (possibly updated) sorted failures. */
struct json_t *w13_providerro_daa14d_ask(const struct json_t *profile, const char *hook,
                                         const struct json_t *fallback,
                                         const struct json_t *scope,
                                         const struct json_t *answers,
                                         const struct json_t *raises);

/* PoP: w13_providerro_daa14d_hook_failed @ agent/provider_route_hooks.py:hook_failed */
int w13_providerro_daa14d_hook_failed(const struct json_t *profile, const char *hook,
                                      const struct json_t *scope);

/* PoP: w13_providerro_daa14d_model_api_mode @ agent/provider_route_hooks.py:model_api_mode
 * `profile` has api_mode, model_mode_answer and model_mode_raises; a raising
 * api_mode_for_model answers profile.api_mode or "chat_completions". */
struct json_t *w13_providerro_daa14d_model_api_mode(const struct json_t *profile,
                                                    const char *model);

/* PoP: w13_providerro_daa14d_route_answer @ agent/provider_route_hooks.py:route_answer
 * First truthy answer across route_profiles, following ask_profile's sticky
 * failure policy. Each profile object carries its own answers/raises maps.
 * Result is {ret, failures}. */
struct json_t *w13_providerro_daa14d_route_answer(const char *hook, const char *provider,
                                                  const char *base_url,
                                                  const struct json_t *profiles,
                                                  const struct json_t *providers,
                                                  const struct json_t *hosts,
                                                  const struct json_t *scope);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_PROVIDERRO_DAA14D_H */
