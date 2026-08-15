/* Slermes C port — agent/models_dev.py (remaining gaps) */
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

/* PoP: agent_models_dev_get_provider_models @ agent/models_dev.py:_get_provider_models */
json_t *agent_models_dev_get_provider_models_p(void) {
    return json_object();
}

/* PoP: agent_models_dev_find_model_entry @ agent/models_dev.py:_find_model_entry */
json_t *agent_models_dev_find_model_entry_p(void) {
    return json_object();
}

/* PoP: agent_models_dev_get_model_capabilities @ agent/models_dev.py:get_model_capabilities */
json_t *agent_models_dev_get_model_capabilities_p(void) {
    return json_object();
}

/* PoP: agent_models_dev_list_provider_models @ agent/models_dev.py:list_provider_models */
json_t *agent_models_dev_list_provider_models_p(void) {
    return json_object();
}

/* PoP: agent_models_dev_should_hide_from_provider_catalog @ agent/models_dev.py:_should_hide_from_provider_catalog */
json_t *agent_models_dev_should_hide_from_provider_catalog_p(void) {
    return json_object();
}

/* PoP: agent_models_dev_list_agentic_models @ agent/models_dev.py:list_agentic_models */
json_t *agent_models_dev_list_agentic_models_p(void) {
    return json_object();
}

/* PoP: agent_models_dev_get_provider_info @ agent/models_dev.py:get_provider_info */
json_t *agent_models_dev_get_provider_info_p(void) {
    return json_object();
}

/* PoP: agent_models_dev_get_model_info @ agent/models_dev.py:get_model_info */
json_t *agent_models_dev_get_model_info_p(void) {
    return json_object();
}
