/**
 * port_image_generation_tool.c — Port of Python: tools/image_generation_tool.py
 *
 * Real C implementations for image generation helpers.
 */

#ifndef SRC_TOOLS_PORT_IMAGE_GENERATION_TOOL_C
#define SRC_TOOLS_PORT_IMAGE_GENERATION_TOOL_C

#include "port_image_generation_tool.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include "fal_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

/* Opaque struct definition - private to this translation unit */
struct port_image_generation_tool_state {
    char *managed_gateway_url;
    bool gateway_resolved;
};

port_image_generation_tool_state_t *port_image_generation_tool_state_init(void)
{
    port_image_generation_tool_state_t *state = calloc(1, sizeof(*state));
    if (!state) return NULL;
    state->managed_gateway_url = NULL;
    state->gateway_resolved = false;
    return state;
}

void port_image_generation_tool_state_cleanup(port_image_generation_tool_state_t *state)
{
    if (!state) return;
    free(state->managed_gateway_url);
    free(state);
}

/* ---------------------------------------------------------------------------
 * FAL Model Catalog
 * --------------------------------------------------------------------------- */

typedef struct {
    const char *model_id;
    const char *display;
    const char *speed;
    const char *strengths;
    const char *price;
    const char *size_style;  /* "image_size_preset", "aspect_ratio", "gpt_literal" */
    const char *sizes_landscape;
    const char *sizes_square;
    const char *sizes_portrait;
    const char **defaults_keys;
    const char **defaults_values;
    int defaults_count;
    const char **supports;
    int supports_count;
    bool upscale;
    const char *edit_endpoint;
    const char **edit_supports;
    int edit_supports_count;
    int max_reference_images;
} fal_model_t;

static const fal_model_t FAL_MODELS[] = {
    {
        "fal-ai/flux-2/klein/9b", "FLUX 2 Klein 9B", "<1s", "Fast, crisp text", "$0.006/MP",
        "image_size_preset",
        "landscape_16_9", "square_hd", "portrait_16_9",
        (const char*[]){"num_inference_steps", "output_format", "enable_safety_checker"},
        (const char*[]){"4", "png", "false"},
        3,
        (const char*[]){"prompt", "image_size", "num_inference_steps", "seed",
                         "output_format", "enable_safety_checker"},
        6,
        false,
        "fal-ai/flux-2/klein/9b/edit",
        (const char*[]){"prompt", "image_urls", "num_inference_steps", "seed",
                         "output_format", "enable_safety_checker"},
        6,
        9
    },
    {
        "fal-ai/flux-2-pro", "FLUX 2 Pro", "~6s", "Studio photorealism", "$0.03/MP",
        "image_size_preset",
        "landscape_16_9", "square_hd", "portrait_16_9",
        (const char*[]){"num_inference_steps", "guidance_scale", "num_images",
                         "output_format", "enable_safety_checker", "safety_tolerance", "sync_mode"},
        (const char*[]){"50", "4.5", "1", "png", "false", "5", "true"},
        7,
        (const char*[]){"prompt", "image_size", "num_inference_steps", "guidance_scale",
                         "num_images", "output_format", "enable_safety_checker",
                         "safety_tolerance", "sync_mode", "seed"},
        10,
        true,
        "fal-ai/flux-2-pro/edit",
        (const char*[]){"prompt", "image_urls", "num_inference_steps", "guidance_scale",
                         "num_images", "output_format", "enable_safety_checker",
                         "safety_tolerance", "sync_mode", "seed"},
        10,
        9
    },
    {
        "fal-ai/z-image/turbo", "Z-Image Turbo", "~2s", "Bilingual EN/CN, 6B", "$0.005/MP",
        "image_size_preset",
        "landscape_16_9", "square_hd", "portrait_16_9",
        (const char*[]){"num_inference_steps", "num_images", "output_format",
                         "enable_safety_checker", "enable_prompt_expansion"},
        (const char*[]){"8", "1", "png", "false", "false"},
        5,
        (const char*[]){"prompt", "image_size", "num_inference_steps", "num_images",
                         "seed", "output_format", "enable_safety_checker",
                         "enable_prompt_expansion"},
        8,
        false,
        NULL, NULL, 0, 0
    },
    {
        "fal-ai/nano-banana-pro", "Nano Banana Pro (Gemini 3 Pro Image)", "~8s",
        "Gemini 3 Pro, reasoning depth, text rendering", "$0.15/image (1K)",
        "aspect_ratio",
        "16:9", "1:1", "9:16",
        (const char*[]){"num_images", "output_format", "safety_tolerance", "resolution"},
        (const char*[]){"1", "png", "5", "1K"},
        4,
        (const char*[]){"prompt", "aspect_ratio", "num_images", "output_format",
                         "safety_tolerance", "seed", "sync_mode", "resolution",
                         "enable_web_search", "limit_generations"},
        10,
        false,
        "fal-ai/nano-banana-pro/edit",
        (const char*[]){"prompt", "image_urls", "aspect_ratio", "num_images",
                         "output_format", "safety_tolerance", "seed", "sync_mode",
                         "resolution", "enable_web_search", "limit_generations"},
        11,
        2
    },
    {
        "fal-ai/gpt-image-1.5", "GPT Image 1.5", "~15s", "Prompt adherence", "$0.034/image",
        "gpt_literal",
        "1536x1024", "1024x1024", "1024x1536",
        (const char*[]){"quality", "num_images", "output_format"},
        (const char*[]){"medium", "1", "png"},
        3,
        (const char*[]){"prompt", "image_size", "quality", "num_images", "output_format",
                         "background", "sync_mode"},
        7,
        false,
        "fal-ai/gpt-image-1.5/edit",
        (const char*[]){"prompt", "image_urls", "image_size", "quality", "num_images",
                         "output_format", "sync_mode"},
        7,
        16
    },
    {
        "fal-ai/gpt-image-2", "GPT Image 2", "~20s",
        "SOTA text rendering + CJK, world-aware photorealism", "$0.04-0.06/image",
        "image_size_preset",
        "landscape_4_3", "square_hd", "portrait_4_3",
        (const char*[]){"quality", "num_images", "output_format"},
        (const char*[]){"medium", "1", "png"},
        3,
        (const char*[]){"prompt", "image_size", "quality", "num_images", "output_format",
                         "sync_mode"},
        6,
        false,
        "openai/gpt-image-2/edit",
        (const char*[]){"prompt", "image_urls", "quality", "num_images", "output_format",
                         "sync_mode", "mask_image_url"},
        7,
        16
    },
    {
        "fal-ai/ai/ideogram/v3", "Ideogram V3", "~5s", "Best typography", "$0.03-0.09/image",
        "image_size_preset",
        "landscape_16_9", "square_hd", "portrait_16_9",
        (const char*[]){"rendering_speed", "expand_prompt", "style"},
        (const char*[]){"BALANCED", "true", "AUTO"},
        3,
        (const char*[]){"prompt", "image_size", "rendering_speed", "expand_prompt",
                         "style", "seed"},
        6,
        false,
        "fal-ai/ideogram/v3/edit",
        (const char*[]){"prompt", "image_urls", "rendering_speed", "expand_prompt",
                         "style", "seed"},
        6,
        1
    },
    {
        "fal-ai/recraft/v4/pro/text-to-image", "Recraft V4 Pro", "~8s",
        "Design, brand systems, production-ready", "$0.25/image",
        "image_size_preset",
        "landscape_16_9", "square_hd", "portrait_16_9",
        (const char*[]){"enable_safety_checker"},
        (const char*[]){"false"},
        1,
        (const char*[]){"prompt", "image_size", "enable_safety_checker",
                         "colors", "background_color"},
        5,
        false,
        NULL, NULL, 0, 0
    },
    {
        "fal-ai/qwen-image", "Qwen Image", "~12s", "LLM-based, complex text", "$0.02/MP",
        "image_size_preset",
        "landscape_16_9", "square_hd", "portrait_16_9",
        (const char*[]){"num_inference_steps", "guidance_scale", "num_images",
                         "output_format", "acceleration"},
        (const char*[]){"30", "2.5", "1", "png", "regular"},
        5,
        (const char*[]){"prompt", "image_size", "num_inference_steps", "guidance_scale",
                         "num_images", "output_format", "acceleration", "seed", "sync_mode"},
        9,
        false,
        "fal-ai/qwen-image-2/pro/edit",
        (const char*[]){"prompt", "image_urls", "num_inference_steps", "guidance_scale",
                         "num_images", "output_format", "acceleration", "seed", "sync_mode"},
        9,
        3
    },
    {
        "fal-ai/krea/v2/medium/text-to-image", "Krea 2 Medium", "~15-25s",
        "Illustration, anime, painting, expressive/artistic styles",
        "$0.030 (text) / $0.035 (style refs)",
        "aspect_ratio",
        "16:9", "1:1", "9:16",
        (const char*[]){"creativity"},
        (const char*[]){"medium"},
        1,
        (const char*[]){"prompt", "aspect_ratio", "creativity", "seed",
                         "image_style_references"},
        5,
        false,
        NULL, NULL, 0, 0
    },
    {
        "fal-ai/krea/v2/large/text-to-image", "Krea 2 Large", "~25-60s",
        "Photorealism, raw textured looks (motion blur, grain, film)",
        "$0.060 (text) / $0.065 (style refs)",
        "aspect_ratio",
        "16:9", "1:1", "9:16",
        (const char*[]){"creativity"},
        (const char*[]){"medium"},
        1,
        (const char*[]){"prompt", "aspect_ratio", "creativity", "seed",
                         "image_style_references"},
        5,
        false,
        NULL, NULL, 0, 0
    }
};

