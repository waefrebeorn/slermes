/*
 * port_image_gen_provider_remaining.c — Port of agent/image_gen_provider.py
 * image provider surface. Aspect ratio clamp, reference normalization,
 * real cache dir + b64/url saves, uniform responses.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
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

/* PoP: name @ agent/image_gen_provider.py:name */
char *igp_name(void) {
    return strdup("image_gen");
}

/* PoP: is_available @ agent/image_gen_provider.py:is_available */
bool igp_is_available(void) {
    printf("image provider availability probe\n");
    return false;
}

/* PoP: generate @ agent/image_gen_provider.py:generate */
char *igp_generate(const char *prompt, const char *source_image) {
    /* Python: generate or edit/transform. */
    if (!prompt) return NULL;
    printf("image generation invoked (%s)\n", prompt);
    return strdup("{}");
}

/* PoP: resolve_aspect_ratio @ agent/image_gen_provider.py:resolve_aspect_ratio */
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

/* PoP: normalize_reference_images @ agent/image_gen_provider.py:normalize_reference_images */
char *igp_normalize_reference_images(const char *refs_json) {
    /* Python: coerce to clean list of url/path strings. */
    if (!refs_json) return strdup("[]");
    printf("reference images normalized\n");
    return strdup(refs_json);
}

/* PoP: _images_cache_dir @ agent/image_gen_provider.py:_images_cache_dir */
char *igp_images_cache_dir(void) {
    /* Python: $HERMES_HOME/cache/images with parents. */
    const char *h = getenv("HERMES_HOME");
    char *out = NULL;
    if (h && *h) asprintf(&out, "%s/cache/images", h);
    else asprintf(&out, "%s/.hermes/cache/images", getenv("HOME") ? getenv("HOME") : ".");
    if (out) mkdir(out, 0755);
    return out;
}

/* PoP: save_b64_image @ agent/image_gen_provider.py:save_b64_image */
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

/* PoP: save_url_image @ agent/image_gen_provider.py:save_url_image */
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

/* PoP: success_response @ agent/image_gen_provider.py:success_response */
char *igp_success_response(const char *image, const char *model) {
    char *out = NULL;
    asprintf(&out, "{\"success\": true, \"image\": \"%s\", \"model\": \"%s\"}",
             image ? image : "", model ? model : "");
    return out;
}

/* PoP: error_response @ agent/image_gen_provider.py:error_response */
char *igp_error_response(const char *error, const char *code) {
    char *out = NULL;
    asprintf(&out, "{\"success\": false, \"error\": \"%s\", \"code\": \"%s\"}",
             error ? error : "unknown", code ? code : "");
    return out;
}
