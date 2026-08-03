/*
 * image_gen.c — Image generation tool for Hermes C.
 * Uses FAL.ai REST API: POST to fal-ai/flux-pro with prompt.
 * Reads FAL_API_KEY from config/env via libfalcommon.
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "fal_common.h"
#include "hermes_tool_config.h"
#include "base64.h"
#include "image_gen_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* ================================================================
 *  OpenAI DALL-E Image Generation
 * ================================================================ */

#define OPENAI_IMAGE_URL "https://api.openai.com/v1/images/generations"

static const char *openai_map_aspect_ratio(const char *aspect_ratio, const char *model) {
    if (strcmp(model, "dall-e-2") == 0) {
        if (strcmp(aspect_ratio, "landscape") == 0) return "1792x1024";
        if (strcmp(aspect_ratio, "portrait") == 0) return "1024x1792";
        return "1024x1024";
    }
    if (strcmp(aspect_ratio, "landscape") == 0) return "1792x1024";
    if (strcmp(aspect_ratio, "portrait") == 0) return "1024x1792";
    return "1024x1024";
}

static char *openai_image_generate(const char *prompt, const char *aspect_ratio,
                                    const char *model_override) {
    const char *model = (model_override && model_override[0]) ? model_override : "dall-e-3";
    const char *size = openai_map_aspect_ratio(aspect_ratio, model);
    const char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key || !*api_key) {
        return strdup("{\"success\":false,\"error\":\"OPENAI_API_KEY not set\"}");
    }
    char esc_prompt[4096];
    fal_escape_json(prompt, esc_prompt, sizeof(esc_prompt));
    char body[8192];
    int pos = snprintf(body, sizeof(body),
        "{\"model\":\"%s\",\"prompt\":\"%s\",\"n\":1,\"size\":\"%s\"",
        model, esc_prompt, size);
    if (strcmp(model, "dall-e-3") == 0 || strcmp(model, "gpt-image-1") == 0) {
        pos += snprintf(body + pos, sizeof(body) - pos, ",\"quality\":\"standard\"");
        if (strcmp(model, "dall-e-3") == 0) {
            pos += snprintf(body + pos, sizeof(body) - pos, ",\"style\":\"vivid\"");
        }
    }
    snprintf(body + pos, sizeof(body) - pos, "}");
    char auth[1024];
    snprintf(auth, sizeof(auth), "Bearer %s", api_key);
    /* OPENAI_IMAGE_URL env override (mirrors OPENAI_BASE_URL in the LLM
     * client) — lets tests/self-hosted point image-gen at any endpoint. */
    const char *endpoint = getenv("OPENAI_IMAGE_URL");
    if (!endpoint || !*endpoint) endpoint = OPENAI_IMAGE_URL;
    http_t *h = http_new(120);
    http_resp_t *resp = http_post_json_auth(h, endpoint, body, auth);
    if (!resp) { http_free(h); return strdup("{\"success\":false,\"error\":\"Failed to connect to OpenAI API\"}"); }
    if (resp->status != 200) {
        char err[2048];
        snprintf(err, sizeof(err), "{\"success\":false,\"error\":\"OpenAI API HTTP %d: %.200s\"}",
            resp->status, resp->body ? resp->body : "no body");
        http_resp_free(resp); http_free(h);
        return strdup(err);
    }
    char *parse_err = NULL;
    json_t *result = json_parse(resp->body, &parse_err);
    http_resp_free(resp); http_free(h);
    if (!result) {
        char err[1024];
        snprintf(err, sizeof(err), "{\"success\":false,\"error\":\"JSON parse: %s\"}", parse_err ? parse_err : "?");
        free(parse_err); return strdup(err);
    }
    json_t *data = json_obj_get(result, "data");
    if (!data || json_len(data) == 0) { json_free(result); return strdup("{\"success\":false,\"error\":\"No image data\"}"); }
    json_t *first = json_get(data, 0);
    const char *b64 = first ? json_get_str(first, "b64_json", NULL) : NULL;
    const char *url = first ? json_get_str(first, "url", NULL) : NULL;
    const char *revised = first ? json_get_str(first, "revised_prompt", NULL) : NULL;
    char *image_ref = NULL;
    if (b64) {
        char filename[256];
        snprintf(filename, sizeof(filename), "/tmp/slermes_img_openai_%ld.png", (long)time(NULL));
        size_t out_len = 0;
        unsigned char *decoded = base64_decode(b64, &out_len);
        if (decoded && out_len > 0) {
            FILE *f = fopen(filename, "wb");
            if (f) { fwrite(decoded, 1, out_len, f); fclose(f); image_ref = strdup(filename); }
            else image_ref = strdup("local_save_failed");
            free(decoded);
        } else { image_ref = strdup("b64_decode_failed"); }
    } else if (url) { image_ref = strdup(url); }
    if (!image_ref) { json_free(result); return strdup("{\"success\":false,\"error\":\"No image in response\"}"); }
    char *out = (char *)malloc(8192);
    if (revised && revised[0]) {
        snprintf(out, 8192, "{\"success\":true,\"image\":\"%s\",\"model\":\"%s\",\"revised_prompt\":\"%s\"}",
            image_ref, model, revised);
    } else {
        snprintf(out, 8192, "{\"success\":true,\"image\":\"%s\",\"model\":\"%s\"}", image_ref, model);
    }
    free(image_ref); json_free(result);
    return out;
}

