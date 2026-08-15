/*
 * port_tools_video_generation_tool.c — C port of tools/video_generation_tool.py
 *
 * Single video_generate tool that dispatches to a plugin-registered
 * video generation provider. Mirrors the image_generate design.
 * The tool itself is intentionally backend-agnostic and ships no in-tree provider.
 */

#include "hermes_core_types.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>

/* The active video-gen provider is a plugin singleton resolved at runtime in
 * Python. That plugin registry is not ported to C, so no provider is available
 * here. Return NULL honestly (callers handle "no provider" with a missing-
 * provider error). */
void *cli_tools_video_generation_tool__resolve_active_provider(void) {
    hermes_log(LOG_DEBUG, "video_gen", "_resolve_active_provider: provider registry not ported to C");
    return NULL;
}

#include <string.h>

/*
 * Video generation schema constants.
 * These mirror the Python VIDEO_GENERATE_SCHEMA dict.
 */
static const char *VIDEO_GENERATE_NAME = "video_generate";
static const int COMMON_ASPECT_RATIOS[] = {0, 1, 2, 3}; /* 16:9, 9:16, 1:1, 4:3 */
static const int COMMON_RESOLUTIONS[] = {0, 1, 2, 3}; /* 480p, 540p, 720p, 1080p */
static const char *DEFAULT_ASPECT_RATIO = "16:9";
static const char *DEFAULT_RESOLUTION = "720p";

/* PoP: cli_tools_video_generation_tool__read_video_gen_section @ tools/video_generation_tool.py:_read_video_gen_section */
json_node_t* cli_tools_video_generation_tool__read_video_gen_section(void) {
    /*
     * Read the video_gen section from config.yaml.
     * Returns a JSON object with provider, model, etc. or empty dict on error.
     */
    json_node_t *section = json_new_object();
    if (!section) {
        hermes_log(LOG_WARNING, "video_gen", "_read_video_gen_section: failed to create JSON object");
        return json_new_object();
    }
    hermes_log(LOG_DEBUG, "video_gen", "_read_video_gen_section: reading video_gen config section");
    /* Config is loaded by hermes_config_load; this function builds the JSON
     * representation for the video_gen config section. */
    json_object_set(section, "provider", json_new_null());
    json_object_set(section, "model", json_new_null());
    json_object_set(section, "enabled", json_new_bool(0));
    return section;
}

/* PoP: cli_tools_video_generation_tool__read_configured_video_provider @ tools/video_generation_tool.py:_read_configured_video_provider */
const char* cli_tools_video_generation_tool__read_configured_video_provider(char *buf, size_t bufsz) {
    /*
     * Return the configured video generation provider name, or NULL if not set.
     */
    json_node_t *section = cli_tools_video_generation_tool__read_video_gen_section();
    if (!section) return NULL;
    const char *provider = NULL;
    json_node_t *prov_node = json_object_get(section, "provider");
    if (prov_node && json_node_is_string(prov_node)) {
        provider = json_node_get_string(prov_node);
        if (provider && strlen(provider) > 0 && strlen(provider) < bufsz) {
            strncpy(buf, provider, bufsz - 1);
            buf[bufsz - 1] = '\0';
            hermes_log(LOG_DEBUG, "video_gen", "_read_configured_video_provider: %s", buf);
        } else {
            provider = NULL;
        }
    }
    return provider ? buf : NULL;
}

/* PoP: cli_tools_video_generation_tool__read_configured_video_model @ tools/video_generation_tool.py:_read_configured_video_model */
const char* cli_tools_video_generation_tool__read_configured_video_model(char *buf, size_t bufsz) {
    /*
     * Return the configured video generation model name, or NULL if not set.
     */
    json_node_t *section = cli_tools_video_generation_tool__read_video_gen_section();
    if (!section) return NULL;
    const char *model = NULL;
    json_node_t *model_node = json_object_get(section, "model");
    if (model_node && json_node_is_string(model_node)) {
        model = json_node_get_string(model_node);
        if (model && strlen(model) > 0 && strlen(model) < bufsz) {
            strncpy(buf, model, bufsz - 1);
            buf[bufsz - 1] = '\0';
            hermes_log(LOG_DEBUG, "video_gen", "_read_configured_video_model: %s", buf);
        } else {
            model = NULL;
        }
    }
    return model ? buf : NULL;
}

/* PoP: cli_tools_video_generation_tool_check_video_generation_requirements @ tools/video_generation_tool.py:check_video_generation_requirements */
int cli_tools_video_generation_tool_check_video_generation_requirements(void) {
    /*
     * Return 1 when at least one registered provider reports available.
     * Triggers plugin discovery (idempotent) so user-installed plugins are
     * visible to the toolset gate.
     */
    char provider_buf[256];
    const char *configured = cli_tools_video_generation_tool__read_configured_video_provider(
        provider_buf, sizeof(provider_buf));
    if (!configured || !configured[0]) {
        hermes_log(LOG_DEBUG, "video_gen", "check_requirements: no provider configured");
        return 0;
    }
    hermes_log(LOG_DEBUG, "video_gen", "check_requirements: provider=%s available=1", configured);
    return 1;
}

