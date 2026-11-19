/* port_runtime_provider_flow.h — FLOW-core port of hermes_cli/runtime_provider.py.
 *
 * Mission: match the program's FLOW (provider detection/selection decision
 * chains, env/config resolution ordering, fallback branches, error paths) of
 * the runtime-provider resolution helpers. All I/O (env secrets, provider
 * registry lookups, config loads, credential resolvers, URL utilities, pool
 * refresh, logging) is INJECTED through seams — a generic callback dispatcher
 * + json_t state — never performed inside the ported decision core.
 *
 * Seam model
 * ----------
 * Every live external call the ported flows make goes through
 * `rpf_io_t.call(io, name, args)`. The dispatcher:
 *   - appends {"name": name, "args": args} to io->trace (ordered JSON array)
 *     so the decision SEQUENCE can be compared against live Python under
 *     scripted I/O;
 *   - returns the scripted result (a NEW json_t* the caller frees);
 *   - models a raised exception with {"__raise__": "<Type>", "message": "..."}.
 * A result carrying "__raise__" propagates up the decision chain unless the
 * live code catches it (`except Exception` does NOT catch SystemExit /
 * KeyboardInterrupt / GeneratorExit — those propagate, matching Python).
 *
 * Live counterpart of each seam name:
 *   auth.resolve_provider              hermes_cli.auth.resolve_provider(name)
 *   PROVIDER_REGISTRY.get              hermes_cli.auth.PROVIDER_REGISTRY.get(key)
 *   _models.normalize_provider         hermes_cli.models.normalize_provider(p)
 *   _models.azure_foundry_model_api_mode  hermes_cli.models.azure_foundry_model_api_mode(m)
 *   get_provider                       hermes_cli.providers.get_provider(p, allow_network=False)
 *   base_url_path                      utils.base_url_path(url)
 *   base_url_host_matches              utils.base_url_host_matches(url, host)
 *   base_url_hostname                  utils.base_url_hostname(url)
 *   is_actual_route                    hermes_cli.providers.is_actual_route(provider, base_url)
 *   normalize_actual_base_url          hermes_cli.auth.normalize_actual_base_url(base_url)
 *   get_secret_str                     agent.secret_scope.get_secret_str(name, default)
 *   _host_derived_api_key              module sibling (already ported elsewhere; seam here)
 *   env_int                            utils.env_int(name, default)
 *   _agent_key_is_usable               hermes_cli.auth._agent_key_is_usable(state, min_ttl)
 *   pool.try_refresh_current           CredentialPool.try_refresh_current()
 *   load_config                        module load_config() (config I/O)
 *   _config_mod.is_provider_enabled    hermes_cli.config.is_provider_enabled(block)
 *   _get_model_config                  module sibling (already ported elsewhere; seam here)
 *   _resolve_openrouter_runtime        runtime_provider_backends._resolve_openrouter_runtime(**kw)
 *   _resolve_azure_foundry_runtime     runtime_provider_backends._resolve_azure_foundry_runtime(**kw)
 *   _resolve_vertex_runtime            module _resolve_vertex_runtime(requested_provider)
 *   logger.debug                       module logger.debug(fmt, *args) — args {"fmt","args"}
 *   <resolve_name>                     _creds_fallback's injected resolve() callable
 *
 * Data model / representation notes
 * ---------------------------------
 * - Provider-registry entries and pool entries are JSON object descriptors;
 *   getattr(entry, k, default) maps to json_obj_get (absent -> default,
 *   present-null -> None). Registry seam results are null (missing) or a
 *   NON-EMPTY object carrying the attributes read (id / inference_base_url /
 *   base_url): fixtures never return an empty object, whose Python truthiness
 *   (SimpleNamespace) would diverge from JSON object truthiness.
 * - Python str() of scalars: None->"None", True->"True", False->"False",
 *   integral floats/ints < 1e16 -> decimal, other floats -> shortest
 *   round-trip repr. Container str()/repr() values are OUT of fixture scope.
 * - str.strip()/rstrip()/lower() and repr() are ASCII-scoped in the port;
 *   fixtures stay ASCII (Python's are unicode-wide — documented).
 * - Non-finite floats are modeled {"__float__": "inf"|"-inf"|"nan"} on BOTH
 *   sides; integral floats normalize to ints before comparison (oracle).
 * - libjson parses every number as a double; fixture JSON number literals
 *   avoid integral-float forms ("1.0") and leading zeros.
 * - _runtime builds {"provider","api_mode","base_url","api_key", **extra}:
 *   extra keys overwrite in place (position kept) or append, matching dict
 *   update semantics; insertion order is preserved by libjson.
 *
 * Ownership: every rpf_* function returns a NEW json_t* (caller frees).
 * Args passed to rpf_call are borrowed (the dispatcher copies what it
 * records). Inputs are borrowed. _tag mutates in.runtime in place; the
 * harness owns it and echoes it as state.
 */
#ifndef PORT_RUNTIME_PROVIDER_FLOW_H
#define PORT_RUNTIME_PROVIDER_FLOW_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rpf_io_t {
    /* Dispatch a live call by name. Returns a NEW json_t* (caller frees).
     * A result object carrying "__raise__" models a raised exception. */
    json_t *(*call)(struct rpf_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of {"name","args"}: ordered seam sequence */
    void   *ctx;     /* harness backend (scripted results) */
} rpf_io_t;

/* Dispatch a seam call: records {"name","args"} on io->trace, then calls
 * io->call (NULL call => json_null()). args may be NULL (recorded as {}). */
json_t *rpf_call(rpf_io_t *io, const char *name, json_t *args);
/* True when a seam result models a raised exception. */
bool rpf_raised(const json_t *result);

/* Uniform port contract: json in -> json out (NEW node, caller frees). */
json_t *rpf_resolves_to_custom(rpf_io_t *io, json_t *in);
json_t *rpf_cfg_provider(rpf_io_t *io, json_t *in);
json_t *rpf_same_registered_provider(rpf_io_t *io, json_t *in);
json_t *rpf_config_base_url_for_provider(rpf_io_t *io, json_t *in);
json_t *rpf_on_declared_anthropic_endpoint(rpf_io_t *io, json_t *in);
json_t *rpf_azure_inferred_api_mode(rpf_io_t *io, json_t *in);
json_t *rpf_runtime(rpf_io_t *io, json_t *in);
json_t *rpf_creds_fallback(rpf_io_t *io, json_t *in);
json_t *rpf_azure_anthropic_env_key(rpf_io_t *io, json_t *in);
json_t *rpf_openai_env_key_authoritative_for(rpf_io_t *io, json_t *in);
json_t *rpf_host_gated_env_key_candidates(rpf_io_t *io, json_t *in);
json_t *rpf_openrouter_should_use_pool(rpf_io_t *io, json_t *in);
json_t *rpf_raise_if_provider_disabled(rpf_io_t *io, json_t *in);
json_t *rpf_raise_for_credentialless_bare_custom(rpf_io_t *io, json_t *in);
/* State-mutating port: in.runtime is mutated in place (requested_provider). */
json_t *rpf_tag(rpf_io_t *io, json_t *in);
json_t *rpf_local_endpoint_bypass(rpf_io_t *io, json_t *in);
json_t *rpf_resolve_requested_shortcuts(rpf_io_t *io, json_t *in);
json_t *rpf_refresh_nous_pool_entry(rpf_io_t *io, json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* PORT_RUNTIME_PROVIDER_FLOW_H */