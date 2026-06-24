/*
 * video_gen.c — Video generation tool for Hermes C.
 * Uses FAL.ai REST API for text-to-video, image-to-video, and video editing.
 * Reads FAL_API_KEY from config/env via libfalcommon (shared with image_gen).
 *
 * Mirrors Python tools/video_generation_tool.py with a single unified
 * tool that dispatches to FAL.ai video endpoints.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "fal_common.h"
#include "xai_http.h"
#include "video_gen_registry.h"
#include "base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  FAL.ai Video API Constants
 * ================================================================ */

/* Primary video generation endpoint — text-to-video / image-to-video */
#define FAL_VIDEO_BASE   "https://fal.run/fal-ai/veo3"

/* Video extension (continue from existing video) */
#define FAL_VIDEO_EXTEND "https://fal.run/fal-ai/video-extend"

/* ================================================================
 *  Forward declarations
 * ================================================================ */

static char *xai_video_generate(const char *prompt, const char *aspect_ratio,
                                 int duration, const char *image_url,
                                 json_t *ref_images);

/* ================================================================
 *  Video Generation Handler
 * ================================================================ */

char *video_generate_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args)
        return strdup("{\"success\":false,\"error\":\"Invalid JSON arguments\"}");

    /* Extract params */
    const char *prompt = json_get_str(args, "prompt", "");
    const char *operation = json_get_str(args, "operation", "generate");
    const char *aspect_ratio = json_get_str(args, "aspect_ratio", "16:9");
    const char *resolution = json_get_str(args, "resolution", "720p");
    int duration = (int)json_get_num(args, "duration", 5);
    const char *image_url = json_get_str(args, "image_url", NULL);
    const char *video_url = json_get_str(args, "video_url", NULL);
    const char *negative_prompt = json_get_str(args, "negative_prompt", NULL);
    int seed = (int)json_get_num(args, "seed", 0);
    bool has_audio = json_get_bool(args, "audio", false);
    const char *style = json_get_str(args, "style", NULL);
    double motion_scale = json_get_num(args, "motion_scale", 0.5);
    bool loop = json_get_bool(args, "loop", false);
    const char *provider = json_get_str(args, "provider", "fal");

    /* Route to xAI provider */
    if (provider && strcmp(provider, "xai") == 0) {
        json_t *ref_images = json_obj_get(args, "reference_image_urls");
        char *result = xai_video_generate(prompt, aspect_ratio, duration,
                                           image_url, ref_images);
        json_free(args);
        return result;
    }

    /* Get FAL API key from shared helper (FAL provider) */
    if (!fal_get_api_key()) {
        json_free(args);
        return fal_error_response("%s", "FAL_API_KEY not set. Get a key at https://fal.ai");
    }

    /* Prompt is required for generate */
    if (strcmp(operation, "generate") == 0 && (!prompt || !*prompt)) {
        json_free(args);
        return fal_error_response("Missing 'prompt' for generate operation");
    }

    /* Determine API endpoint */
    const char *api_url = FAL_VIDEO_BASE;
    if (strcmp(operation, "extend") == 0 || strcmp(operation, "edit") == 0) {
        api_url = FAL_VIDEO_EXTEND;
    }

    /* Build request body — start with required fields */
    char body[16384];
    size_t pos = 0;
    size_t rem = sizeof(body);
    int n;

    n = snprintf(body + pos, rem, "{\"prompt\":\"");
    if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }

    /* Use shared JSON escape helper */
    {
        char esc_prompt[4096];
        fal_escape_json(prompt, esc_prompt, sizeof(esc_prompt));
        n = snprintf(body + pos, rem, "%s", esc_prompt);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    n = snprintf(body + pos, rem, "\",\"aspect_ratio\":\"%s\"", aspect_ratio);
    if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }

    /* Optional fields */
    if (duration > 0) {
        n = snprintf(body + pos, rem, ",\"duration\":%d", duration);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    if (image_url && *image_url) {
        n = snprintf(body + pos, rem, ",\"image_url\":\"%s\"", image_url);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    if (video_url && *video_url) {
        n = snprintf(body + pos, rem, ",\"video_url\":\"%s\"", video_url);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    if (negative_prompt && *negative_prompt) {
        n = snprintf(body + pos, rem, ",\"negative_prompt\":\"%s\"", negative_prompt);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    if (seed > 0) {
        n = snprintf(body + pos, rem, ",\"seed\":%d", seed);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    if (style && *style) {
        char esc_style[256];
        fal_escape_json(style, esc_style, sizeof(esc_style));
        n = snprintf(body + pos, rem, ",\"style\":\"%s\"", esc_style);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    if (motion_scale != 0.5) {
        n = snprintf(body + pos, rem, ",\"motion_scale\":%.2f", motion_scale);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    if (loop) {
        n = snprintf(body + pos, rem, ",\"loop\":true");
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    const char *model = json_get_str(args, "model", NULL);
    if (model && *model) {
        n = snprintf(body + pos, rem, ",\"model\":\"%s\"", model);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    /* Serialize reference_image_urls array */
    json_t *ref_imgs = json_obj_get(args, "reference_image_urls");
    if (ref_imgs && json_len(ref_imgs) > 0) {
        char *ref_serialized = json_serialize(ref_imgs);
        if (ref_serialized) {
            n = snprintf(body + pos, rem, ",\"reference_image_urls\":%s", ref_serialized);
            if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
            free(ref_serialized);
        }
    }

    if (strcmp(resolution, "720p") != 0) {
        n = snprintf(body + pos, rem, ",\"resolution\":\"%s\"", resolution);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    if (has_audio) {
        n = snprintf(body + pos, rem, ",\"audio\":true");
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    /* Close JSON */
    n = snprintf(body + pos, rem, "}");
    if (n >= 0 && (size_t)n < rem) { pos += n; }

    /* Use shared FAL POST helper with 120s timeout (video gen is slow) */
    http_resp_t *resp = fal_post_json(api_url, body, 120);

    if (!resp) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"Failed to connect to FAL API\"}");
    }

    if (resp->status != 200) {
        char *err = fal_error_from_http(resp);
        http_resp_free(resp);
        json_free(args);
        return err;
    }

    /* Parse response */
    char *parse_err = NULL;
    json_t *result = json_parse(resp->body, &parse_err);
    http_resp_free(resp);

    if (!result) {
        char err[1024];
        snprintf(err, sizeof(err),
            "{\"success\":false,\"error\":\"Failed to parse FAL response: %s\"}",
            parse_err ? parse_err : "parse error");
        free(parse_err);
        json_free(args);
        return strdup(err);
    }

    /* Extract video URL from response */
    json_t *videos = json_obj_get(result, "video");
    const char *video_url_out = NULL;
    if (videos) {
        video_url_out = json_get_str(videos, "url", NULL);
    }
    if (!video_url_out) {
        videos = json_obj_get(result, "videos");
        if (videos && json_len(videos) > 0) {
            json_t *first = json_get(videos, 0);
            if (first) video_url_out = json_get_str(first, "url", NULL);
        }
    }

    if (!video_url_out) {
        /* Check for direct url field */
        video_url_out = json_get_str(result, "url", NULL);
    }

    if (!video_url_out) {
        /* Fallback: return the entire response for inspection */
        char *serialized = json_serialize(result);
        char err[2048];
        snprintf(err, sizeof(err),
            "{\"success\":false,\"error\":\"No video URL in response\",\"response\":%s}",
            serialized ? serialized : "null");
        free(serialized);
        json_free(result);
        json_free(args);
        return strdup(err);
    }

    /* Build successful response */
    char out[8192];
    snprintf(out, sizeof(out),
        "{\"success\":true,\"video\":\"%s\",\"operation\":\"%s\"}",
        video_url_out, operation);

    json_free(result);
    json_free(args);
    return strdup(out);
}

/* ================================================================
 *  xAI Video Generation
 * ================================================================ */

#define XAI_VIDEO_GENERATIONS "https://api.x.ai/v1/videos/generations"
#define XAI_VIDEO_MODEL       "grok-imagine-video"
#define XAI_MAX_REFERENCES    7
#define XAI_MAX_DURATION      15
#define XAI_MAX_DURATION_REFS 10

/* Valid xAI aspect ratios */
static const char *xai_map_aspect_ratio(const char *aspect_ratio) {
    if (strcmp(aspect_ratio, "9:16") == 0) return "9:16";
    if (strcmp(aspect_ratio, "1:1") == 0) return "1:1";
    return "16:9"; /* default */
}

static bool xai_video_available(void) {
    return has_xai_credentials();
}

static char *xai_video_generate(const char *prompt, const char *aspect_ratio,
                                 int duration, const char *image_url,
                                 json_t *ref_images) {
    char api_key[XAI_API_KEY_MAX];
    if (!xai_get_api_key(api_key)) {
        return strdup("{\"success\":false,\"error\":\"XAI_API_KEY not set\",\"error_type\":\"auth_required\"}");
    }

    if (!prompt || !*prompt) {
        return strdup("{\"success\":false,\"error\":\"Missing 'prompt' parameter\",\"error_type\":\"missing_prompt\"}");
    }

    /* Validate: image_url and reference_images are mutually exclusive */
    if (image_url && *image_url && ref_images && json_len(ref_images) > 0) {
        return strdup("{\"success\":false,\"error\":\"Cannot specify both image_url and reference_image_urls\",\"error_type\":\"conflicting_inputs\"}");
    }

    /* Validate: max 7 reference images */
    if (ref_images && json_len(ref_images) > XAI_MAX_REFERENCES) {
        char err[256];
        snprintf(err, sizeof(err),
            "{\"success\":false,\"error\":\"Too many reference images (max %d)\",\"error_type\":\"too_many_references\"}",
            XAI_MAX_REFERENCES);
        return strdup(err);
    }

    /* Clamp duration */
    int dur = duration > 0 ? duration : 5;
    int max_dur = (ref_images && json_len(ref_images) > 0) ? XAI_MAX_DURATION_REFS : XAI_MAX_DURATION;
    if (dur > max_dur) dur = max_dur;

    const char *ar = xai_map_aspect_ratio(aspect_ratio);

    /* Build request body */
    char body[16384];
    size_t pos = 0;
    size_t rem = sizeof(body);
    int n;

    n = snprintf(body + pos, rem, "{\"prompt\":\"");
    if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }

    {
        char esc_prompt[4096];
        fal_escape_json(prompt, esc_prompt, sizeof(esc_prompt));
        n = snprintf(body + pos, rem, "%s", esc_prompt);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    n = snprintf(body + pos, rem, "\",\"model\":\"%s\",\"aspect_ratio\":\"%s\",\"duration\":%d",
                 XAI_VIDEO_MODEL, ar, dur);
    if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }

    /* Add image for image-to-video */
    if (image_url && *image_url) {
        n = snprintf(body + pos, rem, ",\"image\":{\"url\":\"%s\"}", image_url);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    /* Add reference_images */
    if (ref_images && json_len(ref_images) > 0) {
        n = snprintf(body + pos, rem, ",\"reference_images\":[");
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
        int len = json_len(ref_images);
        for (int i = 0; i < len; i++) {
            json_t *item = json_get(ref_images, i);
            const char *url = item ? json_get_str(item, "url", NULL) : NULL;
            if (!url) url = "";
            n = snprintf(body + pos, rem, "%s{\"url\":\"%s\"}", i > 0 ? "," : "", url);
            if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
        }
        n = snprintf(body + pos, rem, "]");
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    n = snprintf(body + pos, rem, "}");
    if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }

    /* Build auth header */
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", api_key);

    /* POST to xAI video generations endpoint */
    http_t *h = http_new(120);
    http_resp_t *resp = http_post_json_auth(h, XAI_VIDEO_GENERATIONS, body, auth_header);
    http_free(h);
    if (!resp) {
        return strdup("{\"success\":false,\"error\":\"Failed to connect to xAI API\"}");
    }

    if (resp->status != 200) {
        char err[2048];
        snprintf(err, sizeof(err),
            "{\"success\":false,\"error\":\"xAI API HTTP %d: %s\",\"error_type\":\"api_error\"}",
            resp->status, resp->body ? resp->body : "no response");
        http_resp_free(resp);
        return strdup(err);
    }

    /* Parse response — xAI returns {request_id, status, video: {url, duration}} */
    char *parse_err = NULL;
    json_t *result = json_parse(resp->body, &parse_err);
    http_resp_free(resp);

    if (!result) {
        char err[1024];
        snprintf(err, sizeof(err),
            "{\"success\":false,\"error\":\"Failed to parse xAI response: %s\"}",
            parse_err ? parse_err : "parse error");
        free(parse_err);
        return strdup(err);
    }

    /* Check for video URL in response */
    const char *video_url = NULL;
    json_t *video_obj = json_obj_get(result, "video");
    if (video_obj) {
        video_url = json_get_str(video_obj, "url", NULL);
    }
    if (!video_url) {
        video_url = json_get_str(result, "url", NULL);
    }

    /* Check status — may be async */
    const char *status = json_get_str(result, "status", NULL);
    const char *request_id = json_get_str(result, "request_id", NULL);

    char *out;
    if (video_url) {
        out = malloc(8192);
        snprintf(out, 8192,
            "{\"success\":true,\"video\":\"%s\",\"model\":\"%s\",\"provider\":\"xai\"}",
            video_url, XAI_VIDEO_MODEL);
    } else if (request_id || (status && strcmp(status, "done") != 0)) {
        /* Async: return request_id for polling */
        out = malloc(4096);
        snprintf(out, 4096,
            "{\"success\":true,\"status\":\"%s\",\"request_id\":\"%s\",\"model\":\"%s\",\"provider\":\"xai\",\"note\":\"Video generation in progress — poll for result\"}",
            status ? status : "processing",
            request_id ? request_id : "unknown",
            XAI_VIDEO_MODEL);
    } else {
        char *serialized = json_serialize(result);
        out = malloc(4096);
        snprintf(out, 4096,
            "{\"success\":false,\"error\":\"No video URL in xAI response\",\"response\":%s}",
            serialized ? serialized : "null");
        free(serialized);
    }

    json_free(result);
    return out;
}

/* ================================================================
 *  Video save helpers (port of Python video_gen_provider.py)
 * ================================================================ */

/** Generate a cache/videos/ filename with timestamp + random suffix — Port of Python _videos_cache_dir() */
static char *video_gen_make_cache_path(const char *hermes_home,
                                        const char *prefix,
                                        const char *extension) {
    char dir[4096];
    int n = snprintf(dir, sizeof(dir), "%s/cache/videos", hermes_home);
    if (n < 0 || (size_t)n >= sizeof(dir)) return NULL;

    /* mkdir -p */
    char mkcmd[4160];
    snprintf(mkcmd, sizeof(mkcmd), "mkdir -p '%s'", dir);
    (void)system(mkcmd);

    /* Generate timestamp */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", tm);

    /* Random 8 hex chars */
    unsigned int r = (unsigned int)(now ^ (unsigned long)(void*)&now);
    srand(r + clock());
    char rand_hex[9];
    snprintf(rand_hex, sizeof(rand_hex), "%08x", (unsigned int)rand());

    /* Full path */
    char *path = (char *)malloc(4096);
    if (!path) return NULL;
    snprintf(path, 4096, "%s/%s_%s_%s.%s",
             dir, prefix ? prefix : "video", ts, rand_hex,
             extension ? extension : "mp4");
    return path;
}

/* Decode base64 video data and save to cache/videos/.
 * Port of Python video_gen_provider.py:save_b64_video(). */
char *video_gen_save_b64_video(const char *b64_data, const char *prefix,
                                const char *extension) {
    const char *home = getenv("HERMES_HOME");
    char fallback[1024];
    if (!home || !*home) {
        const char *h = getenv("HOME");
        if (!h) return NULL;
        snprintf(fallback, sizeof(fallback), "%s/.hermes", h);
        home = fallback;
    }

    char *path = video_gen_make_cache_path(home, prefix, extension);
    if (!path) return NULL;

    size_t out_len = 0;
    unsigned char *decoded = base64_decode(b64_data, &out_len);
    if (!decoded || out_len == 0) {
        free(path);
        free(decoded);
        return NULL;
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(decoded);
        free(path);
        return NULL;
    }
    fwrite(decoded, 1, out_len, f);
    fclose(f);
    free(decoded);
    return path;
}

/* Save raw video bytes to cache/videos/.
 * Port of Python video_gen_provider.py:save_bytes_video(). */
char *video_gen_save_bytes_video(const unsigned char *data, size_t data_len,
                                  const char *prefix, const char *extension) {
    if (!data || data_len == 0) return NULL;

    const char *home = getenv("HERMES_HOME");
    char fallback[1024];
    if (!home || !*home) {
        const char *h = getenv("HOME");
        if (!h) return NULL;
        snprintf(fallback, sizeof(fallback), "%s/.hermes", h);
        home = fallback;
    }

    char *path = video_gen_make_cache_path(home, prefix, extension);
    if (!path) return NULL;

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(path);
        return NULL;
    }
    fwrite(data, 1, data_len, f);
    fclose(f);
    return path;
}

/* Build a uniform success response JSON string. Returns malloc'd string (caller free).
 * Port of Python video_gen_provider.py:success_response(). */
char *video_gen_success_response(const char *video, const char *model,
                                  const char *prompt, const char *modality,
                                  const char *aspect_ratio, int duration,
                                  const char *provider, const char *extra_json) {
    json_t *resp = json_object();
    json_set(resp, "success", json_bool(true));
    json_set(resp, "video", json_string(video ? video : ""));
    json_set(resp, "model", json_string(model ? model : ""));
    json_set(resp, "prompt", json_string(prompt ? prompt : ""));
    json_set(resp, "modality", json_string(modality ? modality : "text"));
    json_set(resp, "aspect_ratio", json_string(aspect_ratio ? aspect_ratio : ""));
    json_set(resp, "duration", json_number(duration > 0 ? duration : 0));
    json_set(resp, "provider", json_string(provider ? provider : ""));
    if (extra_json) {
        json_t *extra = json_parse(extra_json, NULL);
        if (extra && extra->type == JSON_OBJECT) {
            for (size_t i = 0; i < extra->c.count; i++) {
                if (!json_has(resp, extra->c.keys[i]))
                    json_set(resp, extra->c.keys[i], json_copy(extra->c.items[i]));
            }
        }
        json_free(extra);
    }
    char *out = json_serialize(resp);
    json_free(resp);
    return out;
}

/* Build a uniform error response JSON string. Returns malloc'd string (caller free).
 * Port of Python video_gen_provider.py:error_response(). */
char *video_gen_error_response(const char *error, const char *error_type,
                                const char *provider, const char *model,
                                const char *prompt, const char *aspect_ratio) {
    json_t *resp = json_object();
    json_set(resp, "success", json_bool(false));
    json_set(resp, "video", json_null());
    json_set(resp, "error", json_string(error ? error : "Unknown error"));
    json_set(resp, "error_type", json_string(error_type ? error_type : "provider_error"));
    json_set(resp, "model", json_string(model ? model : ""));
    json_set(resp, "prompt", json_string(prompt ? prompt : ""));
    json_set(resp, "aspect_ratio", json_string(aspect_ratio ? aspect_ratio : ""));
    json_set(resp, "provider", json_string(provider ? provider : ""));
    char *out = json_serialize(resp);
    json_free(resp);
    return out;
}

/* ================================================================
 *  Registration
 * ================================================================ */

/* FAL provider availability check */
static bool fal_video_is_available(void) {
    return fal_get_api_key() != NULL;
}

void registry_init_video_gen(void) {
    /* Register FAL provider in the video_gen registry */
    video_gen_provider_t fal_provider;
    memset(&fal_provider, 0, sizeof(fal_provider));
    snprintf(fal_provider.name, sizeof(fal_provider.name), "%s", "fal");
    snprintf(fal_provider.display_name, sizeof(fal_provider.display_name), "%s", "FAL.ai (Veo3)");
    fal_provider.is_available = fal_video_is_available;
    fal_provider.generate = NULL; /* Uses video_generate_handler directly */
    video_gen_register_provider(&fal_provider);

    /* Register xAI provider */
    video_gen_provider_t xai_provider;
    memset(&xai_provider, 0, sizeof(xai_provider));
    snprintf(xai_provider.name, sizeof(xai_provider.name), "%s", "xai");
    snprintf(xai_provider.display_name, sizeof(xai_provider.display_name), "%s", "xAI (Grok Imagine)");
    xai_provider.is_available = xai_video_available;
    xai_provider.generate = NULL; /* Uses video_generate_handler with provider routing */
    video_gen_register_provider(&xai_provider);

    registry_register("video_generate",
        "Generate video from text or images. Supports FAL.ai Veo3 and xAI Grok Imagine. "
        "Supports text-to-video, image-to-video (with image_url), and reference images.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
        "  \"prompt\":{\"type\":\"string\",\"description\":\"Text description of the video to generate\"},"
        "  \"provider\":{\"type\":\"string\",\"description\":\"Video generation provider: fal (FAL.ai Veo3), xai (xAI Grok Imagine)\",\"default\":\"fal\"},"
        "  \"operation\":{\"type\":\"string\",\"description\":\"Operation type: generate, edit, extend\",\"enum\":[\"generate\",\"edit\",\"extend\"],\"default\":\"generate\"},"
        "  \"aspect_ratio\":{\"type\":\"string\",\"description\":\"Aspect ratio (16:9, 9:16, 1:1)\",\"default\":\"16:9\"},"
        "  \"duration\":{\"type\":\"integer\",\"description\":\"Target duration in seconds (max 15, or 10 with reference images)\",\"default\":5},"
        "  \"image_url\":{\"type\":\"string\",\"description\":\"Source image URL for image-to-video\"},"
        "  \"video_url\":{\"type\":\"string\",\"description\":\"Source video URL for edit/extend operations (FAL only)\"},"
        "  \"resolution\":{\"type\":\"string\",\"description\":\"Output resolution (720p, 1080p) (FAL only)\",\"default\":\"720p\"},"
        "  \"negative_prompt\":{\"type\":\"string\",\"description\":\"What to avoid in the video (FAL only)\"},"
        "  \"audio\":{\"type\":\"boolean\",\"description\":\"Generate with audio track (FAL only)\",\"default\":false},"
        "  \"seed\":{\"type\":\"integer\",\"description\":\"Random seed for reproducibility (FAL only)\"},"
        "  \"style\":{\"type\":\"string\",\"description\":\"Video style (FAL only)\"},"
        "  \"motion_scale\":{\"type\":\"number\",\"description\":\"Motion intensity scale 0.0-1.0 (FAL only)\",\"default\":0.5},"
        "  \"loop\":{\"type\":\"boolean\",\"description\":\"Loop the video seamlessly (FAL only)\",\"default\":false},"
        "  \"model\":{\"type\":\"string\",\"description\":\"Model override (FAL: fal-ai/veo3, xai: grok-imagine-video)\"},"
        "  \"reference_image_urls\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"description\":\"Optional reference image URLs for style/consistency (max 7, xAI only)\"}"
        "},"
        "\"required\":[\"prompt\"]"
        "}",
        video_generate_handler);
}
