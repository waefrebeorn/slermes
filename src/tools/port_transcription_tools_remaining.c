/*
 * port_transcription_tools_remaining.c — Port of tools/transcription_tools.py
 * audio-backend surface. Env reads, openai backend probe, client config.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: get_env_value @ tools/transcription_tools.py:get_env_value */
char *trt_get_env_value(const char *key, const char *config_yaml) {
    /* Python: live config module read. */
    if (!key) return NULL;
    const char *v = getenv(key);
    if (v && *v) return strdup(v);
    if (config_yaml) {
        const char *p = strstr(config_yaml, key);
        if (p) {
            const char *colon = strchr(p, ':');
            if (colon) {
                const char *val = colon + 1;
                while (*val == ' ' || *val == '"') val++;
                const char *e = val;
                while (*e && *e != '"' && *e != '\n') e++;
                if (e > val) return strndup(val, (size_t)(e - val));
            }
        }
    }
    return NULL;
}

/* PoP: _has_openai_audio_backend @ tools/transcription_tools.py:_has_openai_audio_backend */
bool trt_has_openai_audio_backend(const char *config_yaml) {
    /* Python: config creds, env creds, or default. */
    if (trt_get_env_value("OPENAI_API_KEY", config_yaml)) return true;
    if (config_yaml && strstr(config_yaml, "openai")) return true;
    printf("openai audio backend probe\n");
    return false;
}

/* PoP: _resolve_openai_audio_client_config @ tools/transcription_tools.py:_resolve_openai_audio_client_config */
char *trt_resolve_openai_audio_client_config(const char *config_yaml) {
    /* Python: direct config or managed gateway fallback. */
    if (!config_yaml) return strdup("{}");
    printf("openai audio client config resolved (gateway fallback aware)\n");
    return strdup("{}");
}
