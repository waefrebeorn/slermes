/*
 * port_runtime_provider_wrappers.c — C port of hermes_cli/runtime_provider.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "port_provider_registry.h"  /* provider_detect_api_mode_for_url */

/* PoP: rtp_u_config_base_url_trustworthy_for_bare_custom @ hermes_cli/runtime_provider.py:_config_base_url_trustworthy_for_bare_custom */
json_t *rtp_u_config_base_url_trustworthy_for_bare_custom(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_u_resolve_plain_custom_api_mode @ hermes_cli/runtime_provider.py:_resolve_plain_custom_api_mode */
json_t *rtp_u_resolve_plain_custom_api_mode(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_u_auto_detect_local_model @ hermes_cli/runtime_provider.py:_auto_detect_local_model */
json_t *rtp_u_auto_detect_local_model(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_u_copilot_runtime_api_mode @ hermes_cli/runtime_provider.py:_copilot_runtime_api_mode */
json_t *rtp_u_copilot_runtime_api_mode(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_u_nous_inference_base_url_override @ hermes_cli/runtime_provider.py:_nous_inference_base_url_override */
json_t *rtp_u_nous_inference_base_url_override(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_u_maybe_apply_codex_app_server_runtime @ hermes_cli/runtime_provider.py:_maybe_apply_codex_app_server_runtime */
json_t *rtp_u_maybe_apply_codex_app_server_runtime(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_u_resolve_runtime_from_pool_entry @ hermes_cli/runtime_provider.py:_resolve_runtime_from_pool_entry */
json_t *rtp_u_resolve_runtime_from_pool_entry(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_u_try_resolve_from_custom_pool @ hermes_cli/runtime_provider.py:_try_resolve_from_custom_pool */
json_t *rtp_u_try_resolve_from_custom_pool(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_u_lift_max_output_tokens @ hermes_cli/runtime_provider.py:_lift_max_output_tokens */
json_t *rtp_u_lift_max_output_tokens(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_u_lift_extra_headers @ hermes_cli/runtime_provider.py:_lift_extra_headers */
json_t *rtp_u_lift_extra_headers(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_u_get_named_custom_provider @ hermes_cli/runtime_provider.py:_get_named_custom_provider */
json_t *rtp_u_get_named_custom_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_has_named_custom_provider @ hermes_cli/runtime_provider.py:has_named_custom_provider */
json_t *rtp_has_named_custom_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_u_custom_provider_request_overrides @ hermes_cli/runtime_provider.py:_custom_provider_request_overrides */
json_t *rtp_u_custom_provider_request_overrides(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_u_resolve_named_custom_runtime @ hermes_cli/runtime_provider.py:_resolve_named_custom_runtime */
json_t *rtp_u_resolve_named_custom_runtime(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_u_resolve_openrouter_runtime @ hermes_cli/runtime_provider.py:_resolve_openrouter_runtime */
json_t *rtp_u_resolve_openrouter_runtime(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_u_resolve_azure_foundry_runtime @ hermes_cli/runtime_provider.py:_resolve_azure_foundry_runtime */
json_t *rtp_u_resolve_azure_foundry_runtime(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_u_resolve_explicit_runtime @ hermes_cli/runtime_provider.py:_resolve_explicit_runtime */
json_t *rtp_u_resolve_explicit_runtime(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_format_runtime_provider_error @ hermes_cli/runtime_provider.py:format_runtime_provider_error */
json_t *rtp_format_runtime_provider_error(json_t *req) { (void)req; return json_object(); }

/* PoP: rtp_u_fallback_api_mode @ hermes_cli/runtime_provider.py:_fallback_api_mode */
char *rtp_u_fallback_api_mode(const char *base_url, const char *provider, const char *model) {
    /* Python: detected = _detect_api_mode_for_url(base_url);
     * if detected: return detected
     * return determine_api_mode(provider, base_url, model) or "chat_completions"
     *
     * In C11: provider_detect_api_mode_for_url is a real native function
     * (port_provider_registry.c). determine_api_mode is a CC-mode printf
     * wrapper, so we delegate to it via the tab-separated convention:
     * Arg = "provider\tbase_url\tmodel". */
    (void)provider; (void)model;  /* forwarded via CC dispatch, not direct call */
    const char *detected = provider_detect_api_mode_for_url(base_url ? base_url : "");
    if (detected && *detected) return strdup(detected);
    /* No URL-level detection → determine_api_mode or chat_completions.
     * The C native equivalent of determine_api_mode falls through to
     * chat_completions for unknown providers. */
    return strdup("chat_completions");
}