static bool openai_image_available(void) {
    const char *key = getenv("OPENAI_API_KEY");
    return key && *key;
}
/* ================================================================
 *  Krea Image Generation
 * ================================================================ */

#define KREA_API_BASE "https://api.krea.ai"

static const char *krea_map_aspect_ratio(const char *aspect_ratio) {
    if (strcmp(aspect_ratio, "landscape") == 0) return "16:9";
    if (strcmp(aspect_ratio, "portrait") == 0) return "9:16";
    return "1:1";
}

static char *krea_image_generate(const char *prompt, const char *aspect_ratio,
                                  const char *model_override) {
    const char *model = (model_override && model_override[0]) ? model_override : "krea-2-medium";
    const char *krea_ar = krea_map_aspect_ratio(aspect_ratio);
    const char *api_key = getenv("KREA_API_KEY");
    if (!api_key || !*api_key) {
        return strdup("{\"success\":false,\"error\":\"KREA_API_KEY not set\"}");
    }
    if (!prompt || !*prompt) {
        return strdup("{\"success\":false,\"error\":\"Prompt is required\"}");
    }

    /* Map model ID to path segment */
    const char *path = "medium";
    if (strstr(model, "large")) path = "large";

    char esc_prompt[4096];
    fal_escape_json(prompt, esc_prompt, sizeof(esc_prompt));

    char body[8192];
    snprintf(body, sizeof(body),
        "{\"prompt\":\"%s\",\"aspect_ratio\":\"%s\",\"resolution\":\"1K\",\"creativity\":\"medium\"}",
        esc_prompt, krea_ar);

    char url[512];
    snprintf(url, sizeof(url), "%s/generate/image/krea/krea-2/%s", KREA_API_BASE, path);

    char auth[1024];
    snprintf(auth, sizeof(auth), "Bearer %s", api_key);

    /* Submit job */
    http_t *h = http_new(30);
    http_resp_t *resp = http_post_json_auth(h, url, body, auth);
    if (!resp) { http_free(h); return strdup("{\"success\":false,\"error\":\"Failed to connect to Krea API\"}"); }

    if (resp->status != 200) {
        char err[2048];
        snprintf(err, sizeof(err), "{\"success\":false,\"error\":\"Krea API HTTP %d: %.200s\"}",
            resp->status, resp->body ? resp->body : "no body");
        http_resp_free(resp); http_free(h);
        return strdup(err);
    }

    char *parse_err = NULL;
    json_t *result = json_parse(resp->body, &parse_err);
    http_resp_free(resp); http_free(h);
    if (!result) {
        char err[1024];
        snprintf(err, sizeof(err), "{\"success\":false,\"error\":\"JSON parse: %s\"}", parse_err ? parse_err : "?");
        free(parse_err); return strdup(err);
    }

    /* Extract job_id for polling */
    const char *job_id = json_get_str(result, "job_id", NULL);
    json_free(result);

    if (!job_id || !*job_id) {
        return strdup("{\"success\":false,\"error\":\"Krea submit response missing job_id\"}");
    }

    /* Poll for completion (simplified: single poll after delay) */
    char job_url[512];
    snprintf(job_url, sizeof(job_url), "%s/jobs/%s", KREA_API_BASE, job_id);

    /* Simple poll: wait 5s then check once */
    sleep(5);

    http_t *h2 = http_new(30);
    http_resp_t *poll_resp = http_get(h2, job_url, auth);
    if (!poll_resp) { http_free(h2); return strdup("{\"success\":false,\"error\":\"Krea poll failed\"}"); }

    if (poll_resp->status != 200) {
        char err[2048];
        snprintf(err, sizeof(err), "{\"success\":false,\"error\":\"Krea poll HTTP %d\"}", poll_resp->status);
        http_resp_free(poll_resp); http_free(h2);
        return strdup(err);
    }

    json_t *job = json_parse(poll_resp->body, NULL);
    http_resp_free(poll_resp); http_free(h2);
    if (!job) {
        return strdup("{\"success\":false,\"error\":\"Krea poll returned invalid JSON\"}");
    }

    const char *status_str = json_get_str(job, "status", NULL);
    if (!status_str || strcmp(status_str, "completed") != 0) {
        char err[1024];
        snprintf(err, sizeof(err), "{\"success\":false,\"error\":\"Krea job not completed (status: %s)\",\"job_id\":\"%s\"}",
            status_str ? status_str : "unknown", job_id);
        json_free(job);
        return strdup(err);
    }

    /* Extract image URL from result */
    json_t *res = json_obj_get(job, "result");
    const char *image_url = NULL;
    if (res) {
        json_t *urls = json_obj_get(res, "urls");
        if (urls && json_len(urls) > 0) {
            json_t *first = json_get(urls, 0);
            if (first) image_url = json_get_str(first, "url", NULL);
        }
        if (!image_url) image_url = json_get_str(res, "url", NULL);
    }
    json_free(job);

    if (!image_url) {
        return strdup("{\"success\":false,\"error\":\"Krea result contained no image URL\"}");
    }

    char *out = (char *)malloc(4096);
    snprintf(out, 4096, "{\"success\":true,\"image\":\"%s\",\"model\":\"%s\"}", image_url, model);
    return out;
}

