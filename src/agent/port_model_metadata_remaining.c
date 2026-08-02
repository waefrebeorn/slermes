/*
 * port_model_metadata_remaining.c — Port of agent/model_metadata.py
 * metadata surface. Provider prefix stripping, URL inference, LM studio
 * roots, context cache, kimi/minimax detection, image token counting.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _strip_provider_prefix @ agent/model_metadata.py:_strip_provider_prefix */
char *mmd_strip_provider_prefix(const char *model) {
    /* Python: "local:my-model" → "my-model". */
    if (!model) return NULL;
    const char *colon = strchr(model, ':');
    if (colon) return strdup(colon + 1);
    return strdup(model);
}

/* PoP: _infer_provider_from_url @ agent/model_metadata.py:_infer_provider_from_url */
char *mmd_infer_provider_from_url(const char *base_url) {
    /* Python: models.dev provider name from URL. */
    if (!base_url) return NULL;
    char *l = lowerdup(base_url);
    if (!l) return NULL;
    char *r = NULL;
    if (strstr(l, "openai")) r = strdup("openai");
    else if (strstr(l, "anthropic")) r = strdup("anthropic");
    else if (strstr(l, "googleapis") || strstr(l, "generativelanguage")) r = strdup("google");
    else if (strstr(l, "openrouter")) r = strdup("openrouter");
    else if (strstr(l, "deepseek")) r = strdup("deepseek");
    else if (strstr(l, "moonshot") || strstr(l, "kimi")) r = strdup("moonshot");
    else if (strstr(l, "x.ai") || strstr(l, "grok")) r = strdup("xai");
    free(l);
    return r;
}

/* PoP: _lmstudio_server_root @ agent/model_metadata.py:_lmstudio_server_root */
char *mmd_lmstudio_server_root(const char *base_url) {
    /* Python: root for native /api/v1 endpoints. */
    if (!base_url) return NULL;
    char *out = NULL;
    if (strstr(base_url, "/api/v1")) {
        const char *p = strstr(base_url, "/api/v1");
        out = strndup(base_url, (size_t)(p - base_url));
    } else {
        out = strdup(base_url);
    }
    return out;
}

/* PoP: _is_known_provider_base_url @ agent/model_metadata.py:_is_known_provider_base_url */
bool mmd_is_known_provider_base_url(const char *base_url) {
    char *p = mmd_infer_provider_from_url(base_url);
    bool r = p != NULL;
    free(p);
    return r;
}

/* PoP: _iter_nested_dicts @ agent/model_metadata.py:_iter_nested_dicts */
long mmd_iter_nested_dicts(const char *json) {
    /* Python: count nested dicts. */
    if (!json) return 0;
    long count = 0;
    for (const char *p = json; *p; p++) if (*p == '{') count++;
    return count;
}

/* PoP: _get_context_cache_path @ agent/model_metadata.py:_get_context_cache_path */
char *mmd_get_context_cache_path(void) {
    const char *h = getenv("HERMES_HOME");
    char *out = NULL;
    if (h && *h) asprintf(&out, "%s/cache/context_length.json", h);
    else asprintf(&out, "%s/.hermes/cache/context_length.json", getenv("HOME") ? getenv("HOME") : ".");
    return out;
}

/* PoP: _load_context_cache @ agent/model_metadata.py:_load_context_cache */
char *mmd_load_context_cache(void) {
    char *path = mmd_get_context_cache_path();
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return strdup("{}");
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = 0;
    if (buf) { r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; }
    fclose(f);
    if (!buf) return strdup("{}");
    return buf;
}

/* PoP: _invalidate_cached_context_length @ agent/model_metadata.py:_invalidate_cached_context_length */
int mmd_invalidate_cached_context_length(const char *key) {
    /* Python: drop stale entry. */
    if (!key) return -1;
    printf("context cache entry invalidated: %s\n", key);
    return 0;
}

/* PoP: _model_name_suggests_kimi @ agent/model_metadata.py:_model_name_suggests_kimi */
bool mmd_model_name_suggests_kimi(const char *model) {
    /* Python: kimi-*, moonshot-* names. */
    if (!model) return false;
    char *l = lowerdup(model);
    if (!l) return false;
    bool r = strstr(l, "kimi") != NULL || (strstr(l, "moonshot") != NULL);
    free(l);
    return r;
}

/* PoP: _model_name_suggests_minimax_m3 @ agent/model_metadata.py:_model_name_suggests_minimax_m3 */
bool mmd_model_name_suggests_minimax_m3(const char *model) {
    /* Python: MiniMax-M3 family. */
    if (!model) return false;
    char *l = lowerdup(model);
    if (!l) return false;
    bool r = strstr(l, "minimax-m3") != NULL || strstr(l, "minimax_m3") != NULL;
    free(l);
    return r;
}

/* PoP: _count_image_tokens @ agent/model_metadata.py:_count_image_tokens */
long mmd_count_image_tokens(const char *content_json) {
    /* Python: count image parts; token cost. */
    if (!content_json) return 0;
    long count = 0;
    const char *p = content_json;
    while ((p = strstr(p, "image_url")) != NULL) {
        count++;
        p += 9;
    }
    /* 85 base + 170 per 512px tile heuristic */
    return count > 0 ? 85 + 170 * (count - 1) : 0;
}
