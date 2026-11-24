/*
 * noc_w31_localenvpo_3ff534.h - C surface for upstream
 * tools/environments/local_env_policy.py (gate w31_localenvpo_3ff534).
 *
 * The pure value->value core of the child-process secret-scrub policy: name
 * classification (which env names are Hermes credentials, dynamic internal
 * secrets or platform authorization gates), the two raw-config walks that
 * derive credential names from config shapes, camelCase field folding, and the
 * two dict rewrites (supervisor-identity strip, profile-gate strip).
 *
 * Everything that reads process/global state - os.environ, the provider and
 * adapter registries, OPTIONAL_ENV_VARS, the per-home plugin manifests, the
 * secret-source registry, the live session ContextVar - stays in the caller;
 * the sets those reads produce arrive here as plain JSON arrays of names.  No
 * function in this header performs I/O.
 */
#ifndef NOC_W31_LOCALENVPO_3FF534_H
#define NOC_W31_LOCALENVPO_3FF534_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* str(key) with camelCase folded to snake_case (_snake_field). */
json_t *w31_localenvpo_3ff534_snake_field(const json_t *key);

/* _matches_terminal_first_party_prefix: name.upper() starts with "BUZZ_". */
json_t *w31_localenvpo_3ff534_matches_terminal_first_party_prefix(const json_t *name);

/* _is_hermes_internal_secret: dynamic-name Hermes credential. */
json_t *w31_localenvpo_3ff534_is_hermes_internal_secret(const json_t *key);

/* _is_provider_env_blocklisted(name, _registered): `extra` carries the
 * import-derived members of _HERMES_PROVIDER_ENV_BLOCKLIST (provider registry,
 * OPTIONAL_ENV_VARS, left-core, secret-source bootstrap, adapter secrets);
 * `registered` is the per-profile set the caller passes as `_registered`.
 * Both may be null (treated as empty). */
json_t *w31_localenvpo_3ff534_is_provider_env_blocklisted(const json_t *name,
                                                           const json_t *extra,
                                                           const json_t *registered);

/* is_profile_gate_env(name, _prefixes): `prefixes` null falls back to the
 * module's own static owner set (GATEWAY, QQ). */
json_t *w31_localenvpo_3ff534_is_profile_gate_env(const json_t *name,
                                                  const json_t *prefixes);

/* carry_gateway_profile_pin(env): a copy of `env`, stamped with
 * HERMES_GATEWAY_PROFILE_PIN=1 when it still holds a pinning supervisor marker. */
json_t *w31_localenvpo_3ff534_carry_gateway_profile_pin(const json_t *env);

/* strip_supervised_process_identity(env): the pin survives, the supervisor
 * identity names do not. */
json_t *w31_localenvpo_3ff534_strip_supervised_process_identity(const json_t *env);

/* strip_profile_gate_env(env): every is_profile_gate_env key dropped. */
json_t *w31_localenvpo_3ff534_strip_profile_gate_env(const json_t *env,
                                                     const json_t *prefixes);

/* _config_declared_key_envs(node): sorted, de-duplicated env names every
 * credential-pointer field (key_env/api_key_env/token_env/...) points at. */
json_t *w31_localenvpo_3ff534_config_declared_key_envs(const json_t *node);

/* _config_credential_env_refs(node): sorted, de-duplicated env names every
 * credential field references as ${VAR} / ${env:VAR}; mcp_servers skipped. */
json_t *w31_localenvpo_3ff534_config_credential_env_refs(const json_t *node);

/* The module's own literal tables, as an object. */
json_t *w31_localenvpo_3ff534_constants(void);

#ifdef __cplusplus
}
#endif
#endif
