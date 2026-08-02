/*
 * port_nous_subscription_remaining.c — Port of hermes_cli/nous_subscription.py
 * feature-surface. Feature accessors, ordered items, model config dict.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static bool feat(const char *features_json, const char *key) {
    if (!features_json) return false;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(features_json, needle);
    if (!p) return false;
    const char *colon = strchr(p, ':');
    if (!colon) return false;
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t') v++;
    return strncmp(v, "true", 4) == 0 || *v == '1';
}

/* PoP: web @ hermes_cli/nous_subscription.py:web */
bool nsub_web(const char *features_json) {
    return feat(features_json, "web");
}

/* PoP: tts @ hermes_cli/nous_subscription.py:tts */
bool nsub_tts(const char *features_json) {
    return feat(features_json, "tts");
}

/* PoP: browser @ hermes_cli/nous_subscription.py:browser */
bool nsub_browser(const char *features_json) {
    return feat(features_json, "browser");
}

/* PoP: modal @ hermes_cli/nous_subscription.py:modal */
bool nsub_modal(const char *features_json) {
    return feat(features_json, "modal");
}

/* PoP: items @ hermes_cli/nous_subscription.py:items */
char *nsub_items(void) {
    /* Python: ordered feature list. */
    return strdup("[\"web\", \"image_gen\", \"video_gen\", \"tts\", \"stt\", \"browser\", \"modal\"]");
}

/* PoP: _model_config_dict @ hermes_cli/nous_subscription.py:_model_config_dict */
char *nsub_model_config_dict(const char *config_json) {
    /* Python: model dict or string → dict. */
    if (!config_json) return strdup("{}");
    if (config_json[0] == '{') return strdup(config_json);
    char *out = NULL;
    asprintf(&out, "{\"default\": \"%s\"}", config_json);
    return out ? out : strdup("{}");
}
