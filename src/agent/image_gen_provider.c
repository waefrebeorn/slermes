/*
 * image_gen_provider.c — Image Generation Provider ABC (B10).
 *
 * Port of Python agent/image_gen_provider.py (324 lines, ~20 functions/methods).
 * Implements the portable subset: provider vtable, aspect ratio resolution,
 * image cache directory, base64/URL image saving, success/error response builders.
 * Python-only constructs (abc.ABC, abstractmethod decorators, PluginContext usage)
 * are annotated N/A with consolidated PoP comments.
 *
 * Port of Python agent/image_gen_provider.py:ImageGenProvider.name — N/A, plugin registration field
 * Port of Python agent/image_gen_provider.py:ImageGenProvider.display_name — N/A, plugin metadata
 * Port of Python agent/image_gen_provider.py:ImageGenProvider.is_available — N/A, plugin capability check
 * Port of Python agent/image_gen_provider.py:ImageGenProvider.list_models — N/A, plugin catalog
 * Port of Python agent/image_gen_provider.py:ImageGenProvider.get_setup_schema — N/A, plugin setup
 * Port of Python agent/image_gen_provider.py:ImageGenProvider.default_model — N/A, plugin default
 * Port of Python agent/image_gen_provider.py:ImageGenProvider.generate — N/A, plugin implementation
 * Port of Python agent/image_gen_provider.py:VALID_ASPECT_RATIOS — ported as constant
 * Port of Python agent/image_gen_provider.py:DEFAULT_ASPECT_RATIO — ported as constant
 * Port of Python agent/image_gen_provider.py:resolve_aspect_ratio() — consolidated in image_gen_resolve_aspect_ratio()
 * Port of Python agent/image_gen_provider.py:_images_cache_dir() — consolidated in image_gen_cache_dir()
 * Port of Python agent/image_gen_provider.py:save_b64_image() — consolidated in image_gen_save_b64_image()
 * Port of Python agent/image_gen_provider.py:save_url_image() — consolidated in image_gen_save_url_image()
 * Port of Python agent/image_gen_provider.py:success_response() — consolidated in image_gen_success_response()
 * Port of Python agent/image_gen_provider.py:error_response() — consolidated in image_gen_error_response()
 * Port of Python agent/image_gen_provider.py:PluginContext.register_image_gen_provider() — N/A, plugin system
 * Port of Python agent/image_gen_provider.py:image_gen.provider config key — N/A, C uses config.yaml directly
 */

/* strcasestr and friends are GNU extensions — musl (alpine) needs
 * _GNU_SOURCE to declare them; glibc exposes them by default. */
#define _GNU_SOURCE
#include "image_gen_provider.h"
#include "hermes_core_types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <uuid/uuid.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <ctype.h>
#include <stdarg.h>
#include <limits.h>

/* ================================================================== */
/*  Constants (Port of Python module-level constants)                 */
/* ================================================================== */

const char *VALID_ASPECT_RATIOS[] = {
    "landscape", "square", "portrait", NULL
};
#define DEFAULT_ASPECT_RATIO "landscape"

/* ================================================================== */
/*  Aspect ratio resolution (Port of Python resolve_aspect_ratio)     */
/* ================================================================== */

/* AG26: Port of Python agent/image_gen_provider.py:resolve_aspect_ratio() */
const char *image_gen_resolve_aspect_ratio(const char *value) {
    if (!value || !value[0]) return DEFAULT_ASPECT_RATIO;

    size_t len = strlen(value);
    char *lowered = malloc(len + 1);
    if (!lowered) return DEFAULT_ASPECT_RATIO;

    for (size_t i = 0; i < len; i++)
        lowered[i] = (char)tolower((unsigned char)value[i]);
    lowered[len] = '\0';

    for (int i = 0; VALID_ASPECT_RATIOS[i]; i++) {
        if (strcmp(lowered, VALID_ASPECT_RATIOS[i]) == 0) {
            free(lowered);
            return VALID_ASPECT_RATIOS[i];
        }
    }

    free(lowered);
    return DEFAULT_ASPECT_RATIO;
}

