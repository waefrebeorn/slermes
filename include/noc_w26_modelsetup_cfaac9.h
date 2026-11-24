/*
 * noc_w26_modelsetup_cfaac9.h - C surface for upstream
 * hermes_cli/model_setup_flows_azure.py (gate w26_modelsetup_cfaac9).
 *
 * The Azure Foundry wizard's decisions: which transport label a mode carries,
 * the active settings shown as prompt defaults, the model/deployment the user
 * ends up with, and the transport picked when detection is incomplete.  The
 * prompts, the .env read and the endpoint probe are seams -- answers and probe
 * results arrive as values, and only what those values decide is ported here.
 */
#ifndef NOC_W26_MODELSETUP_CFAAC9_H
#define NOC_W26_MODELSETUP_CFAAC9_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _azure_mode_label: "OpenAI-style" for chat_completions, else Anthropic-style. */
json_t *w26_modelsetup_cfaac9_mode_label(const char *mode);

/* _azure_current(config) -> {base_url, api_mode, auth_mode, entra, api_key};
 * state: {env_key: <AZURE_FOUNDRY_API_KEY or null>, config: {...}}. */
json_t *w26_modelsetup_cfaac9_current(const json_t *state);

/* _azure_pick_model(discovered_models, current_model) -> model name or null,
 * with the answer scripted: state {models, current, answer}. */
json_t *w26_modelsetup_cfaac9_pick_model(const json_t *state);

/* _azure_detect_transport's decision -> [api_mode, discovered_models] or null;
 * state {detected: {api_mode, models, reason}, current_api_mode, answer}. */
json_t *w26_modelsetup_cfaac9_transport(const json_t *state);

/* The module's literal constants, as an object. */
json_t *w26_modelsetup_cfaac9_constants(void);

#ifdef __cplusplus
}
#endif
#endif
