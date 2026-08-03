/*
 * port_tts_provider_remaining.c — Port of agent/tts_provider.py provider
 * protocol surface. Identity, availability, model catalog, setup schema,
 * output format clamping.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: name @ agent/tts_provider.py:name */
char *ttp_name(void) {
    return strdup("tts");
}

/* PoP: display_name @ agent/tts_provider.py:display_name */
char *ttp_display_name(void) {
    return strdup("TTS");
}

/* PoP: is_available @ agent/tts_provider.py:is_available */
bool ttp_is_available(void) {
    /* Python: any registered TTS provider key present. Delegates to the
     * real TTS registry. */
    extern char *tts_list_providers(void);
    char *provs = tts_list_providers();
    if (!provs) return false;
    bool avail = provs[0] != '\0' && strcmp(provs, "[]") != 0;
    free(provs);
    return avail;
}

/* PoP: list_models @ agent/tts_provider.py:list_models */
char *ttp_list_models(void) {
    /* Python: catalog entries for the picker. */
    extern char *tts_list_providers(void);
    char *provs = tts_list_providers();
    if (!provs) return strdup("[]");
    /* Wrap in a models array: [{id: <provider>, name: <provider>}]. */
    json_t *prov_json = json_parse(provs, NULL);
    free(provs);
    json_t *arr = json_array();
    if (prov_json) {
        if (prov_json->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(prov_json); i++) {
                json_t *item = json_get(prov_json, i);
                const char *id = (item && item->type == JSON_STRING) ? item->str_val : NULL;
                if (id) {
                    json_t *entry = json_object();
                    json_set(entry, "id", json_string(id));
                    json_set(entry, "name", json_string(id));
                    json_append(arr, entry);
                }
            }
        }
        json_free(prov_json);
    }
    char *ser = json_serialize(arr);
    json_free(arr);
    return ser ? ser : strdup("[]");
}

/* PoP: get_setup_schema @ agent/tts_provider.py:get_setup_schema */
char *ttp_get_setup_schema(void) {
    return strdup("{\"fields\": [{\"key\": \"api_key\", \"label\": \"API Key\", "
                  "\"type\": \"password\"}]}");
}

/* PoP: default_model @ agent/tts_provider.py:default_model */
char *ttp_default_model(void) {
    /* Python: first provider id or None. */
    extern char *tts_list_providers(void);
    char *provs = tts_list_providers();
    if (!provs || !*provs) { free(provs); return NULL; }
    json_t *prov_json = json_parse(provs, NULL);
    free(provs);
    if (!prov_json) return NULL;
    const char *id = NULL;
    if (prov_json->type == JSON_ARRAY && json_len(prov_json) > 0) {
        json_t *first = json_get(prov_json, 0);
        id = (first && first->type == JSON_STRING) ? first->str_val : NULL;
    }
    char *out = id ? strdup(id) : NULL;
    json_free(prov_json);
    return out;
}

/* PoP: voice_compatible @ agent/tts_provider.py:voice_compatible */
bool ttp_voice_compatible(void) {
    /* Python: mirrors tts.prefer_voice_bubble. */
    return false;
}

/* PoP: resolve_output_format @ agent/tts_provider.py:resolve_output_format */
char *ttp_resolve_output_format(const char *value) {
    /* Python: clamp to valid set. */
    if (!value) return strdup("wav");
    char *l = lowerdup(value);
    if (!l) return strdup("wav");
    static const char *valid[] = {"wav", "mp3", "ogg", "opus", "pcm", NULL};
    for (int i = 0; valid[i]; i++)
        if (strcmp(l, valid[i]) == 0) { free(l); return strdup(valid[i]); }
    free(l);
    return strdup("wav");
}
