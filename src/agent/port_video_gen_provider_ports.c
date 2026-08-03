/*
 * port_video_gen_provider_remaining.c — Port of agent/video_gen_provider.py
 * provider protocol surface. Catalog metadata, availability, model
 * listing, setup schema, b64/bytes cache saves, uniform responses.
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
#include "json.h"
#include "video_gen_registry.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static const char *videos_cache_dir(void) {
    const char *h = getenv("HERMES_HOME");
    static char buf[1024];
    if (h && *h) snprintf(buf, sizeof(buf), "%s/cache/videos", h);
    else snprintf(buf, sizeof(buf), "%s/.hermes/cache/videos", getenv("HOME") ? getenv("HOME") : ".");
    return buf;
}

/* PoP: name @ agent/video_gen_provider.py:name */
char *vgp_name(void) {
    /* Python: stable short id. */
    return strdup("video_gen");
}

/* PoP: display_name @ agent/video_gen_provider.py:display_name */
char *vgp_display_name(void) {
    /* Python: title-cased label. */
    return strdup("Video Gen");
}

/* PoP: is_available @ agent/video_gen_provider.py:is_available */
bool vgp_is_available(void) {
    /* Python: typically key presence check. Any registered provider that
     * reports available (delegates to the real registry). */
    extern int video_gen_provider_count(void);
    extern const video_gen_provider_t *video_gen_get_provider_by_index(int idx);
    int n = video_gen_provider_count();
    for (int i = 0; i < n; i++) {
        const video_gen_provider_t *p = video_gen_get_provider_by_index(i);
        if (p && p->is_available && p->is_available()) return true;
    }
    return false;
}

/* PoP: list_models @ agent/video_gen_provider.py:list_models */
char *vgp_list_models(void) {
    /* Python: catalog entries for the picker. Return the registered
     * provider names as a JSON array of {id, name}. */
    extern int video_gen_provider_count(void);
    extern const video_gen_provider_t *video_gen_get_provider_by_index(int idx);
    int n = video_gen_provider_count();
    json_t *arr = json_array();
    for (int i = 0; i < n; i++) {
        const video_gen_provider_t *p = video_gen_get_provider_by_index(i);
        if (!p) continue;
        json_t *entry = json_object();
        json_set(entry, "id", json_string(p->name));
        if (p->display_name[0]) json_set(entry, "name", json_string(p->display_name));
        json_append(arr, entry);
    }
    char *ser = json_serialize(arr);
    json_free(arr);
    return ser ? ser : strdup("[]");
}

/* PoP: get_setup_schema @ agent/video_gen_provider.py:get_setup_schema */
char *vgp_get_setup_schema(void) {
    /* Python: picker metadata. */
    return strdup("{\"fields\": [{\"key\": \"api_key\", \"label\": \"API Key\", "
                  "\"type\": \"password\"}]}");
}

/* PoP: default_model @ agent/video_gen_provider.py:default_model */
char *vgp_default_model(void) {
    /* Python: first model id or None. */
    extern int video_gen_provider_count(void);
    extern const video_gen_provider_t *video_gen_get_provider_by_index(int idx);
    int n = video_gen_provider_count();
    if (n <= 0) return NULL;
    const video_gen_provider_t *p = video_gen_get_provider_by_index(0);
    if (!p || !p->name[0]) return NULL;
    return strdup(p->name);
}

/* PoP: capabilities @ agent/video_gen_provider.py:capabilities */
char *vgp_capabilities(void) {
    /* Python: supported feature dict. */
    bool avail = vgp_is_available();
    json_t *out = json_object();
    json_set(out, "text_to_video", json_bool(avail));
    json_set(out, "image_to_video", json_bool(avail));
    char *ser = json_serialize(out);
    json_free(out);
    return ser ? ser : strdup("{\"text_to_video\": false, \"image_to_video\": false}");
}

/* PoP: generate @ agent/video_gen_provider.py:generate */
char *vgp_generate(const char *prompt, const char *input_image) {
    /* Python: text-to-video or image animation. Delegates to the active
     * registered provider's generate. */
    if (!prompt) return NULL;

    extern const video_gen_provider_t *video_gen_get_active_provider(void);
    const video_gen_provider_t *p = video_gen_get_active_provider();
    if (!p || !p->generate) return strdup("{}");

    return p->generate(prompt, "16:9", input_image, 5, 0, false, NULL,
                       input_image ? "image_to_video" : "generate", "720p");
}

/* PoP: save_b64_video @ agent/video_gen_provider.py:save_b64_video */
char *vgp_save_b64_video(const char *b64_data) {
    /* Python: decode + write under cache/videos. */
    if (!b64_data) return NULL;
    mkdir(videos_cache_dir(), 0755);
    char *path = NULL;
    asprintf(&path, "%s/video_%ld.mp4", videos_cache_dir(), (long)time(NULL));
    /* decode base64 (urlsafe) */
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

/* PoP: save_bytes_video @ agent/video_gen_provider.py:save_bytes_video */
char *vgp_save_bytes_video(const char *data, size_t len) {
    /* Python: write raw bytes to cache. */
    if (!data) return NULL;
    mkdir(videos_cache_dir(), 0755);
    char *path = NULL;
    asprintf(&path, "%s/video_%ld.mp4", videos_cache_dir(), (long)time(NULL));
    FILE *w = fopen(path, "wb");
    if (w) { fwrite(data, 1, len, w); fclose(w); }
    return path;
}

/* PoP: success_response @ agent/video_gen_provider.py:success_response */
char *vgp_success_response(const char *video, const char *model) {
    /* Python: uniform success dict. */
    char *out = NULL;
    asprintf(&out, "{\"success\": true, \"video\": \"%s\", \"model\": \"%s\"}",
             video ? video : "", model ? model : "");
    return out;
}

/* PoP: error_response @ agent/video_gen_provider.py:error_response */
char *vgp_error_response(const char *error, const char *code) {
    /* Python: uniform error dict. */
    char *out = NULL;
    asprintf(&out, "{\"success\": false, \"error\": \"%s\", \"code\": \"%s\"}",
             error ? error : "unknown", code ? code : "");
    return out;
}
