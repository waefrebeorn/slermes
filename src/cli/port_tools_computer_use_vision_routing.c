/*
 * port_tools_computer_use_vision_routing.c — C port of tools/computer_use/vision_routing.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_computer_use_vision_routing__lookup_user_declared_supports_vision @ tools/computer_use/vision_routing.py:_lookup_user_declared_supports_vision */

/* Port of Python tools/computer_use/vision_routing.py:_lookup_user_declared_supports_vision */
/* Return config-declared supports_vision for the active route. */
/* Returns: 1=true, 0=false, -1=not found/error */
int cli_tools_computer_use_vision_routing__lookup_user_declared_supports_vision(
    const char *provider, const char *model,
    const char **cfg_keys, const char **cfg_values, int cfg_count)
{
    if (!provider || !model) return -1;

    /* Look for image_routing.supports_vision.<provider>.<model> in config */
    char lookup_key[512];
    snprintf(lookup_key, sizeof(lookup_key),
        "image_routing.supports_vision.%s.%s", provider, model);

    for (int i = 0; i < cfg_count; i++) {
        if (cfg_keys[i] && strcmp(cfg_keys[i], lookup_key) == 0) {
            if (cfg_values[i]) {
                if (strcmp(cfg_values[i], "true") == 0 || strcmp(cfg_values[i], "1") == 0) {
                    return 1;
                }
                return 0;
            }
        }
    }

    return -1; /* not configured */
}

/* PoP: cli_tools_computer_use_vision_routing__provider_accepts_multimodal_tool_result @ tools/computer_use/vision_routing.py:_provider_accepts_multimodal_tool_result */

/* Port of Python tools/computer_use/vision_routing.py:_provider_accepts_multimodal_tool_result */
/* Return whether provider+model carries images inside tool-result messages. */
int cli_tools_computer_use_vision_routing__provider_accepts_multimodal_tool_result(
    const char *provider, const char *model)
{
    if (!provider || !*provider) return -1;

    /* Known providers that accept multimodal tool results */
    /* In a full implementation, this would query tools.vision_tools */
    /* For now, use a simplified lookup based on known provider capabilities */

    /* Anthropic models accept multimodal tool results */
    if (strcmp(provider, "anthropic") == 0) {
        return 1;
    }

    /* OpenAI GPT-4o and later accept multimodal */
    if (strcmp(provider, "openai") == 0 || strcmp(provider, "openai-codex") == 0) {
        if (model && (strstr(model, "gpt-4o") || strstr(model, "gpt-4.1") ||
                       strstr(model, "o1") || strstr(model, "o3") || strstr(model, "o4"))) {
            return 1;
        }
        return 0;
    }

    /* Google Gemini accepts multimodal */
    if (strcmp(provider, "google") == 0 || strcmp(provider, "gemini") == 0) {
        return 1;
    }

    return -1; /* unknown — caller should fall back to aux routing */
}

/* PoP: cli_tools_computer_use_vision_routing_should_route_capture_to_aux_vision @ tools/computer_use/vision_routing.py:should_route_capture_to_aux_vision */

/* Port of Python tools/computer_use/vision_routing.py:should_route_capture_to_aux_vision */
/* Return 1 iff the captured screenshot should be pre-analysed via aux vision. */
int cli_tools_computer_use_vision_routing_should_route_capture_to_aux_vision(
    const char *provider, const char *model,
    const char **cfg_keys, const char **cfg_values, int cfg_count)
{
    if (!provider || !model) return 0;

    /* 1. Check user-declared supports_vision override */
    int declared = cli_tools_computer_use_vision_routing__lookup_user_declared_supports_vision(
        provider, model, cfg_keys, cfg_values, cfg_count);
    if (declared == 1) return 0;  /* main model handles vision natively */
    if (declared == 0) return 1;  /* explicitly disabled, use aux */

    /* 2. Check if provider accepts multimodal tool results */
    int accepts_multimodal = cli_tools_computer_use_vision_routing__provider_accepts_multimodal_tool_result(
        provider, model);
    if (accepts_multimodal == 1) return 0;  /* main model handles it */

    /* 3. Default: route to aux vision */
    return 1;
}