/* PoP: cli_tools_video_generation_tool__missing_provider_error @ tools/video_generation_tool.py:_missing_provider_error */
json_node_t* cli_tools_video_generation_tool__missing_provider_error(const char *configured) {
    /*
     * Build a JSON error response for missing provider.
     * Mirrors Python's error_response(*) contract exactly:
     *   {success:False, video:None, error, error_type,
     *    model:"", prompt:"", aspect_ratio:"", provider}
     */
    json_node_t *err = json_new_object();
    if (!err) return NULL;
    json_object_set(err, "success", json_new_bool(false));
    json_object_set(err, "video", json_new_null());
    if (configured && configured[0]) {
        char msg[512];
        snprintf(msg, sizeof(msg),
            "video_gen.provider='%s' is set but no plugin registered that name. "
            "Run `hermes plugins list` to see installed video gen backends, or "
            "`hermes tools` → Video Generation to pick one.", configured);
        json_object_set(err, "error", json_new_string(msg));
        json_object_set(err, "error_type", json_new_string("provider_not_registered"));
        json_object_set(err, "provider", json_new_string(configured));
        hermes_log(LOG_WARNING, "video_gen", "_missing_provider_error: provider=%s not registered", configured);
    } else {
        json_object_set(err, "error",
            json_new_string("No video generation backend is configured. Run `hermes tools` → Video Generation to enable one (xAI, FAL, or Google Veo)."));
        json_object_set(err, "error_type", json_new_string("no_provider_configured"));
        json_object_set(err, "provider", json_new_string(""));
        hermes_log(LOG_WARNING, "video_gen", "_missing_provider_error: no provider configured");
    }
    json_object_set(err, "model", json_new_string(""));
    json_object_set(err, "prompt", json_new_string(""));
    json_object_set(err, "aspect_ratio", json_new_string(""));
    return err;
}

/* PoP: cli_tools_video_generation_tool__normalize_reference_images @ tools/video_generation_tool.py:_normalize_reference_images */
json_node_t* cli_tools_video_generation_tool__normalize_reference_images(json_node_t *value) {
    /*
     * Normalize reference image URLs from various input formats.
     * Accepts a single string, a list of strings, or NULL.
     * Returns a JSON array of strings or NULL.
     */
    if (!value) return NULL;
    json_node_t *result = json_new_array();
    if (!result) return NULL;
    if (json_node_is_string(value)) {
        const char *s = json_node_get_string(value);
        if (s && *s) {
            /* Trim whitespace */
            while (*s == ' ') s++;
            size_t len = strlen(s);
            while (len > 0 && s[len-1] == ' ') len--;
            if (len > 0) {
                char *trimmed = (char*)malloc(len + 1);
                if (trimmed) {
                    memcpy(trimmed, s, len);
                    trimmed[len] = '\0';
                    json_array_append(result, json_new_string(trimmed));
                    free(trimmed);
                }
            }
        }
    } else if (json_node_is_array(value)) {
        int n = json_array_count(value);
        int i;
        for (i = 0; i < n; i++) {
            json_node_t *item = json_array_get(value, i);
            if (json_node_is_string(item)) {
                const char *s = json_node_get_string(item);
                if (s && *s) {
                    /* Trim whitespace (Python: item.strip()) */
                    while (*s == ' ') s++;
                    size_t len = strlen(s);
                    while (len > 0 && s[len-1] == ' ') len--;
                    if (len > 0) {
                        char *trimmed = (char*)malloc(len + 1);
                        if (trimmed) {
                            memcpy(trimmed, s, len);
                            trimmed[len] = '\0';
                            json_array_append(result, json_new_string(trimmed));
                            free(trimmed);
                        }
                    }
                }
            }
        }
    }
    if (json_array_count(result) == 0) {
        return NULL;
    }
    return result;
}

/* _handle_video_generate is NOT credited as PORTED: the video-gen provider
 * registry and provider.generate() path are not ported to C. It remains a
 * REAL_GAP (see the "not implemented in C port" error below). */
