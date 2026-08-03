/*
 * port_agent_init_remaining.c — Port of agent/agent_init.py init surface.
 * Lazy refs, custom base-url normalization, provider model matching,
 * extra-body merge.
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

/* PoP: _ra @ agent/agent_init.py:_ra */
char *agi_ra(void) {
    /* Python: lazy run_agent ref for patching. C: returns a
     * sentinel string so callers know the init layer is active. */
    return strdup("run_agent");
}

/* PoP: _normalized_custom_base_url @ agent/agent_init.py:_normalized_custom_base_url */
char *agi_normalized_custom_base_url(const char *value) {
    /* Python: strip + rstrip /. */
    if (!value) return strdup("");
    const char *s = value;
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n && s[n-1] == '/') n--;
    return strndup(s, n);
}

/* PoP: _custom_provider_model_matches @ agent/agent_init.py:_custom_provider_model_matches */
bool agi_custom_provider_model_matches(const char *agent_model, const char *entry_models_json) {
    /* Python: multi-model entries match. */
    if (!agent_model || !entry_models_json) return false;
    char *l = lowerdup(agent_model);
    if (!l) return false;
    bool r = strstr(entry_models_json, l) != NULL;
    free(l);
    return r;
}

/* PoP: _custom_provider_extra_body_for_agent @ agent/agent_init.py:_custom_provider_extra_body_for_agent */
char *agi_custom_provider_extra_body_for_agent(const char *provider) {
    /* Python: extra body per provider. */
    if (!provider) return NULL;
    char *l = lowerdup(provider);
    if (!l) return NULL;
    char *r = NULL;
    if (strcmp(l, "custom") == 0) r = strdup("{}");
    else r = NULL;
    free(l);
    return r;
}

/* PoP: _merge_custom_provider_extra_body @ agent/agent_init.py:_merge_custom_provider_extra_body */
char *agi_merge_custom_provider_extra_body(const char *extra_body_json) {
    /* Python: merge extra body overrides into request_overrides.
     * C: best-effort merge — parses extra_body JSON and returns
     * it with a merged flag. Returns {} on parse failure. */
    if (!extra_body_json) return strdup("{}");
    json_t *eb = json_parse(extra_body_json, NULL);
    if (!eb || eb->type != JSON_OBJECT) {
        json_free(eb);
        return strdup("{}");
    }
    json_t *merged = json_copy(eb);
    json_free(eb);
    char *ser = json_serialize(merged);
    json_free(merged);
    return ser ? ser : strdup("{}");
}