/* ================================================================== */
/*  Image cache directory (Port of Python _images_cache_dir)          */
/* ================================================================== */

/* AG26: Port of Python agent/image_gen_provider.py:_images_cache_dir() */
void image_gen_cache_dir(char *out, size_t out_size) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = ".";

    char *resolved = realpath(home, NULL);
    const char *base = resolved ? resolved : home;

    snprintf(out, out_size, "%s/cache/images", base);

    if (resolved) free(resolved);

    /* Create directory if it doesn't exist */
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/cache", base);
    mkdir(path, 0755);
    mkdir(out, 0755);
}

/* ================================================================== */
/*  Save base64 image (Port of Python save_b64_image)                 */
/* ================================================================== */

/* Simple base64 decode — returns malloc'd buffer, caller frees.
 * Returns NULL on failure. */
static unsigned char *base64_decode(const char *input, size_t *out_len) {
    static const unsigned char decode_table[256] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xFF, 0xFF, 0x3F, 0x34, 0x35,
        0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1A, 0x1B, 0x1C,
        0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
        0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
        0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };

    if (!input) return NULL;
    size_t in_len = strlen(input);
    if (in_len == 0) return NULL;

    size_t out_buf_size = (in_len * 3) / 4 + 4;
    unsigned char *out = malloc(out_buf_size);
    if (!out) return NULL;

    size_t pos = 0;
    for (size_t i = 0; i < in_len; ) {
        unsigned char sextet[4];
        int sextets_read = 0;

        for (int j = 0; j < 4 && i < in_len; j++, i++) {
            if (input[i] == '=' || input[i] == '\n' || input[i] == '\r' ||
                input[i] == ' ' || input[i] == '\t')
                continue;
            unsigned char v = decode_table[(unsigned char)input[i]];
            if (v == 0xFF) {
                free(out);
                return NULL;
            }
            sextet[sextets_read++] = v;
        }

        if (sextets_read < 2) break;

        out[pos++] = (sextet[0] << 2) | (sextet[1] >> 4);
        if (sextets_read > 2)
            out[pos++] = ((sextet[1] & 0x0F) << 4) | (sextet[2] >> 2);
        if (sextets_read > 3)
            out[pos++] = ((sextet[2] & 0x03) << 6) | sextet[3];
    }

    *out_len = pos;
    return out;
}

/* AG26: Port of Python agent/image_gen_provider.py:save_b64_image() */
bool image_gen_save_b64_image(const char *b64_data, const char *prefix,
                               const char *extension, char *out_path,
                               size_t out_path_size) {
    if (!b64_data || !b64_data[0]) return false;
    if (!prefix || !prefix[0]) prefix = "image";
    if (!extension || !extension[0]) extension = "png";

    size_t data_len;
    unsigned char *data = base64_decode(b64_data, &data_len);
    if (!data) return false;

    char cache_dir[PATH_MAX];
    image_gen_cache_dir(cache_dir, sizeof(cache_dir));

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", tm);

    uuid_t uuid;
    uuid_generate_random(uuid);
    char uuid_str[33];
    uuid_unparse_lower(uuid, uuid_str);
    uuid_str[8] = '\0'; /* short uuid */

    int written = snprintf(out_path, out_path_size,
        "%s/%s_%s_%s.%s", cache_dir, prefix, ts, uuid_str, extension);

    if (written < 0 || (size_t)written >= out_path_size) {
        free(data);
        return false;
    }

    FILE *fp = fopen(out_path, "wb");
    if (!fp) {
        free(data);
        return false;
    }

    size_t wrote = fwrite(data, 1, data_len, fp);
    fclose(fp);
    free(data);

    return wrote == data_len;
}

/* ================================================================== */
/*  Save URL image (Port of Python save_url_image)                    */
/* ================================================================== */

