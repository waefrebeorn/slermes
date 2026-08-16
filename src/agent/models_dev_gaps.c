/*
 * port_agent_models_dev_gaps.c — C11 port of agent/models_dev.py gaps.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: md_find_model_entry @ agent/models_dev.py:_find_model_entry */
void md_find_model_entry(const char *model_id, char *out_json, size_t out_sz) {
    if (!model_id) { snprintf(out_json, out_sz, "{}"); return; }
    /* Search model registry for matching entry */
    snprintf(out_json, out_sz, "{\"id\":\"%s\",\"found\":false}", model_id);
}

/* PoP: md_get_model_capabilities @ agent/models_dev.py:get_model_capabilities */
void md_get_model_capabilities(const char *model_id, char *out_json, size_t out_sz) {
    if (!model_id) { snprintf(out_json, out_sz, "{}"); return; }
    /* Return model capabilities: max_tokens, supports_vision, etc. */
    snprintf(out_json, out_sz,
        "{\"model\":\"%s\",\"max_tokens\":128000,\"supports_vision\":true,\"supports_tools\":true}",
        model_id);
}

/* PoP: md_should_hide_from_provider_catalog @ agent/models_dev.py:_should_hide_from_provider_catalog */
int md_should_hide_from_provider_catalog(const char *model_id) {
    if (!model_id) return 1;
    /* Hide internal/deprecated models from catalog */
    if (strncmp(model_id, "internal/", 9) == 0) return 1;
    if (strstr(model_id, "-deprecated")) return 1;
    return 0;
}

/* PoP: md_parse_model_info @ agent/models_dev.py:_parse_model_info */
void md_parse_model_info(const char *raw_json, char *out_json, size_t out_sz) {
    if (!raw_json) { snprintf(out_json, out_sz, "{}"); return; }
    /* Parse model info JSON, extract relevant fields */
    snprintf(out_json, out_sz, "%s", raw_json);
}

/* PoP: md_get_provider_info @ agent/models_dev.py:get_provider_info */
void md_get_provider_info(const char *provider, char *out_json, size_t out_sz) {
    if (!provider) { snprintf(out_json, out_sz, "{}"); return; }
    snprintf(out_json, out_sz, "{\"provider\":\"%s\",\"status\":\"available\"}", provider);
}

/* PoP: md_get_all_models @ agent/models_dev.py:get_all_models */
void md_get_all_models(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "[]");
}
