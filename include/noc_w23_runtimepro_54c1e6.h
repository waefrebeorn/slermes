/*
 * noc_w23_runtimepro_54c1e6.h - C surface for upstream
 * hermes_cli/runtime_provider_backends.py (gate w23_runtimepro_54c1e6).
 *
 * Provider-specific runtime builders for hermes_cli.runtime_provider: Azure
 * Foundry, the OpenRouter / bare-custom fallback resolver, AWS Bedrock and the
 * external-process (agent CLI over stdio) providers.
 *
 * The module's own contract is that origin-internal collaborators are resolved
 * on `hermes_cli.runtime_provider` at call time through `_rp()` "so test
 * patches still apply". That is exactly the seam here: `world.rp` carries what
 * each `rp.*` / `auth` / `runtime_provider_custom` collaborator answers
 * (config reads, api-mode parsing, the runtime builder, credential-pool
 * lookups, host-gated env keys), and `world.secrets` / `world.dotenv` carry
 * `get_secret_str` and the config `.env` reads. Everything this module itself
 * decides -- precedence, coercion, the `/v1` strip, the auth-mode branch, key
 * selection order, source labels, the OpenRouter-context test, the Bedrock
 * routing -- is ported here.
 *
 * Every builder returns the same envelope so a refusal is data, not an abort:
 *   {"status":"ok","runtime":{provider,api_mode,base_url,api_key,...}}
 *   {"status":"error","type":"AuthError","message":...,"code":...}
 */
#ifndef NOC_W23_RUNTIMEPRO_54C1E6_H
#define NOC_W23_RUNTIMEPRO_54C1E6_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w23_runtimepro_54c1e6_azure_foundry @ hermes_cli/runtime_provider_backends.py:_resolve_azure_foundry_runtime
 * Azure Foundry: `model.base_url` + `model.api_mode` (or explicit overrides),
 * API key from `.env`/env or a per-request Entra ID token, trailing `/v1`
 * stripped for Anthropic-style endpoints.
 *   world keys: model_cfg, requested_provider, explicit_api_key,
 *   explicit_base_url, target_model, explicit_is_token_provider, rp{},
 *   secrets{AZURE_FOUNDRY_BASE_URL,AZURE_FOUNDRY_API_KEY}, dotenv{}, azure{}. */
json_t *w23_runtimepro_54c1e6_azure_foundry(const json_t *world);

/* PoP: w23_runtimepro_54c1e6_openrouter @ hermes_cli/runtime_provider_backends.py:_resolve_openrouter_runtime
 * Terminal resolver: OpenRouter, or a bare/aliased `custom` endpoint.
 * base_url precedence: explicit > CUSTOM_BASE_URL > trusted `model.base_url` >
 * OPENROUTER_BASE_URL > default.
 *   world keys: requested_provider, explicit_api_key, explicit_base_url,
 *   model_cfg, rp{}, secrets{}, dotenv{}. */
json_t *w23_runtimepro_54c1e6_openrouter(const json_t *world);

/* PoP: w23_runtimepro_54c1e6_bedrock @ hermes_cli/runtime_provider_backends.py:_resolve_bedrock_runtime
 * AWS Bedrock triple-path routing: bare in-Region OpenAI IDs -> Bedrock
 * Mantle Responses; Claude -> AnthropicBedrock; others -> Converse.
 *   world keys: requested_provider, model_cfg, target_model, aws{bearer,
 *   env_bearer,has_credentials,region,auth_env_var,guardrail,mantle,anthropic},
 *   bedrock_cfg{}, rp{}. */
json_t *w23_runtimepro_54c1e6_bedrock(const json_t *world);

/* PoP: w23_runtimepro_54c1e6_is_external @ hermes_cli/runtime_provider_backends.py:_is_external_process_provider
 * Keyed on the registered provider's auth_type (CLI registry first, then the
 * profile registry).  Returns a JSON bool.
 *   world keys: provider, registry{name:{auth_type}}, registry_error,
 *   profiles{name:{auth_type}}, profiles_error. */
json_t *w23_runtimepro_54c1e6_is_external(const json_t *world);

/* PoP: w23_runtimepro_54c1e6_external_runtime @ hermes_cli/runtime_provider_backends.py:_resolve_external_process_runtime
 * The runtime a registered external-process provider resolves to.
 *   world keys: provider, requested_provider, creds{base_url,api_key,command,
 *   args[],source}, rp{}. */
json_t *w23_runtimepro_54c1e6_external_runtime(const json_t *world);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W23_RUNTIMEPRO_54C1E6_H */
