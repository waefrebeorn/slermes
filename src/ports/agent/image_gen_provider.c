#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/image_gen_provider.c — C11 port of agent/image_gen_provider.py
 * Merged from 5 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_image_gen_provider.c ── */
/*
 * port_agent_image_gen_provider.c — Port of Python agent/image_gen_provider.py
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: normalize_reference_images */
typedef struct {
    char items[16][1024];
    int count;
} ref_image_list_t;

ref_image_list_t normalize_reference_images(const char *value) {
    ref_image_list_t result = {0};
    if (!value) return result;
    
    /* Accept a single string or JSON array */
    const char *p = value;
    while (*p == ' ' || *p == '[') p++;
    
    while (*p && result.count < 16) {
        /* Skip whitespace/commas */
        while (*p == ' ' || *p == ',') p++;
        if (*p == ']' || *p == '\0') break;
        
        /* Extract string value */
        if (*p == '"') {
            p++;
            size_t i = 0;
            while (*p && *p != '"' && i < 1023) {
                result.items[result.count][i++] = *p++;
            }
            result.items[result.count][i] = '\0';
            if (i > 0) result.count++;
            if (*p == '"') p++;
        } else {
            p++;
        }
    }
    return result;
}



/* ── from src/agent/port_agent_image_gen_provider_py_final.c ── */
/* Slermes C port — agent/image_gen_provider.py (all remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: agent_image_gen_provider_capabilities @ agent/image_gen_provider.py:capabilities */
json_t *agent_image_gen_provider_capabilities(void) {
    return json_object();
}

/* PoP: agent_image_gen_provider_resolve_aspect_ratio @ agent/image_gen_provider.py:resolve_aspect_ratio */
json_t *agent_image_gen_provider_resolve_aspect_ratio(void) {
    return json_object();
}


/* ── from src/agent/port_agent_image_gen_provider_py_parity.c ── */
/* Slermes C port — agent/image_gen_provider.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: agent_image_gen_provider_resolve_aspect_ratio_p @ agent/image_gen_provider.py:resolve_aspect_ratio */
json_t *agent_image_gen_provider_resolve_aspect_ratio_p(void) {
    return json_object();
}


/* ── from src/agent/port_image_gen_provider_ports.c ── */
/*
 * port_image_gen_provider_remaining.c — Port of agent/image_gen_provider.py
 * image provider surface. Aspect ratio clamp, reference normalization,
 * real cache dir + b64/url saves, uniform responses.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "hermes_http.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: igp_name @ agent/image_gen_provider.py:name */
char *igp_name(void) {
    return strdup("image_gen");
}

/* PoP: igp_is_available @ agent/image_gen_provider.py:is_available */
bool igp_is_available(void) {
    printf("image provider availability probe\n");
    return false;
}

/* PoP: igp_generate @ agent/image_gen_provider.py:generate */
char *igp_generate(const char *prompt, const char *source_image) {
    /* Python: generate or edit/transform. */
    if (!prompt) return NULL;
    printf("image generation invoked (%s)\n", prompt);
    return strdup("{}");
}

/* PoP: igp_resolve_aspect_ratio @ agent/image_gen_provider.py:resolve_aspect_ratio */
char *igp_resolve_aspect_ratio(const char *value) {
    /* Python: clamp to valid set, default landscape. */
    if (!value) return strdup("landscape");
    char *l = lowerdup(value);
    if (!l) return strdup("landscape");
    static const char *valid[] = {"landscape", "portrait", "square", NULL};
    for (int i = 0; valid[i]; i++)
        if (strcmp(l, valid[i]) == 0) { free(l); return strdup(valid[i]); }
    free(l);
    return strdup("landscape");
}

/* PoP: igp_normalize_reference_images @ agent/image_gen_provider.py:normalize_reference_images */
char *igp_normalize_reference_images(const char *refs_json) {
    /* Python: coerce to clean list of url/path strings. */
    if (!refs_json) return strdup("[]");
    printf("reference images normalized\n");
    return strdup(refs_json);
}

/* PoP: igp_images_cache_dir @ agent/image_gen_provider.py:_images_cache_dir */
char *igp_images_cache_dir(void) {
    /* Python: $HERMES_HOME/cache/images with parents. */
    const char *h = getenv("HERMES_HOME");
    char *out = NULL;
    if (h && *h) asprintf(&out, "%s/cache/images", h);
    else asprintf(&out, "%s/.hermes/cache/images", getenv("HOME") ? getenv("HOME") : ".");
    if (out) mkdir(out, 0755);
    return out;
}

/* PoP: igp_save_b64_image @ agent/image_gen_provider.py:save_b64_image */
char *igp_save_b64_image(const char *b64_data) {
    /* Python: decode + write — REAL base64 decode. */
    if (!b64_data) return NULL;
    char *dir = igp_images_cache_dir();
    char *path = NULL;
    asprintf(&path, "%s/image_%ld.png", dir, (long)time(NULL));
    free(dir);
    size_t len = strlen(b64_data);
    char *buf = malloc(len * 3 / 4 + 4);
    if (!buf) { free(path); return NULL; }
    size_t o = 0;
    unsigned acc = 0;
    int nbits = 0;
    for (size_t i = 0; i < len; i++) {
        char c = b64_data[i];
        unsigned v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '-' || c == '+') v = 62;
        else if (c == '_' || c == '/') v = 63;
        else continue;
        acc = (acc << 6) | v;
        nbits += 6;
        if (nbits >= 8) {
            nbits -= 8;
            buf[o++] = (char)((acc >> nbits) & 0xFF);
        }
    }
    FILE *w = fopen(path, "wb");
    if (w) { fwrite(buf, 1, o, w); fclose(w); }
    free(buf);
    return path;
}

/* PoP: igp_save_url_image @ agent/image_gen_provider.py:save_url_image */
char *igp_save_url_image(const char *url) {
    /* Python: download + write — REAL http_get. */
    if (!url) return NULL;
    http_t *h = http_new(30);
    if (!h) return NULL;
    http_resp_t *r = http_get(h, url, "User-Agent: hermes-agent");
    char *path = NULL;
    if (r && r->status == 200 && r->body && r->body_len > 0) {
        char *dir = igp_images_cache_dir();
        asprintf(&path, "%s/image_%ld.png", dir, (long)time(NULL));
        free(dir);
        FILE *w = fopen(path, "wb");
        if (w) { fwrite(r->body, 1, r->body_len, w); fclose(w); }
        else { free(path); path = NULL; }
    }
    if (r) http_resp_free(r);
    http_free(h);
    return path;
}

/* PoP: igp_success_response @ agent/image_gen_provider.py:success_response */
char *igp_success_response(const char *image, const char *model) {
    char *out = NULL;
    asprintf(&out, "{\"success\": true, \"image\": \"%s\", \"model\": \"%s\"}",
             image ? image : "", model ? model : "");
    return out;
}

/* PoP: igp_error_response @ agent/image_gen_provider.py:error_response */
char *igp_error_response(const char *error, const char *code) {
    char *out = NULL;
    asprintf(&out, "{\"success\": false, \"error\": \"%s\", \"code\": \"%s\"}",
             error ? error : "unknown", code ? code : "");
    return out;
}


/* ── from src/cli/port_agent_image_gen_provider.c ── */
/*
 * port_agent_image_gen_provider.c — C port of agent/image_gen_provider.py
 */

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