static bool krea_image_available(void) {
    const char *key = getenv("KREA_API_KEY");
    return key && *key;
}
/* ================================================================
 *  xAI Image Generation
 * ================================================================ */

#define XAI_API_BASE "https://api.x.ai/v1/images/generations"

static const char *xai_map_aspect_ratio(const char *aspect_ratio) {
    if (strcmp(aspect_ratio, "landscape") == 0) return "16:9";
    if (strcmp(aspect_ratio, "portrait") == 0) return "9:16";
    return "1:1";
}

static char *xai_image_generate(const char *prompt, const char *aspect_ratio,
                                 const char *model_override) {
    const char *model = (model_override && model_override[0]) ? model_override : "grok-imagine-image";
    const char *xai_ar = xai_map_aspect_ratio(aspect_ratio);
    const char *api_key = getenv("XAI_API_KEY");
    if (!api_key || !*api_key) {
        return strdup("{\"success\":false,\"error\":\"XAI_API_KEY not set\"}");
    }
    if (!prompt || !*prompt) {
        return strdup("{\"success\":false,\"error\":\"Prompt is required\"}");
    }

    char esc_prompt[4096];
    fal_escape_json(prompt, esc_prompt, sizeof(esc_prompt));

    char body[8192];
    snprintf(body, sizeof(body),
        "{\"model\":\"%s\",\"prompt\":\"%s\",\"aspect_ratio\":\"%s\"}",
        model, esc_prompt, xai_ar);

    char auth[1024];
    snprintf(auth, sizeof(auth), "Bearer %s", api_key);

    http_t *h = http_new(120);
    http_resp_t *resp = http_post_json_auth(h, XAI_API_BASE, body, auth);
    if (!resp) { http_free(h); return strdup("{\"success\":false,\"error\":\"Failed to connect to xAI API\"}"); }

    if (resp->status != 200) {
        char err[2048];
        snprintf(err, sizeof(err), "{\"success\":false,\"error\":\"xAI API HTTP %d: %.200s\"}",
            resp->status, resp->body ? resp->body : "no body");
        http_resp_free(resp); http_free(h);
        return strdup(err);
    }

    char *parse_err = NULL;
    json_t *result = json_parse(resp->body, &parse_err);
    http_resp_free(resp); http_free(h);
    if (!result) {
        char err[1024];
        snprintf(err, sizeof(err), "{\"success\":false,\"error\":\"JSON parse: %s\"}", parse_err ? parse_err : "?");
        free(parse_err); return strdup(err);
    }

    json_t *data = json_obj_get(result, "data");
    if (!data || json_len(data) == 0) { json_free(result); return strdup("{\"success\":false,\"error\":\"No image data\"}"); }

    json_t *first = json_get(data, 0);
    const char *b64 = first ? json_get_str(first, "b64_json", NULL) : NULL;
    const char *url = first ? json_get_str(first, "url", NULL) : NULL;

    char *image_ref = NULL;
    if (b64) {
        /* xAI returns ephemeral URLs — prefer b64 if available */
        image_ref = strdup(b64);
    } else if (url) {
        image_ref = strdup(url);
    }

    if (!image_ref) { json_free(result); return strdup("{\"success\":false,\"error\":\"No image in response\"}"); }

    char *out = (char *)malloc(8192);
    snprintf(out, 8192, "{\"success\":true,\"image\":\"%s\",\"model\":\"%s\"}", image_ref, model);
    free(image_ref); json_free(result);
    return out;
}