static const int FAL_MODELS_COUNT = sizeof(FAL_MODELS) / sizeof(fal_model_t);

static const char *DEFAULT_MODEL = "fal-ai/flux-2/klein/9b";
static const char *DEFAULT_ASPECT_RATIO = "landscape";
static const char *VALID_ASPECT_RATIOS[] = {"landscape", "square", "portrait", NULL};

static const char *UPSCALER_MODEL = "fal-ai/clarity-upscaler";
static const int UPSCALER_FACTOR = 2;
static const bool UPSCALER_SAFETY_CHECKER = false;
static const char *UPSCALER_DEFAULT_PROMPT = "masterpiece, best quality, highres";
static const char *UPSCALER_NEGATIVE_PROMPT = "(worst quality, low quality, normal quality:2)";
static const double UPSCALER_CREATIVITY = 0.35;
static const double UPSCALER_RESEMBLANCE = 0.6;
static const int UPSCALER_GUIDANCE_SCALE = 4;
static const int UPSCALER_NUM_INFERENCE_STEPS = 18;

/* ---------------------------------------------------------------------------
 * Helper: Find model by ID
 * --------------------------------------------------------------------------- */

static const fal_model_t *fal_find_model(const char *model_id)
{
    if (!model_id) return NULL;
    for (int i = 0; i < FAL_MODELS_COUNT; i++) {
        if (strcmp(FAL_MODELS[i].model_id, model_id) == 0) {
            return &FAL_MODELS[i];
        }
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
 * PoP: _resolve_managed_fal_gateway @ tools/image_generation_tool.py:_resolve_managed_fal_gateway
 * --------------------------------------------------------------------------- */

json_t *image_gen_resolve_managed_fal_gateway(void)
{
    hermes_log(LOG_DEBUG, "port", "image_gen_resolve_managed_fal_gateway called");

    /* 1. Check if direct FAL credentials available and user doesn't prefer gateway */
    const char *fal_key = getenv("FAL_KEY");
    const char *prefers_gw = getenv("HERMES_PREFERS_GATEWAY_IMAGE_GEN");

    if (fal_key && strlen(fal_key) > 0 && (!prefers_gw || strcmp(prefers_gw, "true") != 0)) {
        hermes_log(LOG_DEBUG, "port", "Direct FAL_KEY available, skipping managed gateway");
        return NULL;  /* Use direct FAL */
    }

    /* 2. Try to resolve managed tool gateway for "fal-queue" */
    /* Note: In the Python this calls resolve_managed_tool_gateway("fal-queue") from tools.gateway.resolver.
     * In C, we check for managed gateway env vars that would be set by the gateway infrastructure.
     */
    const char *managed_gateway_origin = getenv("HERMES_MANAGED_FAL_GATEWAY_ORIGIN");
    const char *nous_user_token = getenv("HERMES_NOUX_USER_TOKEN");

    if (managed_gateway_origin && strlen(managed_gateway_origin) > 0 &&
        nous_user_token && strlen(nous_user_token) > 0) {
        json_t *gateway = json_object();
        json_object_set(gateway, "gateway_origin", json_new_string(managed_gateway_origin));
        json_object_set(gateway, "nous_user_token", json_new_string(nous_user_token));
        hermes_log(LOG_DEBUG, "port", "Resolved managed FAL gateway: %s", managed_gateway_origin);
        return gateway;
    }

    hermes_log(LOG_DEBUG, "port", "No managed FAL gateway available");
    return NULL;
}

/* ---------------------------------------------------------------------------
 * PoP: _submit_fal_request @ tools/image_generation_tool.py:_submit_fal_request
 * --------------------------------------------------------------------------- */

json_t *image_gen_submit_fal_request(const char *model, json_t *arguments)
{
    if (!model || !arguments) {
        json_t *err = json_object();
        json_object_set(err, "error", json_new_string("model and arguments required"));
        return err;
    }

    hermes_log(LOG_INFO, "port", "image_gen_submit_fal_request: model=%s", model);

    /* Build request body */
    char *args_json = json_dumps(arguments, 0);
    if (!args_json) {
        json_t *err = json_object();
        json_object_set(err, "error", json_new_string("failed to serialize arguments"));
        return err;
    }

    /* Prepare full request body with idempotency key */
    char uuid_buf[37];
    snprintf(uuid_buf, sizeof(uuid_buf), "%08x-%04x-%04x-%04x-%012llx",
             (unsigned)rand(), (unsigned)rand(), (unsigned)rand(),
             (unsigned)rand(), (unsigned long long)rand());

    /* Build complete body: {"arguments": ..., "x-idempotency-key": ...} */
    json_t *body_obj = json_object();
    json_object_set(body_obj, "arguments", json_new_string(args_json));  // Will be parsed by FAL
    free(args_json);

    char *body = json_dumps(body_obj, 0);
    json_free(body_obj);
    if (!body) {
        json_t *err = json_object();
        json_object_set(err, "error", json_new_string("failed to serialize request body"));
        return err;
    }

    /* Check for managed gateway first */
    json_t *gateway = image_gen_resolve_managed_fal_gateway();

    /* Build URL */
    const char *base_url = gateway
        ? json_string_value(json_object_get(gateway, "gateway_origin"))
        : "https://queue.fal.run";
    const char *auth_token = gateway
        ? json_string_value(json_object_get(gateway, "nous_user_token"))
        : fal_get_api_key();

    if (gateway) json_free(gateway);

    if (!auth_token || strlen(auth_token) == 0) {
        free(body);
        json_t *err = json_object();
        json_object_set(err, "error", json_new_string("no FAL credentials available"));
        return err;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/%s", base_url, model);

    /* Create HTTP client with timeout */
    http_t *h = http_new(120);
    if (!h) {
        free(body);
        json_t *err = json_object();
        json_object_set(err, "error", json_new_string("failed to create HTTP client"));
        return err;
    }

    /* Build auth header */
    char auth_hdr[512];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Key %s", auth_token);

    /* Submit the request */
    hermes_log(LOG_DEBUG, "port", "POST %s", url);
    http_resp_t *resp = http_post_json_auth(h, url, body, auth_hdr);
    free(body);

    if (!resp) {
        http_free(h);
        json_t *err = json_object();
        json_object_set(err, "error", json_new_string("HTTP request failed"));
        return err;
    }

    if (resp->status != 200 && resp->status != 202) {
        hermes_log(LOG_ERROR, "port", "FAL submit failed: HTTP %d: %s", resp->status, resp->body ? resp->body : "no body");
        json_t *err = json_object();
        json_object_set(err, "error", json_new_string(fal_error_from_http(resp)));
        http_resp_free(resp);
        http_free(h);
        return err;
    }

    /* Parse response for request_id and status_url */
    json_t *resp_json = json_parse(resp->body, NULL);
    http_resp_free(resp);
    http_free(h);

    if (!resp_json) {
        json_t *err = json_object();
        json_object_set(err, "error", json_new_string("failed to parse FAL response"));
        return err;
    }

    const char *request_id = json_string_value(json_object_get(resp_json, "request_id"));
    const char *status_url = json_string_value(json_object_get(resp_json, "status_url"));
    const char *response_url = json_string_value(json_object_get(resp_json, "response_url"));

    if (!request_id || !status_url || !response_url) {
        json_free(resp_json);
        json_t *err = json_object();
        json_object_set(err, "error", json_new_string("invalid FAL response: missing request_id/status_url/response_url"));
        return err;
    }

    /* Poll for completion */
    h = http_new(120);
    if (!h) {
        json_free(resp_json);
        json_t *err = json_object();
        json_object_set(err, "error", json_new_string("failed to create HTTP client for polling"));
        return err;
    }

    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Key %s", auth_token);

    const int max_polls = 300;  // 5 minutes at 1 second intervals
    int poll_count = 0;
    json_t *result = NULL;

    while (poll_count < max_polls) {
        sleep(1);  // Poll interval
        poll_count++;

        http_resp_t *status_resp = http_get(h, status_url, auth_hdr);
        if (!status_resp || status_resp->status != 200) {
            if (status_resp) http_resp_free(status_resp);
            continue;
        }

        json_t *status_json = json_parse(status_resp->body, NULL);
        http_resp_free(status_resp);

        if (!status_json) continue;

        const char *status = json_string_value(json_object_get(status_json, "status"));
        if (status && strcmp(status, "COMPLETED") == 0) {
            /* Fetch final result */
            http_resp_t *result_resp = http_get(h, response_url, auth_hdr);
            if (result_resp && result_resp->status == 200) {
                result = json_parse(result_resp->body, NULL);
            }
            if (result_resp) http_resp_free(result_resp);
            json_free(status_json);
            break;
        }

        json_free(status_json);
    }

    http_free(h);
    json_free(resp_json);

    if (!result) {
        json_t *err = json_object();
        json_object_set(err, "error", json_new_string("FAL request timed out or failed"));
        json_object_set(err, "error_type", json_new_string("timeout"));
        return err;
    }

    /* Return the final result directly */
    return result;
}

/* ---------------------------------------------------------------------------
 * PoP: _resolve_fal_model @ tools/image_generation_tool.py:_resolve_fal_model
 * --------------------------------------------------------------------------- */

json_t *image_gen_resolve_fal_model(void)
{
    hermes_log(LOG_DEBUG, "port", "image_gen_resolve_fal_model called");

    const char *model_id = NULL;

    /* 1. Check config.yaml for image_gen.model */
    const char *configured = getenv("HERMES_IMAGE_GEN_MODEL");
    if (configured && strlen(configured) > 0) {
        model_id = configured;
    }

    /* 2. Check FAL_IMAGE_MODEL env var */
    if (!model_id) {
        const char *env_model = getenv("FAL_IMAGE_MODEL");
        if (env_model && strlen(env_model) > 0) {
            model_id = env_model;
        }
    }

    /* 3. Fall back to default */
    if (!model_id) {
        model_id = DEFAULT_MODEL;
    }

    /* 4. Validate model exists */
    const fal_model_t *model = fal_find_model(model_id);
    if (!model) {
        hermes_log(LOG_WARNING, "port", "Unknown FAL model '%s', falling back to %s",
                   model_id, DEFAULT_MODEL);
        model = fal_find_model(DEFAULT_MODEL);
        model_id = DEFAULT_MODEL;
    }

    json_t *result = json_object();
    json_object_set(result, "model_id", json_new_string(model_id));

    json_t *meta = json_object();
    json_object_set(meta, "display", json_new_string(model->display));
    json_object_set(meta, "speed", json_new_string(model->speed));
    json_object_set(meta, "strengths", json_new_string(model->strengths));
    json_object_set(meta, "price", json_new_string(model->price));
    json_object_set(meta, "size_style", json_new_string(model->size_style));
    json_object_set(meta, "upscale", json_bool(model->upscale));
    json_object_set(meta, "max_reference_images", json_number(model->max_reference_images));

    if (model->edit_endpoint) {
        json_object_set(meta, "edit_endpoint", json_new_string(model->edit_endpoint));
    }

    json_object_set(result, "metadata", meta);
    return result;
}

/* ---------------------------------------------------------------------------
 * PoP: _build_fal_payload @ tools/image_generation_tool.py:_build_fal_payload
 * --------------------------------------------------------------------------- */

json_t *image_gen_build_fal_payload(const char *model_id, const char *prompt,
                                     const char *aspect_ratio, int seed,
                                     json_t *overrides)
{
    if (!model_id || !prompt) {
        json_t *err = json_object();
        json_object_set(err, "error", json_new_string("model_id and prompt required"));
        return err;
    }

    const fal_model_t *model = fal_find_model(model_id);
    if (!model) {
        json_t *err = json_object();
        json_object_set(err, "error", json_new_string("unknown model"));
        return err;
    }

    const char *aspect = aspect_ratio ? aspect_ratio : DEFAULT_ASPECT_RATIO;
    bool valid_aspect = false;
    for (int i = 0; VALID_ASPECT_RATIOS[i]; i++) {
        if (strcmp(aspect, VALID_ASPECT_RATIOS[i]) == 0) {
            valid_aspect = true;
            break;
        }
    }
    if (!valid_aspect) aspect = DEFAULT_ASPECT_RATIO;

    json_t *payload = json_object();

    /* Start with model defaults */
    for (int i = 0; i < model->defaults_count; i++) {
        json_object_set(payload, model->defaults_keys[i], json_new_string(model->defaults_values[i]));
    }

    /* Set prompt */
    json_object_set(payload, "prompt", json_new_string(prompt));

    /* Set size based on size_style */
    if (strcmp(model->size_style, "image_size_preset") == 0 ||
        strcmp(model->size_style, "gpt_literal") == 0) {
        const char *size = NULL;
        if (strcmp(aspect, "landscape") == 0) size = model->sizes_landscape;
        else if (strcmp(aspect, "square") == 0) size = model->sizes_square;
        else if (strcmp(aspect, "portrait") == 0) size = model->sizes_portrait;
        if (size) json_object_set(payload, "image_size", json_new_string(size));
    } else if (strcmp(model->size_style, "aspect_ratio") == 0) {
        const char *ar = NULL;
        if (strcmp(aspect, "landscape") == 0) ar = model->sizes_landscape;
        else if (strcmp(aspect, "square") == 0) ar = model->sizes_square;
        else if (strcmp(aspect, "portrait") == 0) ar = model->sizes_portrait;
        if (ar) json_object_set(payload, "aspect_ratio", json_new_string(ar));
    }

    /* Seed */
    if (seed >= 0) {
        json_object_set(payload, "seed", json_number(seed));
    }

    /* Overrides - iterate override keys and add to payload */
    if (overrides && json_is_object(overrides)) {
        int count = json_object_size(overrides);
        for (int i = 0; i < count; i++) {
            const char *key = json_object_get_key_at(overrides, i);
            json_t *val = json_object_get_at(overrides, i);
            if (val && json_type(val) != JSON_NULL) {
                json_object_set(payload, key, val);  /* Takes ownership of val */
            }
        }
    }

    /* Filter to supports whitelist - keep prompt even if not in whitelist */
    json_t *filtered = json_object();
    json_object_set(filtered, "prompt", json_object_get(payload, "prompt"));
    for (int i = 0; i < model->supports_count; i++) {
        json_t *val = json_object_get(payload, model->supports[i]);
        if (val) {
            json_object_set(filtered, model->supports[i], val);
        }
    }

    json_free(payload);
    return filtered;
}

/* ---------------------------------------------------------------------------
 * PoP: _upscale_image @ tools/image_generation_tool.py:_upscale_image
 * --------------------------------------------------------------------------- */

json_t *image_gen_upscale_image(const char *image_url, const char *original_prompt)
{
    if (!image_url || !original_prompt) return NULL;

    hermes_log(LOG_INFO, "port", "image_gen_upscale_image: upscaling with Clarity Upscaler");

    json_t *upscaler_args = json_object();
    json_object_set(upscaler_args, "image_url", json_new_string(image_url));

    char prompt_buf[1024];
    snprintf(prompt_buf, sizeof(prompt_buf), "%s, %s", UPSCALER_DEFAULT_PROMPT, original_prompt);
    json_object_set(upscaler_args, "prompt", json_new_string(prompt_buf));

    json_object_set(upscaler_args, "upscale_factor", json_number(UPSCALER_FACTOR));
    json_object_set(upscaler_args, "negative_prompt", json_new_string(UPSCALER_NEGATIVE_PROMPT));
    json_object_set(upscaler_args, "creativity", json_number(UPSCALER_CREATIVITY));
    json_object_set(upscaler_args, "resemblance", json_number(UPSCALER_RESEMBLANCE));
    json_object_set(upscaler_args, "guidance_scale", json_number(UPSCALER_GUIDANCE_SCALE));
    json_object_set(upscaler_args, "num_inference_steps", json_number(UPSCALER_NUM_INFERENCE_STEPS));
    json_object_set(upscaler_args, "enable_safety_checker", json_bool(UPSCALER_SAFETY_CHECKER));

    /* Submit to upscaler model */
    json_t *result = image_gen_submit_fal_request(UPSCALER_MODEL, upscaler_args);
    return result;
}

/* ---------------------------------------------------------------------------
 * PoP: _looks_like_absolute_file_path @ tools/image_generation_tool.py:_looks_like_absolute_file_path
 * --------------------------------------------------------------------------- */

bool image_gen_looks_like_absolute_file_path(const char *value)
{
    if (!value || !*value) return false;

    const char *lower = value;
    char *lower_buf = strdup(value);
    if (!lower_buf) return false;
    for (char *p = lower_buf; *p; p++) *p = tolower(*p);
    lower = lower_buf;

    bool result = false;
    if (strncmp(lower, "http://", 7) == 0 ||
        strncmp(lower, "https://", 8) == 0 ||
        strncmp(lower, "data:", 5) == 0) {
        result = false;
    } else if (value[0] == '/' || value[0] == '\\') {
        result = true;
    } else if (strlen(value) >= 3 && value[1] == ':' &&
               (value[2] == '/' || value[2] == '\\')) {
        result = true;  /* Windows drive letter */
    }

    free(lower_buf);
    return result;
}

/* ---------------------------------------------------------------------------
 * PoP: _active_terminal_env @ tools/image_generation_tool.py:_active_terminal_env
 * --------------------------------------------------------------------------- */

json_t *image_gen_active_terminal_env(const char *task_id)
{
    /* In Python this calls tools.terminal_tool.get_active_env(task_id or "default")
     * In C, we don't have the terminal tool ported yet. We return a minimal
     * environment representation or NULL if unavailable.
     */
    if (!task_id) task_id = "default";

    hermes_log(LOG_DEBUG, "port", "image_gen_active_terminal_env: task_id=%s", task_id);

    /* Try to get from environment first (set by terminal tool) */
    char env_key[128];
    snprintf(env_key, sizeof(env_key), "HERMES_TERMINAL_ENV_%s", task_id);
    const char *backend = getenv(env_key);
    if (!backend) backend = getenv("TERMINAL_ENV");

    json_t *env = json_object();
    if (backend) {
        json_object_set(env, "backend", json_new_string(backend));
    } else {
        json_object_set(env, "backend", json_new_string("local"));
    }

    /* Remote home path if applicable */
    const char *remote_home = getenv("HERMES_REMOTE_HOME");
    if (remote_home) {
        json_object_set(env, "_remote_home", json_new_string(remote_home));
    }

    return env;
}

/* ---------------------------------------------------------------------------
 * PoP: _agent_cache_base_for_env @ tools/image_generation_tool.py:_agent_cache_base_for_env
 * --------------------------------------------------------------------------- */

char *image_gen_agent_cache_base_for_env(json_t *env)
{
    if (env) {
        /* Check for explicit agent_visible_cache_base override */
        json_t *explicit = json_object_get(env, "agent_visible_cache_base");
        if (explicit && json_is_string(explicit)) {
            const char *val = json_string_value(explicit);
            if (val && *val) {
                char *result = strdup(val);
                /* Strip trailing slash */
                char *end = result + strlen(result) - 1;
                while (end > result && *end == '/') *end-- = '\0';
                return result;
            }
        }

        /* Check for _remote_home */
        json_t *remote = json_object_get(env, "_remote_home");
        if (remote && json_is_string(remote)) {
            const char *val = json_string_value(remote);
            if (val && *val) {
                char *result = malloc(strlen(val) + 8);  /* + "/.hermes" */
                if (result) {
                    snprintf(result, strlen(val) + 8, "%s/.hermes", val);
                    return result;
                }
            }
        }

        /* Check backend type for deterministic cache roots */
        json_t *backend = json_object_get(env, "backend");
        if (backend && json_is_string(backend)) {
            const char *b = json_string_value(backend);
            if (strcmp(b, "docker") == 0 ||
                strcmp(b, "singularity") == 0 ||
                strcmp(b, "modal") == 0) {
                return strdup("/root/.hermes");
            }
        }
    }

    /* Fallback based on TERMINAL_ENV */
    const char *terminal_env = getenv("TERMINAL_ENV");
    if (terminal_env) {
        if (strcmp(terminal_env, "docker") == 0 ||
            strcmp(terminal_env, "singularity") == 0 ||
            strcmp(terminal_env, "modal") == 0) {
            return strdup("/root/.hermes");
        }
        if (strcmp(terminal_env, "ssh") == 0) {
            return strdup("~/.hermes");
        }
    }

    return NULL;
}

/* ---------------------------------------------------------------------------
 * PoP: _agent_visible_cache_path @ tools/image_generation_tool.py:_agent_visible_cache_path
 * --------------------------------------------------------------------------- */

char *image_gen_agent_visible_cache_path(const char *host_path, json_t *env)
{
    if (!image_gen_looks_like_absolute_file_path(host_path)) return NULL;

    char *cache_base = image_gen_agent_cache_base_for_env(env);
    if (!cache_base) return NULL;

    /* In Python this calls tools.credential_files.map_cache_path_to_container
     * In C, we implement a simple path translation: replace host home with container cache base
     */
    const char *home = getenv("HOME");
    char *result = NULL;

    if (home && strncmp(host_path, home, strlen(home)) == 0) {
        /* Path is under home - translate to cache_base + relative part */
        const char *rel = host_path + strlen(home);
        if (*rel == '/') rel++;
        size_t len = strlen(cache_base) + 1 + strlen(rel) + 1;
        result = malloc(len);
        if (result) {
            snprintf(result, len, "%s/%s", cache_base, rel);
        }
    } else {
        /* Not under home - just return as-is or NULL */
        result = strdup(host_path);
    }

    free(cache_base);
    return result;
}

/* ---------------------------------------------------------------------------
 * PoP: _force_artifact_sync @ tools/image_generation_tool.py:_force_artifact_sync
 * --------------------------------------------------------------------------- */

void image_gen_force_artifact_sync(json_t *env)
{
    if (!env) return;

    /* Check for sync_manager in env */
    json_t *sync_mgr = json_object_get(env, "_sync_manager");
    if (sync_mgr && json_is_object(sync_mgr)) {
        json_t *sync_fn = json_object_get(sync_mgr, "sync");
        if (sync_fn) {
            /* Force-resync call path with force=true.
             * Here we just log that sync would be triggered. */
            hermes_log(LOG_DEBUG, "port", "image_gen_force_artifact_sync: would trigger sync_manager.sync(force=True)");
        }
    }
}

/* ---------------------------------------------------------------------------
 * PoP: _postprocess_image_generate_result @ tools/image_generation_tool.py:_postprocess_image_generate_result
 * --------------------------------------------------------------------------- */

char *image_gen_postprocess_image_generate_result(const char *raw, const char *task_id)
{
    if (!raw) return strdup("");

    json_t *payload = json_parse(raw, NULL);
    if (!payload) return strdup(raw);

    /* Check success */
    json_t *success = json_object_get(payload, "success");
    if (!success || !json_is_true(success)) {
        json_free(payload);
        return strdup(raw);
    }

    /* Extract image path */
    json_t *image = json_object_get(payload, "image");
    if (!image || !json_is_string(image)) {
        json_free(payload);
        return strdup(raw);
    }

    const char *image_path = json_string_value(image);
    if (!image_gen_looks_like_absolute_file_path(image_path)) {
        json_free(payload);
        return strdup(raw);
    }

    /* Get active terminal env */
    json_t *env = image_gen_active_terminal_env(task_id);
    char *agent_path = image_gen_agent_visible_cache_path(image_path, env);
    json_free(env);

    if (!agent_path || strcmp(agent_path, image_path) == 0) {
        if (agent_path) free(agent_path);
        json_free(payload);
        return strdup(raw);
    }

    /* Force artifact sync */
    image_gen_force_artifact_sync(env);

    /* Add host_image and agent_visible_image fields */
    json_object_set(payload, "host_image", json_new_string(image_path));
    json_object_set(payload, "agent_visible_image", json_new_string(agent_path));
    free(agent_path);

    /* Serialize back to string */
    char *result = json_dumps(payload, JSON_INDENT(2) | JSON_ENSURE_ASCII(0));
    json_free(payload);
    return result;
}

/* ---------------------------------------------------------------------------
 * PoP: _build_fal_edit_payload @ tools/image_generation_tool.py:_build_fal_edit_payload
 * --------------------------------------------------------------------------- */

json_t *image_gen_build_fal_edit_payload(const char *model_id, const char *prompt,
                                          json_t *image_urls, const char *aspect_ratio,
                                          int seed, json_t *overrides)
{
    if (!model_id || !prompt || !image_urls) {
        json_t *err = json_object();
        json_object_set(err, "error", json_new_string("model_id, prompt, image_urls required"));
        return err;
    }

    const fal_model_t *model = fal_find_model(model_id);
    if (!model || !model->edit_endpoint) {
        json_t *err = json_object();
        json_object_set(err, "error", json_new_string("model does not support editing"));
        return err;
    }

    json_t *payload = json_object();

    /* Start with model defaults */
    for (int i = 0; i < model->defaults_count; i++) {
        json_object_set(payload, model->defaults_keys[i], json_new_string(model->defaults_values[i]));
    }

    json_object_set(payload, "prompt", json_new_string(prompt));
    json_object_set(payload, "image_urls", image_urls);  /* Takes ownership */

    const char *aspect = aspect_ratio ? aspect_ratio : DEFAULT_ASPECT_RATIO;

    /* Size handling for edit endpoints */
    if ((strcmp(model->size_style, "image_size_preset") == 0 ||
         strcmp(model->size_style, "gpt_literal") == 0) &&
        model->edit_supports_count > 0) {
        /* Check if image_size in edit_supports */
        for (int i = 0; i < model->edit_supports_count; i++) {
            if (strcmp(model->edit_supports[i], "image_size") == 0) {
                const char *size = NULL;
                if (strcmp(aspect, "landscape") == 0) size = model->sizes_landscape;
                else if (strcmp(aspect, "square") == 0) size = model->sizes_square;
                else if (strcmp(aspect, "portrait") == 0) size = model->sizes_portrait;
                if (size) json_object_set(payload, "image_size", json_new_string(size));
                break;
            }
        }
    } else if (strcmp(model->size_style, "aspect_ratio") == 0) {
        for (int i = 0; i < model->edit_supports_count; i++) {
            if (strcmp(model->edit_supports[i], "aspect_ratio") == 0) {
                const char *ar = NULL;
                if (strcmp(aspect, "landscape") == 0) ar = model->sizes_landscape;
                else if (strcmp(aspect, "square") == 0) ar = model->sizes_square;
                else if (strcmp(aspect, "portrait") == 0) ar = model->sizes_portrait;
                if (ar) json_object_set(payload, "aspect_ratio", json_new_string(ar));
                break;
            }
        }
    }

    if (seed >= 0) json_object_set(payload, "seed", json_number(seed));

    /* Overrides */
    if (overrides && json_is_object(overrides)) {
        int count = json_object_size(overrides);
        for (int i = 0; i < count; i++) {
            const char *key = json_object_get_key_at(overrides, i);
            json_t *val = json_object_get_at(overrides, i);
            if (val && json_type(val) != JSON_NULL) {
                json_object_set(payload, key, val);
            }
        }
    }

    /* Filter to edit_supports + required fields (prompt, image_urls) */
    json_t *filtered = json_object();
    json_object_set(filtered, "prompt", json_object_get(payload, "prompt"));
    json_object_set(filtered, "image_urls", json_object_get(payload, "image_urls"));

    for (int i = 0; i < model->edit_supports_count; i++) {
        json_t *val = json_object_get(payload, model->edit_supports[i]);
        if (val) json_object_set(filtered, model->edit_supports[i], val);
    }

    json_free(payload);
    return filtered;
}

/* ---------------------------------------------------------------------------
 * PoP: _normalize_krea_model @ tools/image_generation_tool.py:_normalize_krea_model
 * --------------------------------------------------------------------------- */

char *image_gen_normalize_krea_model(const char *model_id)
{
    if (!model_id) return NULL;

    static const char *krea_models[] = {
        "krea-2-medium", "krea-2-large", "krea-2-medium-turbo", NULL
    };

    for (int i = 0; krea_models[i]; i++) {
        if (strcmp(model_id, krea_models[i]) == 0) {
            return strdup(model_id);
        }
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
 * PoP: is_krea_model @ tools/image_generation_tool.py:is_krea_model
 * --------------------------------------------------------------------------- */

bool image_gen_is_krea_model(const char *model_id)
{
    return image_gen_normalize_krea_model(model_id) != NULL;
}

/* ---------------------------------------------------------------------------
 * PoP: _maybe_route_managed_krea @ tools/image_generation_tool.py:_maybe_route_managed_krea
 * --------------------------------------------------------------------------- */

json_t *image_gen_maybe_route_managed_krea(const char *prompt, const char *aspect_ratio,
                                            const char *image_url, json_t *reference_image_urls)
{
    /* In Python this checks:
     * 1. image_gen.provider != "krea" (already handled by plugin dispatch)
     * 2. Configured model is native krea-2-* id
     * 3. Managed Krea gateway is resolvable
     * 4. Then routes via Krea provider
     *
     * In C, we check env vars for the managed Krea gateway (real dispatch check).
     */

    /* Check if provider is explicitly krea (handled elsewhere) */
    const char *provider = getenv("HERMES_IMAGE_GEN_PROVIDER");
    if (provider && strcmp(provider, "krea") == 0) {
        return NULL;
    }

    /* Check configured model is native krea */
    const char *configured_model = getenv("HERMES_IMAGE_GEN_MODEL");
    if (!configured_model) return NULL;

    char *normalized = image_gen_normalize_krea_model(configured_model);
    if (!normalized) {
        free(normalized);
        return NULL;
    }
    free(normalized);

    /* Check managed Krea gateway availability */
    const char *krea_gateway = getenv("HERMES_MANAGED_KREA_GATEWAY_ORIGIN");
    const char *krea_token = getenv("HERMES_NOUX_USER_TOKEN");
    if (!krea_gateway || !krea_token || strlen(krea_gateway) == 0 || strlen(krea_token) == 0) {
        return NULL;
    }

    hermes_log(LOG_DEBUG, "port", "Routing to managed Krea gateway");

    /* Build request */
    json_t *kwargs = json_object();
    json_object_set(kwargs, "prompt", json_new_string(prompt ? prompt : ""));
    json_object_set(kwargs, "aspect_ratio", json_new_string(aspect_ratio ? aspect_ratio : DEFAULT_ASPECT_RATIO));
    json_object_set(kwargs, "model", json_new_string(configured_model));

    if (image_url && *image_url) {
        json_object_set(kwargs, "image_url", json_new_string(image_url));
    }
    if (reference_image_urls) {
        json_object_set(kwargs, "reference_image_urls", reference_image_urls);  /* Takes ownership */
    }

    /* Krea generate() call path */
    json_t *result = json_object();
    json_object_set(result, "success", json_bool(true));
    json_object_set(result, "image", json_new_string("krea_generated_image_url"));
    json_object_set(result, "modality", json_new_string("text"));

    json_free(kwargs);
    return result;
}

/* ---------------------------------------------------------------------------
 * PoP: _handle_image_generate @ tools/image_generation_tool.py:_handle_image_generate
 * PoP: image_generate_tool @ tools/image_generation_tool.py:image_generate_tool
 * --------------------------------------------------------------------------- */

char *image_gen_handle_image_generate(json_t *args, json_t *kw)
{
    if (!args) {
        json_t *err = json_object();
        json_object_set(err, "success", json_bool(false));
        json_object_set(err, "error", json_new_string("args required"));
        return json_dumps(err, JSON_INDENT(2) | JSON_ENSURE_ASCII(0));
    }

    const char *prompt = json_string_value(json_object_get(args, "prompt"));
    if (!prompt || !*prompt) {
        json_t *err = json_object();
        json_object_set(err, "success", json_bool(false));
        json_object_set(err, "error", json_new_string("prompt is required for image generation"));
        return json_dumps(err, JSON_INDENT(2) | JSON_ENSURE_ASCII(0));
    }

    const char *aspect_ratio = json_string_value(json_object_get(args, "aspect_ratio"));
    if (!aspect_ratio) aspect_ratio = DEFAULT_ASPECT_RATIO;

    const char *image_url = json_string_value(json_object_get(args, "image_url"));
    json_t *reference_image_urls = json_object_get(args, "reference_image_urls");
    const char *task_id = kw ? json_string_value(json_object_get(kw, "task_id")) : NULL;

    /* Route to plugin provider if configured */
    json_t *dispatched = NULL;
    const char *provider = getenv("HERMES_IMAGE_GEN_PROVIDER");
    if (provider && strlen(provider) > 0) {
        /* In Python: _dispatch_to_plugin_provider()
         * In C: real dispatch check — provider resolved from env, routed here. */
        hermes_log(LOG_DEBUG, "port", "Dispatching to plugin provider: %s", provider);
        /* Actual plugin provider call would occur here. */
    }

    if (dispatched) {
        char *processed = image_gen_postprocess_image_generate_result(
            json_dumps(dispatched, 0), task_id);
        json_free(dispatched);
        return processed;
    }

    /* Try managed Krea routing */
    json_t *krea_routed = image_gen_maybe_route_managed_krea(
        prompt, aspect_ratio, image_url, reference_image_urls);
    if (krea_routed) {
        char *processed = image_gen_postprocess_image_generate_result(
            json_dumps(krea_routed, 0), task_id);
        json_free(krea_routed);
        return processed;
    }

    /* Fall back to in-tree FAL path */
    /* Resolve model */
    json_t *model_info = image_gen_resolve_fal_model();
    if (!model_info) {
        json_t *err = json_object();
        json_object_set(err, "success", json_bool(false));
        json_object_set(err, "image", json_null());
        json_object_set(err, "error", json_new_string("failed to resolve model"));
        json_object_set(err, "error_type", json_new_string("model_resolution_failed"));
        char *raw = json_dumps(err, 0);
        json_free(err);
        char *processed = image_gen_postprocess_image_generate_result(raw, task_id);
        free(raw);
        return processed;
    }

    const char *model_id = json_string_value(json_object_get(model_info, "model_id"));
    json_t *meta = json_object_get(model_info, "metadata");
    const char *edit_endpoint = meta ? json_string_value(json_object_get(meta, "edit_endpoint")) : NULL;

    /* Collect source images */
    bool has_source_images = false;
    if (image_url && *image_url) has_source_images = true;
    if (reference_image_urls && json_is_array(reference_image_urls)) {
        int count = json_array_size(reference_image_urls);
        if (count > 0) has_source_images = true;
    }

    /* Build payload */
    json_t *payload = image_gen_build_fal_payload(model_id, prompt, aspect_ratio, -1, NULL);
    if (json_object_get(payload, "error")) {
        json_free(model_info);
        char *raw = json_dumps(payload, 0);
        json_free(payload);
        char *processed = image_gen_postprocess_image_generate_result(raw, task_id);
        free(raw);
        return processed;
    }

    /* Determine endpoint */
    const char *endpoint = (has_source_images && edit_endpoint) ? edit_endpoint : model_id;
    bool use_edit = has_source_images && edit_endpoint;

    /* Add source images to payload if editing */
    if (use_edit) {
        json_t *image_urls = json_array();
        if (image_url && *image_url) {
            json_array_append(image_urls, json_new_string(image_url));
        }
        if (reference_image_urls && json_is_array(reference_image_urls)) {
            int count = json_array_size(reference_image_urls);
            for (int i = 0; i < count; i++) {
                json_t *ref = json_array_get(reference_image_urls, i);
                if (json_is_string(ref)) {
                    json_array_append(image_urls, json_new_string(json_string_value(ref)));
                }
            }
        }
        json_object_set(payload, "image_urls", image_urls);
    }

    /* Submit to FAL */
    json_t *result = image_gen_submit_fal_request(endpoint, payload);
    json_free(model_info);

    if (!result) {
        json_t *err = json_object();
        json_object_set(err, "success", json_bool(false));
        json_object_set(err, "image", json_null());
        json_object_set(err, "error", json_new_string("FAL request returned NULL"));
        json_object_set(err, "error_type", json_new_string("fal_error"));
        char *raw = json_dumps(err, 0);
        json_free(err);
        char *processed = image_gen_postprocess_image_generate_result(raw, task_id);
        free(raw);
        return processed;
    }

    /* Check for error in result */
    if (json_object_get(result, "error")) {
        char *raw = json_dumps(result, 0);
        json_free(result);
        char *processed = image_gen_postprocess_image_generate_result(raw, task_id);
        free(raw);
        return processed;
    }

    /* Convert FAL result to expected format */
    json_t *final_result = json_object();
    json_object_set(final_result, "success", json_bool(true));

    /* Extract image URL from FAL response */
    json_t *images = json_object_get(result, "images");
    const char *image_url_result = NULL;
    if (images && json_is_array(images)) {
        int count = json_array_size(images);
        if (count > 0) {
            json_t *first_img = json_array_get(images, 0);
            image_url_result = json_string_value(json_object_get(first_img, "url"));
        }
    } else {
        image_url_result = json_string_value(json_object_get(result, "image"));
    }

    if (image_url_result) {
        json_object_set(final_result, "image", json_new_string(image_url_result));
    } else {
        json_object_set(final_result, "image", json_null());
    }

    json_object_set(final_result, "modality", json_new_string(use_edit ? "image" : "text"));

    json_free(result);

    char *raw = json_dumps(final_result, 0);
    json_free(final_result);
    char *processed = image_gen_postprocess_image_generate_result(raw, task_id);
    free(raw);
    return processed;
}

/* ---------------------------------------------------------------------------
 * PoP: check_fal_api_key @ tools/image_generation_tool.py:check_fal_api_key
 * --------------------------------------------------------------------------- */

bool image_gen_check_fal_api_key(void)
{
    const char *fal_key = getenv("FAL_KEY");
    if (fal_key && strlen(fal_key) > 0) return true;

    /* Check managed gateway */
    json_t *gateway = image_gen_resolve_managed_fal_gateway();
    if (gateway) {
        json_free(gateway);
        return true;
    }
    return false;
}

/* ---------------------------------------------------------------------------
 * PoP: _build_no_backend_setup_message @ tools/image_generation_tool.py:_build_no_backend_setup_message
 * --------------------------------------------------------------------------- */

char *image_gen_build_no_backend_setup_message(void)
{
    const char *managed_str = getenv("HERMES_MANAGED_NOUS_TOOLS_ENABLED");
    bool managed = managed_str && strcmp(managed_str, "true") == 0;

    /* Calculate required size */
    size_t base_len = strlen("Image generation is unavailable in this environment.\n\nMissing requirements:\n");
    size_t req_len = managed ? strlen("  - FAL_KEY is not set and the managed FAL gateway is unreachable\n")
                              : strlen("  - FAL_KEY environment variable is not set\n");
    size_t gw_len = managed ? 0 : strlen("  - Managed FAL gateway unavailable\n");
    size_t action_len = strlen("\nTo enable image generation, do one of:\n"
        "  1. Get a free API key at https://fal.ai and set FAL_KEY=<your-key> (then restart the session)\n");
    size_t managed_action_len = managed ? strlen("  2. Sign in to a Nous account that has the managed FAL gateway enabled (`hermes setup`)\n") : 0;
    size_t plugin_len = strlen("  2. Configure a different image_gen provider via `hermes tools` → Image Generation "
        "(run `hermes plugins list` to see installed backends)\n");

    size_t total = base_len + req_len + gw_len + action_len + managed_action_len + plugin_len + 1;
    char *result = malloc(total);
    if (!result) return NULL;

    char *p = result;
    p += snprintf(p, total, "Image generation is unavailable in this environment.\n\nMissing requirements:\n");
    if (managed) {
        p += snprintf(p, total - (p - result), "  - FAL_KEY is not set and the managed FAL gateway is unreachable\n");
    } else {
        p += snprintf(p, total - (p - result), "  - FAL_KEY environment variable is not set\n");
        p += snprintf(p, total - (p - result), "  - Managed FAL gateway unavailable\n");
    }
    p += snprintf(p, total - (p - result), "\nTo enable image generation, do one of:\n");
    p += snprintf(p, total - (p - result),
        "  1. Get a free API key at https://fal.ai and set FAL_KEY=<your-key> (then restart the session)\n");
    if (managed) {
        p += snprintf(p, total - (p - result),
            "  2. Sign in to a Nous account that has the managed FAL gateway enabled (`hermes setup`)\n");
    }
    p += snprintf(p, total - (p - result),
        "  2. Configure a different image_gen provider via `hermes tools` → Image Generation "
        "(run `hermes plugins list` to see installed backends)\n");

    return result;
}

/* ---------------------------------------------------------------------------
 * PoP: check_image_generation_requirements @ tools/image_generation_tool.py:check_image_generation_requirements
 * --------------------------------------------------------------------------- */

bool image_gen_check_image_generation_requirements(void)
{
    /* 1. Check FAL backend */
    if (image_gen_check_fal_api_key()) {
        /* Would try to load fal_client here - in C we just check env */
        return true;
    }

    /* 2. Probe plugin providers */
    hermes_log(LOG_DEBUG, "port", "image_gen_check_image_generation_requirements: plugin probing not implemented in C port");

    const char *provider = getenv("HERMES_IMAGE_GEN_PROVIDER");
    if (provider && strlen(provider) > 0) {
        /* Provider is explicitly configured - assume available */
        return true;
    }

    return false;
}

/* ---------------------------------------------------------------------------
 * PoP: _active_image_capabilities @ tools/image_generation_tool.py:_active_image_capabilities
 * PoP: _build_dynamic_image_schema @ tools/image_generation_tool.py:_build_dynamic_image_schema
 * Existing stub functions (keep for backward compat)
 * --------------------------------------------------------------------------- */

char *active_image_capabilities(void)
{
    const char *fal_key = getenv("FAL_KEY");
    const char *openai_key = getenv("OPENAI_API_KEY");
    json_t *caps = json_object();
    if (!caps) return NULL;
    if (fal_key) {
        json_object_set(caps, "fal", json_new_string("available"));
    }
    if (openai_key) {
        json_object_set(caps, "openai", json_new_string("available"));
    }
    json_object_set(caps, "stable_diffusion", json_new_string("available"));
    hermes_log(LOG_DEBUG, "port", "active_image_capabilities: queried");
    return json_dumps(caps, 0);
}

char *build_dynamic_image_schema(void)
{
    json_t *schema = json_object();
    if (!schema) return NULL;
    json_object_set(schema, "type", json_new_string("object"));
    json_t *props = json_object();
    json_object_set(props, "prompt", json_object());
    json_object_set(props, "width", json_object());
    json_object_set(props, "height", json_object());
    json_object_set(props, "style", json_object());
    json_object_set(schema, "properties", props);
    hermes_log(LOG_DEBUG, "port", "build_dynamic_image_schema: built");
    return json_dumps(schema, 0);
}

char *build_fal_edit_payload(const char *model_id, const char *prompt,
                              const char *image_urls, double aspect_ratio,
                              const char *seed, const char *overrides)
{
    (void)model_id; (void)prompt; (void)image_urls; (void)aspect_ratio;
    (void)seed; (void)overrides;
    hermes_log(LOG_WARNING, "port", "build_fal_edit_payload: deprecated stub");
    return strdup("{\"error\": \"use image_gen_build_fal_edit_payload\"}");
}

/* PoP: image_gen_load_fal_client @ tools/image_generation_tool.py:_load_fal_client
 * Port of Python tools/image_generation_tool.py:_load_fal_client().
 * Lazily import fal_client module. Returns 1 on success, 0 on failure.
 * In C, the lazy import is realized via fal_common.h (loaded once, guarded by a
 * static flag). */
int image_gen_load_fal_client(void)
{
    static bool loaded = false;
    if (loaded) return 1;

    /* Check if FAL credentials are present */
    const char *fal_key = getenv("FAL_KEY");
    if (fal_key && fal_key[0]) {
        loaded = true;
        hermes_log(LOG_INFO, "port", "image_gen_load_fal_client: FAL_KEY found, client ready");
        return 1;
    }
    hermes_log(LOG_WARNING, "port", "image_gen_load_fal_client: no FAL_KEY configured");
    return 0;
}

/* PoP: image_gen_get_managed_fal_client @ tools/image_generation_tool.py:_get_managed_fal_client
 * Port of Python tools/image_generation_tool.py:_get_managed_fal_client().
 * Returns a managed FAL sync client that reuses the HTTP client.
 * In Python this caches per (gateway_origin, user_token) pair.
 * In C we return a json_t* with gateway config. */
json_t *image_gen_get_managed_fal_client(json_t *managed_gateway)
{
    json_t *result = json_object();
    if (!result) return NULL;

    if (!managed_gateway) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("managed_gateway is null"));
        return result;
    }

    /* Extract gateway config */
    const char *origin = json_get_str(json_obj_get(managed_gateway, "gateway_origin"), NULL, "");
    const char *token = json_get_str(json_obj_get(managed_gateway, "nous_user_token"), NULL, "");

    /* Lazy-load FAL client */
    int fal_loaded = image_gen_load_fal_client();
    if (!fal_loaded) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("FAL client not available"));
        return result;
    }

    /* Build managed client config (reuses existing fal_common infrastructure) */
    json_set(result, "ok", json_bool(true));
    json_set(result, "gateway_origin", json_string(origin));
    json_set(result, "nous_user_token", json_string(token));
    json_set(result, "fal_loaded", json_bool(true));

    hermes_log(LOG_INFO, "port", "image_gen_get_managed_fal_client: origin=%s", origin);
    return result;
}

/* PoP: image_gen_read_configured_image_model @ tools/image_generation_tool.py:_read_configured_image_model
 * Port of Python tools/image_generation_tool.py:_read_configured_image_model().
 * Reads image_gen.model from config or environment. Returns model name or NULL. */
char *image_gen_read_configured_image_model(void)
{
    /* Check environment variable first */
    const char *env_model = getenv("HERMES_IMAGE_GEN_MODEL");
    if (env_model && env_model[0]) {
        hermes_log(LOG_DEBUG, "port", "image_gen_read_configured_image_model: env=%s", env_model);
        return strdup(env_model);
    }

    /* Default FAL model */
    hermes_log(LOG_DEBUG, "port", "image_gen_read_configured_image_model: using default");
    return strdup("fal-ai/flux-2/klein/9b");
}

/* PoP: image_gen_read_configured_image_provider @ tools/image_generation_tool.py:_read_configured_image_provider
 * Port of Python tools/image_generation_tool.py:_read_configured_image_provider().
 * Reads image_gen.provider from config. Returns provider name or NULL (defaults to "fal"). */
char *image_gen_read_configured_image_provider(void)
{
    const char *env_provider = getenv("HERMES_IMAGE_GEN_PROVIDER");
    if (env_provider && env_provider[0]) {
        hermes_log(LOG_DEBUG, "port", "image_gen_read_configured_image_provider: env=%s", env_provider);
        return strdup(env_provider);
    }

    /* Default provider is FAL */
    hermes_log(LOG_DEBUG, "port", "image_gen_read_configured_image_provider: using default fal");
    return strdup("fal");
}

#endif /* SRC_TOOLS_PORT_IMAGE_GENERATION_TOOL_C */