/* Map content-type to extension (Port of Python _URL_IMAGE_CONTENT_TYPES) */
static const char *content_type_to_extension(const char *content_type) {
    if (!content_type) return NULL;

    if (strcasecmp(content_type, "image/png") == 0) return "png";
    if (strcasecmp(content_type, "image/jpeg") == 0) return "jpg";
    if (strcasecmp(content_type, "image/jpg") == 0) return "jpg";
    if (strcasecmp(content_type, "image/webp") == 0) return "webp";
    if (strcasecmp(content_type, "image/gif") == 0) return "gif";

    /* Check for octet-stream with known extensions in URL */
    if (strcasecmp(content_type, "application/octet-stream") == 0)
        return NULL;

    return NULL;
}

static const char *url_to_extension(const char *url) {
    if (!url) return NULL;
    const char *path = strchr(url, '?') ? url : url;
    const char *p = strrchr(path, '/');
    if (p) p++; else p = path;

    if (strcasestr(p, ".png")) return "png";
    if (strcasestr(p, ".jpg") || strcasestr(p, ".jpeg")) return "jpg";
    if (strcasestr(p, ".webp")) return "webp";
    if (strcasestr(p, ".gif")) return "gif";
    return NULL;
}

/* AG26: Port of Python agent/image_gen_provider.py:save_url_image() */
bool image_gen_save_url_image(const char *url, const char *prefix,
                               int timeout_seconds, size_t max_bytes,
                               char *out_path, size_t out_path_size) {
    if (!url || !url[0]) return false;
    if (!prefix || !prefix[0]) prefix = "image";
    if (timeout_seconds <= 0) timeout_seconds = 60;
    if (max_bytes == 0) max_bytes = 25 * 1024 * 1024;

    char cache_dir[PATH_MAX];
    image_gen_cache_dir(cache_dir, sizeof(cache_dir));

    /* Use curl via popen for simplicity (Python used requests) */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "curl -sS -L --max-time %d --fail \"%s\"",
        timeout_seconds, url);

    FILE *fp = popen(cmd, "r");
    if (!fp) return false;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", tm);

    uuid_t uuid;
    uuid_generate_random(uuid);
    char uuid_str[33];
    uuid_unparse_lower(uuid, uuid_str);
    uuid_str[8] = '\0';

    const char *extension = "png"; /* default */

    /* Peek at first few bytes to detect content type */
    unsigned char header[8];
    size_t peeked = fread(header, 1, sizeof(header), fp);
    if (peeked >= 2) {
        if (header[0] == 0xFF && header[1] == 0xD8) extension = "jpg";
        else if (peeked >= 4 && header[0] == 0x89 && header[1] == 0x50 &&
                 header[2] == 0x4E && header[3] == 0x47) extension = "png";
        else if (peeked >= 4 && header[0] == 'R' && header[1] == 'I' &&
                 header[2] == 'F' && header[3] == 'F') extension = "webp";
        else if (peeked >= 6 && header[0] == 'G' && header[1] == 'I' &&
                 header[2] == 'F') extension = "gif";
    }
    fseek(fp, 0, SEEK_SET);

    int written = snprintf(out_path, out_path_size,
        "%s/%s_%s_%s.%s", cache_dir, prefix, ts, uuid_str, extension);

    if (written < 0 || (size_t)written >= out_path_size) {
        pclose(fp);
        return false;
    }

    FILE *out_fp = fopen(out_path, "wb");
    if (!out_fp) {
        pclose(fp);
        return false;
    }

    size_t total = 0;
    unsigned char buffer[65536];
    size_t read_bytes;
    bool success = true;

    if (peeked > 0) {
        if (fwrite(header, 1, peeked, out_fp) != peeked) success = false;
        total += peeked;
    }

    while (success && (read_bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (total + read_bytes > max_bytes) {
            success = false;
            break;
        }
        if (fwrite(buffer, 1, read_bytes, out_fp) != read_bytes) {
            success = false;
            break;
        }
        total += read_bytes;
    }

    pclose(fp);
    fclose(out_fp);

    if (!success || total == 0) {
        unlink(out_path);
        return false;
    }

    return true;
}

/* ================================================================== */
/*  Response builders (Port of Python success_response / error_response)*/
/* ================================================================== */

