/*
 * port_agent_image_gen_provider.c — C port of agent/image_gen_provider.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_image_gen_provider_display_name @ agent/image_gen_provider.py:display_name */

/* Port of Python agent/image_gen_provider.py:display_name */
/* Return the display name for an image generation provider. */
const char *cli_agent_image_gen_provider_display_name(const char *provider_id)
{
    if (!provider_id || !provider_id[0]) return "Image Generation";

    if (strcmp(provider_id, "fal") == 0) return "FAL.ai Image Generation";
    if (strcmp(provider_id, "openai") == 0) return "OpenAI DALL-E";
    if (strcmp(provider_id, "stability") == 0) return "Stability AI";
    if (strcmp(provider_id, "replicate") == 0) return "Replicate";

    hermes_log(LOG_DEBUG, "port",
               "image_gen_provider: unknown provider '%s', using default", provider_id);
    return "Image Generation";
}

/* PoP: cli_agent_image_gen_provider_list_models @ agent/image_gen_provider.py:list_models */

/* Port of Python agent/image_gen_provider.py:list_models */
/* List available image generation models. Returns JSON array. */
char *cli_agent_image_gen_provider_list_models(const char *provider_id)
{
    if (!provider_id || !provider_id[0]) {
        return strdup("[\"dall-e-3\",\"dall-e-2\",\"flux-1.1-pro\",\"sdxl\"]");
    }

    if (strcmp(provider_id, "fal") == 0) {
        return strdup("[\"flux-1.1-pro\",\"flux-schnell\",\"sdxl\",\"playground-v2\"]");
    }
    if (strcmp(provider_id, "openai") == 0) {
        return strdup("[\"dall-e-3\",\"dall-e-2\"]");
    }

    hermes_log(LOG_DEBUG, "port",
               "image_gen_provider: listing models for provider '%s'", provider_id);
    return strdup("[]");
}

/* PoP: cli_agent_image_gen_provider_get_setup_schema @ agent/image_gen_provider.py:get_setup_schema */

/* Port of Python agent/image_gen_provider.py:get_setup_schema */
/* Return the setup schema for an image generation provider. Returns JSON object. */
char *cli_agent_image_gen_provider_get_setup_schema(const char *provider_id)
{
    if (!provider_id || !provider_id[0]) {
        return strdup("{"
            "\"type\":\"object\","
            "\"properties\":{"
                "\"api_key\":{\"type\":\"string\",\"description\":\"API key\"},"
                "\"model\":{\"type\":\"string\",\"description\":\"Model name\"}"
            "},"
            "\"required\":[\"api_key\"]"
        "}");
    }

    if (strcmp(provider_id, "fal") == 0) {
        return strdup("{"
            "\"type\":\"object\","
            "\"properties\":{"
                "\"api_key\":{\"type\":\"string\",\"description\":\"FAL API key\"},"
                "\"model\":{\"type\":\"string\",\"description\":\"Model ID\"},"
                "\"queue_run_origin\":{\"type\":\"string\",\"description\":\"Queue origin URL\"}"
            "},"
            "\"required\":[\"api_key\"]"
        "}");
    }

    return strdup("{"
        "\"type\":\"object\","
        "\"properties\":{"
            "\"api_key\":{\"type\":\"string\",\"description\":\"API key\"}"
        "},"
        "\"required\":[\"api_key\"]"
    "}");
}

/* PoP: cli_agent_image_gen_provider_default_model @ agent/image_gen_provider.py:default_model */

/* Port of Python agent/image_gen_provider.py:default_model */
/* Return the default model for an image generation provider. */
const char *cli_agent_image_gen_provider_default_model(const char *provider_id)
{
    if (!provider_id || !provider_id[0]) return "dall-e-3";

    if (strcmp(provider_id, "fal") == 0) return "flux-1.1-pro";
    if (strcmp(provider_id, "openai") == 0) return "dall-e-3";
    if (strcmp(provider_id, "stability") == 0) return "stable-diffusion-xl-1024-v1-0";

    return "dall-e-3";
}