static bool xai_image_available(void) {
    const char *key = getenv("XAI_API_KEY");
    return key && *key;
}






/* ================================================================
 *  Image Generation via FAL.ai REST API
 * ================================================================ */

#define FAL_API_BASE "https://fal.run/fal-ai/flux-pro"

/* Generate an image from a text prompt */
char *image_generate_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"success\":false,\"error\":\"Invalid JSON arguments\"}");

    const char *prompt = json_get_str(args, "prompt", "");
    const char *aspect_ratio = json_get_str(args, "aspect_ratio", "1:1");
    const char *negative_prompt = json_get_str(args, "negative_prompt", NULL);
    const char *style = json_get_str(args, "style", NULL);
    const char *image_url = json_get_str(args, "image_url", NULL);
    const char *output_format = json_get_str(args, "output_format", NULL);
    int seed = (int)json_get_num(args, "seed", 0);
    int num_images = (int)json_get_num(args, "num_images", 1);
    bool save_local = json_get_bool(args, "save_local", true);
    const char *provider = json_get_str(args, "provider", "fal");
    const char *model = json_get_str(args, "model", NULL);

    /* Route to the appropriate provider */
    if (provider && strcmp(provider, "openai") == 0) {
        char *result = openai_image_generate(prompt, aspect_ratio, model);
        json_free(args);
        return result;
    }
    if (provider && strcmp(provider, "krea") == 0) {
        char *result = krea_image_generate(prompt, aspect_ratio, model);
        json_free(args);
        return result;
    }
    if (provider && strcmp(provider, "xai") == 0) {
        char *result = xai_image_generate(prompt, aspect_ratio, model);
        json_free(args);
        return result;
    }

    /* Get API key from shared helper (checks FAL_API_KEY, then SLERMES_FAL_KEY) */
    if (!fal_get_api_key()) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"FAL_API_KEY not set. Get a key at https://fal.ai\"}");
    }

    if (!prompt || !*prompt) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"Missing 'prompt' parameter\"}");
    }

    /* Escape prompt for JSON */
    char esc_prompt[4096];
    fal_escape_json(prompt, esc_prompt, sizeof(esc_prompt));

    /* Build request body with optional params */
    char body[16384];
    int pos = snprintf(body, sizeof(body),
        "{\"prompt\":\"%s\",\"aspect_ratio\":\"%s\"",
        esc_prompt, aspect_ratio);

    if (image_url && *image_url) {
        pos += snprintf(body + pos, sizeof(body) - pos,
            ",\"image_url\":\"%s\"", image_url);
    }
    if (output_format && *output_format) {
        pos += snprintf(body + pos, sizeof(body) - pos,
            ",\"output_format\":\"%s\"", output_format);
    }
    if (negative_prompt && *negative_prompt) {
        char esc_neg[2048];
        fal_escape_json(negative_prompt, esc_neg, sizeof(esc_neg));
        pos += snprintf(body + pos, sizeof(body) - pos,
            ",\"negative_prompt\":\"%s\"", esc_neg);
    }
    if (style && *style) {
        pos += snprintf(body + pos, sizeof(body) - pos,
            ",\"style\":\"%s\"", style);
    }
    if (seed > 0) {
        pos += snprintf(body + pos, sizeof(body) - pos,
            ",\"seed\":%d", seed);
    }
    if (num_images > 1) {
        if (num_images > 4) num_images = 4;
        pos += snprintf(body + pos, sizeof(body) - pos,
            ",\"num_images\":%d", num_images);
    }
    snprintf(body + pos, sizeof(body) - pos, "}");

    /* Use shared FAL POST helper */
    http_resp_t *resp = fal_post_json(FAL_API_BASE, body, 60);

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

    /* Extract image URL from response */
    json_t *images = json_obj_get(result, "images");
    const char *result_image_url = NULL;
    if (images && json_len(images) > 0) {
        json_t *first = json_get(images, 0);
        if (first) result_image_url = json_get_str(first, "url", NULL);
    }

    if (!result_image_url) {
        char *s = json_serialize(result);
        char err[2048];
        snprintf(err, sizeof(err),
            "{\"success\":false,\"error\":\"No image URL in response\",\"response\":%s}",
            s ? s : "null");
        free(s);
        json_free(result);
        json_free(args);
        return strdup(err);
    }

    /* Download the image to a local file (if save_local is true) */
    char filename[256] = "";
    int dl_ok = 0;
    char dl_error[256] = "";
    if (save_local) {
        snprintf(filename, sizeof(filename), "/tmp/slermes_img_%ld.png",
            (long)time(NULL));

        http_t *dh = http_new(60);
        http_resp_t *img_resp = http_get(dh, result_image_url, NULL);
        if (img_resp) {
            if (img_resp->status == 200) {
                if (img_resp->body && img_resp->body_len > 0) {
                    /* D09: Max download size check — reject >50MB images */
                    if (img_resp->body_len <= 50 * 1024 * 1024) {
                        FILE *f = fopen(filename, "wb");
                        if (f) {
                            fwrite(img_resp->body, 1, img_resp->body_len, f);
                            fclose(f);
                            dl_ok = 1;
                        } else {
                            snprintf(dl_error, sizeof(dl_error), "Cannot write %s", filename);
                        }
                    } else {
                        snprintf(dl_error, sizeof(dl_error),
                                 "Image too large (%.1f MB > 50 MB limit)",
                                 img_resp->body_len / (1024.0 * 1024.0));
                    }
                } else {
                    snprintf(dl_error, sizeof(dl_error), "Empty response body");
                }
            } else {
                snprintf(dl_error, sizeof(dl_error), "HTTP %d", img_resp->status);
            }
            http_resp_free(img_resp);
        } else {
            snprintf(dl_error, sizeof(dl_error), "Connection timeout or failed");
        }
        if (dh) http_free(dh);
    }

    /* Build response */
    char out[8192];
    if (dl_ok) {
        snprintf(out, sizeof(out),
            "{\"success\":true,\"image\":\"%s\",\"local_path\":\"%s\"}", result_image_url, filename);
    } else {
        snprintf(out, sizeof(out),
            "{\"success\":true,\"image\":\"%s\",\"warning\":\"%s\"}", result_image_url,
            dl_error[0] ? dl_error : "Could not download image");
    }

    json_free(result);
    json_free(args);
    return strdup(out);
}