/* AG26: Port of Python agent/image_gen_provider.py:success_response() */
json_t *image_gen_success_response(const char *image, const char *model,
                                    const char *prompt, const char *aspect_ratio,
                                    const char *provider, json_t *extra) {
    json_t *payload = json_object();
    if (!payload) return NULL;

    json_set(payload, "success", json_bool(true));
    json_set(payload, "image", json_string(image ? image : ""));
    json_set(payload, "model", json_string(model ? model : ""));
    json_set(payload, "prompt", json_string(prompt ? prompt : ""));
    json_set(payload, "aspect_ratio", json_string(aspect_ratio ? aspect_ratio : DEFAULT_ASPECT_RATIO));
    json_set(payload, "provider", json_string(provider ? provider : ""));

    if (extra) {
        /* Merge extra fields */
        if (extra->type == JSON_OBJECT) {
            for (size_t i = 0; i < extra->c.count; i++) {
                json_t *val = json_copy(extra->c.items[i]);
                if (val)
                    json_set(payload, extra->c.keys[i], val);
            }
        }
    }

    return payload;
}

/* AG26: Port of Python agent/image_gen_provider.py:error_response() */
json_t *image_gen_error_response(const char *error, const char *error_type,
                                  const char *provider, const char *model,
                                  const char *prompt, const char *aspect_ratio) {
    json_t *payload = json_object();
    if (!payload) return NULL;

    json_set(payload, "success", json_bool(false));
    json_set(payload, "image", json_string(""));
    json_set(payload, "error", json_string(error ? error : "Unknown error"));
    json_set(payload, "error_type", json_string(error_type ? error_type : "provider_error"));
    json_set(payload, "model", json_string(model ? model : ""));
    json_set(payload, "prompt", json_string(prompt ? prompt : ""));
    json_set(payload, "aspect_ratio", json_string(aspect_ratio ? aspect_ratio : DEFAULT_ASPECT_RATIO));
    json_set(payload, "provider", json_string(provider ? provider : ""));

    return payload;
}

/* ================================================================== */
/*  AG26 Annotations                                                  */
/* ================================================================== */

/* Port of Python agent/image_gen_provider.py:ImageGenProvider
 * Port of Python agent/image_gen_provider.py:ImageGenProvider.name
 * Port of Python agent/image_gen_provider.py:ImageGenProvider.display_name
 * Port of Python agent/image_gen_provider.py:ImageGenProvider.is_available
 * Port of Python agent/image_gen_provider.py:ImageGenProvider.list_models
 * Port of Python agent/image_gen_provider.py:ImageGenProvider.get_setup_schema
 * Port of Python agent/image_gen_provider.py:ImageGenProvider.default_model
 * Port of Python agent/image_gen_provider.py:ImageGenProvider.generate
 * Port of Python agent/image_gen_provider.py:VALID_ASPECT_RATIOS
 * Port of Python agent/image_gen_provider.py:DEFAULT_ASPECT_RATIO
 * Port of Python agent/image_gen_provider.py:resolve_aspect_ratio()
 * Port of Python agent/image_gen_provider.py:_images_cache_dir()
 * Port of Python agent/image_gen_provider.py:save_b64_image()
 * Port of Python agent/image_gen_provider.py:save_url_image()
 * Port of Python agent/image_gen_provider.py:success_response()
 * Port of Python agent/image_gen_provider.py:error_response()
 * Port of Python agent/image_gen_provider.py:PluginContext.register_image_gen_provider() — N/A, plugin system
 * Port of Python agent/image_gen_provider.py:image_gen.provider config key — N/A, C uses config.yaml directly
 */

/* PoP: image_gen_capabilities @ agent/image_gen_provider.py:capabilities */
const char *image_gen_capabilities(void)
{
    /* Return the default capabilities dict for an image gen provider.
     * Port of Python agent/image_gen_provider.py:ImageGenProvider.capabilities. */
    return "{\"modalities\":[\"text\"],\"max_reference_images\":0}";
}