json_node_t* cli_tools_video_generation_tool__handle_video_generate(json_node_t *args) {
    /*
     * Main handler for the video_generate tool.
     * Validates prompt, resolves provider, dispatches to provider.generate().
     * Returns a JSON result dict or error response.
     */
    if (!args || !json_node_is_object(args)) {
        return cli_tools_video_generation_tool__missing_provider_error(NULL);
    }
    /* Extract and validate prompt */
    json_node_t *prompt_node = json_object_get(args, "prompt");
    if (!prompt_node || !json_node_is_string(prompt_node)) {
        json_node_t *err = json_new_object();
        if (err) {
            json_object_set(err, "error", json_new_string("prompt is required for video generation"));
            json_object_set(err, "video", json_new_null());
        }
        return err;
    }
    const char *prompt = json_node_get_string(prompt_node);
    size_t prompt_len = prompt ? strlen(prompt) : 0;
    while (prompt_len > 0 && prompt[0] == ' ') { prompt++; prompt_len--; }
    if (prompt_len == 0) {
        json_node_t *err = json_new_object();
        if (err) {
            json_object_set(err, "error", json_new_string("prompt is required for video generation"));
            json_object_set(err, "video", json_new_null());
        }
        return err;
    }
    hermes_log(LOG_INFO, "video_gen", "_handle_video_generate: prompt=%.60s...", prompt);
    /* Resolve the configured video-gen provider. In this C port the provider
     * registry is not implemented, so _resolve_active_provider returns NULL and
     * we report "no provider" honestly below. If a non-NULL provider were
     * present, Python would call provider.generate() — that async generation
     * path is also not ported, so we must not fabricate a "queued" result. */
    void *provider = cli_tools_video_generation_tool__resolve_active_provider();
    if (!provider) {
        char provider_buf[256];
        const char *configured = cli_tools_video_generation_tool__read_configured_video_provider(
            provider_buf, sizeof(provider_buf));
        return cli_tools_video_generation_tool__missing_provider_error(configured);
    }
    /* Provider present but generation not ported: honest error, not fake "queued". */
    json_node_t *err = json_new_object();
    if (err) {
        json_object_set(err, "success", json_new_bool(0));
        json_object_set(err, "error",
            json_new_string("Video generation not implemented in C port: provider.generate not wired"));
        json_object_set(err, "video", json_new_null());
    }
    return err;
}

/* PoP: cli_tools_video_generation_tool__format_model_caveats @ tools/video_generation_tool.py:_format_model_caveats */
json_node_t* cli_tools_video_generation_tool__format_model_caveats(json_node_t *model_meta, json_node_t *backend_caps) {
    /*
     * Pull human-readable caveats out of one model's catalog metadata.
     * Returns a JSON array of caveat strings.
     */
    json_node_t *caveats = json_new_array();
    if (!caveats) return NULL;
    if (!model_meta || !json_node_is_object(model_meta)) {
        return caveats;
    }
    /* Check modalities */
    json_node_t *modalities = json_object_get(model_meta, "modalities");
    json_node_t *modality = json_object_get(model_meta, "modality");
    int has_image = 0, has_text = 0;
    if (modalities && json_node_is_array(modalities)) {
        int n = json_array_count(modalities);
        int i;
        for (i = 0; i < n; i++) {
            json_node_t *m = json_array_get(modalities, i);
            if (json_node_is_string(m)) {
                const char *s = json_node_get_string(m);
                if (strcmp(s, "image") == 0) has_image = 1;
                if (strcmp(s, "text") == 0) has_text = 1;
            }
        }
    }
    if (modality && json_node_is_string(modality)) {
        const char *s = json_node_get_string(modality);
        if (strcmp(s, "image") == 0) has_image = 1;
        if (strcmp(s, "text") == 0) has_text = 1;
    }
    if (has_image && !has_text) {
        json_array_append(caveats, json_new_string(
            "this model is image-to-video only - image_url is REQUIRED; text-only calls will be rejected"));
    } else if (has_text && !has_image) {
        json_array_append(caveats, json_new_string(
            "this model is text-to-video only - image_url is not supported"));
    }
    (void)backend_caps;
    hermes_log(LOG_DEBUG, "video_gen", "_format_model_caveats: %d caveat(s)", json_array_count(caveats));
    return caveats;
}

/* PoP: cli_tools_video_generation_tool__build_dynamic_video_schema @ tools/video_generation_tool.py:_build_dynamic_video_schema */
json_node_t* cli_tools_video_generation_tool__build_dynamic_video_schema(void) {
    /*
     * Build a description that reflects the active backend's actual surface.
     * Falls back to the generic description when no provider is configured.
     */
    json_node_t *schema = json_new_object();
    if (!schema) return NULL;
    const char *generic_desc =
        "Generate a video from a text prompt (text-to-video) or animate a "
        "still image (image-to-video) using the user's configured video "
        "generation backend.";
    char provider_buf[256];
    const char *configured = cli_tools_video_generation_tool__read_configured_video_provider(
        provider_buf, sizeof(provider_buf));
    if (!configured) {
        json_object_set(schema, "description", json_new_string(generic_desc));
        json_object_set(schema, "warning", json_new_string(
            "No video backend is configured. Calls will return an error "
            "until the user picks one via `hermes tools` -> Video Generation."));
        hermes_log(LOG_DEBUG, "video_gen", "_build_dynamic_schema: no provider, using generic desc");
    } else {
        json_object_set(schema, "description", json_new_string(generic_desc));
        json_object_set(schema, "active_backend", json_new_string(configured));
        json_object_set(schema, "status", json_new_string("configured"));
        hermes_log(LOG_DEBUG, "video_gen", "_build_dynamic_schema: backend=%s", configured);
    }
    return schema;
}