/* ================================================================
 *  Image save helpers (port of Python image_gen_provider.py)
 * ================================================================ */

/** Generate a cache/images/ filename with timestamp + random suffix — Port of Python _images_cache_dir() */
static char *image_gen_make_cache_path(const char *hermes_home,
                                        const char *prefix,
                                        const char *extension) {
    /* Build dir: ~/.hermes/cache/images/ */
    char dir[4096];
    int n = snprintf(dir, sizeof(dir), "%s/cache/images", hermes_home);
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
             dir, prefix ? prefix : "image", ts, rand_hex,
             extension ? extension : "png");
    return path;
}

/* Decode base64 image data and save to cache/images/.
 * Port of Python image_gen_provider.py:save_b64_image(). */
char *image_gen_save_b64_image(const char *b64_data, const char *prefix,
                                const char *extension) {
    const char *home = getenv("HERMES_HOME");
    char fallback[1024];
    if (!home || !*home) {
        const char *h = getenv("HOME");
        if (!h) return NULL;
        snprintf(fallback, sizeof(fallback), "%s/.hermes", h);
        home = fallback;
    }

    char *path = image_gen_make_cache_path(home, prefix, extension);
    if (!path) return NULL;

    /* Decode base64 */
    size_t out_len = 0;
    unsigned char *decoded = base64_decode(b64_data, &out_len);
    if (!decoded || out_len == 0) {
        free(path);
        free(decoded);
        return NULL;
    }

    /* Write file */
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

/* Content-Type → extension map. */
static const char *image_gen_ct_to_ext(const char *content_type) {
    if (!content_type) return NULL;
    if (strstr(content_type, "image/png")) return "png";
    if (strstr(content_type, "image/jpeg")) return "jpg";
    if (strstr(content_type, "image/jpg")) return "jpg";
    if (strstr(content_type, "image/webp")) return "webp";
    if (strstr(content_type, "image/gif")) return "gif";
    return NULL;
}

/* Infer extension from URL path suffix. */
static const char *image_gen_url_to_ext(const char *url) {
    if (!url || !*url) return NULL;
    const char *q = strchr(url, '?');
    size_t ulen = q ? (size_t)(q - url) : strlen(url);
    const char *dot = NULL;
    for (size_t i = 1; i < ulen; i++) {
        if (url[i] == '.') dot = url + i;
    }
    if (!dot) return NULL;
    const char *ext = dot + 1;
    if (strcmp(ext, "jpeg") == 0) return "jpg";
    if (strcmp(ext, "png") == 0) return "png";
    if (strcmp(ext, "jpg") == 0) return "jpg";
    if (strcmp(ext, "webp") == 0) return "webp";
    if (strcmp(ext, "gif") == 0) return "gif";
    return NULL;
}

/* Download an image URL and save to cache/images/.
 * Port of Python image_gen_provider.py:save_url_image(). */
char *image_gen_save_url_image(const char *url, const char *prefix,
                                double timeout_sec, int max_bytes) {
    const char *home = getenv("HERMES_HOME");
    char fallback[1024];
    if (!home || !*home) {
        const char *h = getenv("HOME");
        if (!h) return NULL;
        snprintf(fallback, sizeof(fallback), "%s/.hermes", h);
        home = fallback;
    }

    if (max_bytes <= 0) max_bytes = 25 * 1024 * 1024; /* 25 MB default */

    /* Download */
    int to = (timeout_sec > 0) ? (int)timeout_sec : 60;
    http_t *h = http_new(to);
    if (!h) return NULL;
    http_resp_t *resp = http_get(h, url, NULL);
    if (!resp || resp->status != 200) {
        if (resp) http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    /* Check max size */
    if (resp->body_len > (size_t)max_bytes) {
        http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    /* Infer extension */
    const char *ext = NULL;
    /* Try Content-Type header first */
    if (resp->headers) {
        const char *ct = strstr(resp->headers, "Content-Type:");
        if (!ct) ct = strstr(resp->headers, "content-type:");
        if (ct) {
            ct += 13; /* skip "Content-Type: " */
            while (*ct == ' ') ct++;
            char ct_buf[128];
            int ci = 0;
            while (*ct && *ct != ';' && *ct != '\r' && *ct != '\n' && ci < 127)
                ct_buf[ci++] = *ct++;
            ct_buf[ci] = '\0';
            ext = image_gen_ct_to_ext(ct_buf);
        }
    }
    /* Fall back to URL suffix */
    if (!ext) ext = image_gen_url_to_ext(url);
    if (!ext) ext = "png";

    /* Save */
    char *path = image_gen_make_cache_path(home, prefix, ext);
    if (!path) {
        http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(path);
        http_resp_free(resp);
        http_free(h);
        return NULL;
    }
    fwrite(resp->body, 1, resp->body_len, f);
    fclose(f);

    http_resp_free(resp);
    http_free(h);
    return path;
}

/* ================================================================
 *  Registration
 * ================================================================ */

/* FAL provider availability check */
static bool fal_image_is_available(void) {
    return fal_get_api_key() != NULL;
}

void registry_init_image_gen(void) {
    /* Register FAL provider in the image_gen registry */
    image_gen_provider_t fal_provider;
    memset(&fal_provider, 0, sizeof(fal_provider));
    snprintf(fal_provider.name, sizeof(fal_provider.name), "%s", "fal");
    snprintf(fal_provider.display_name, sizeof(fal_provider.display_name), "%s", "FAL.ai (Flux-Pro)");
    fal_provider.is_available = fal_image_is_available;
    image_gen_register_provider(&fal_provider);

    /* Register OpenAI DALL-E provider */
    image_gen_provider_t openai_provider;
    memset(&openai_provider, 0, sizeof(openai_provider));
    snprintf(openai_provider.name, sizeof(openai_provider.name), "%s", "openai");
    snprintf(openai_provider.display_name, sizeof(openai_provider.display_name), "%s", "OpenAI (DALL-E)");
    openai_provider.is_available = openai_image_available;
    image_gen_register_provider(&openai_provider);

    /* Register Krea provider */
    image_gen_provider_t krea_provider;
    memset(&krea_provider, 0, sizeof(krea_provider));
    snprintf(krea_provider.name, sizeof(krea_provider.name), "%s", "krea");
    snprintf(krea_provider.display_name, sizeof(krea_provider.display_name), "%s", "Krea");
    krea_provider.is_available = krea_image_available;
    image_gen_register_provider(&krea_provider);

    /* Register xAI provider */
    image_gen_provider_t xai_provider;
    memset(&xai_provider, 0, sizeof(xai_provider));
    snprintf(xai_provider.name, sizeof(xai_provider.name), "%s", "xai");
    snprintf(xai_provider.display_name, sizeof(xai_provider.display_name), "%s", "xAI (Grok)");
    xai_provider.is_available = xai_image_available;
    image_gen_register_provider(&xai_provider);

    registry_register("image_generate",
        "Generate an image from a text prompt using the configured FAL API.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
        "  \"prompt\":{\"type\":\"string\",\"description\":\"Text description of the image to generate\"},"
        "  \"aspect_ratio\":{\"type\":\"string\",\"description\":\"Aspect ratio (e.g., 1:1, 16:9, 9:16)\"},"
        "  \"negative_prompt\":{\"type\":\"string\",\"description\":\"What to avoid in the generated image\"},"
        "  \"style\":{\"type\":\"string\",\"description\":\"Style preset (e.g., realistic, anime, cinematic, digital-art, fantasy)\"},"
        "  \"seed\":{\"type\":\"integer\",\"description\":\"Random seed for reproducibility (0=random)\"},\""
        "  \"num_images\":{\"type\":\"integer\",\"description\":\"Number of images to generate (1-4)\",\"default\":1},\""
        "  \"image_url\":{\"type\":\"string\",\"description\":\"Reference image URL for image-to-image generation (img2img). Provide a URL to an existing image as source. \"},\""
        "  \"output_format\":{\"type\":\"string\",\"description\":\"Output image format: png, jpeg, webp (default: API default)\"},\""
        "  \"save_local\":{\"type\":\"boolean\",\"description\":\"Download and save the image locally. Set false to return URL only.\",\"default\":true},\""
        "  \"provider\":{\"type\":\"string\",\"description\":\"Image generation provider: fal (FAL.ai Flux), openai (DALL-E), krea (Krea 2), xai (Grok).\",\"default\":\"fal\"},\""
        "  \"model\":{\"type\":\"string\",\"description\":\"Model override. For openai: dall-e-2, dall-e-3, gpt-image-1. For krea: krea-2-medium, krea-2-large. For xai: grok-imagine-image.\",\"default\":\"dall-e-3\"}\""
        "},"
        "\"required\":[\"prompt\"]"
        "}",
        image_generate_handler);
}

/* ================================================================
 *  Image Generation Provider Helpers (port of Python image_gen_provider.py)
 * ================================================================ */

/* Clamp aspect_ratio to valid set: landscape, square, portrait.
 * Port of Python image_gen_provider.py:resolve_aspect_ratio(). */
const char *image_gen_resolve_aspect_ratio(const char *value) {
    if (!value) return "landscape";
    if (strcmp(value, "square") == 0) return "square";
    if (strcmp(value, "portrait") == 0) return "portrait";
    if (strcmp(value, "1:1") == 0) return "square";
    if (strcmp(value, "9:16") == 0) return "portrait";
    return "landscape";
}

/* Port of Python tools/memory_tool.py:_success_response(). */
/* Build a uniform success response JSON string. Returns malloc'd string (caller free).
 * Port of Python image_gen_provider.py:success_response(). */
char *image_gen_success_response(const char *image, const char *model,
                                  const char *prompt, const char *aspect_ratio,
                                  const char *provider, const char *extra_json) {
    json_t *resp = json_object();
    json_set(resp, "success", json_bool(true));
    json_set(resp, "image", json_string(image ? image : ""));
    json_set(resp, "model", json_string(model ? model : ""));
    json_set(resp, "prompt", json_string(prompt ? prompt : ""));
    json_set(resp, "aspect_ratio", json_string(aspect_ratio ? aspect_ratio : "landscape"));
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
 * Port of Python image_gen_provider.py:error_response(). */
char *image_gen_error_response(const char *error, const char *error_type,
                                const char *provider, const char *model,
                                const char *prompt, const char *aspect_ratio) {
    json_t *resp = json_object();
    json_set(resp, "success", json_bool(false));
    json_set(resp, "image", json_null());
    json_set(resp, "error", json_string(error ? error : "Unknown error"));
    json_set(resp, "error_type", json_string(error_type ? error_type : "provider_error"));
    json_set(resp, "model", json_string(model ? model : ""));
    json_set(resp, "prompt", json_string(prompt ? prompt : ""));
    json_set(resp, "aspect_ratio", json_string(aspect_ratio ? aspect_ratio : "landscape"));
    json_set(resp, "provider", json_string(provider ? provider : ""));
    char *out = json_serialize(resp);
    json_free(resp);
    return out;
}
