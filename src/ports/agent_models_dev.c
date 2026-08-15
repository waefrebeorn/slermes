/* Slermes C11 port — agent/models_dev.py */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: get_model_capabilities @ agent/models_dev.py:get_model_capabilities */
json_t *get_model_capabilities(const char * provider, const char * model) {
    return json_object();
}

/* PoP: list_provider_models @ agent/models_dev.py:list_provider_models */
json_t *list_provider_models(const char * provider) {
    return json_object();
}

/* PoP: _should_hide_from_provider_catalog @ agent/models_dev.py:_should_hide_from_provider_catalog */
bool _should_hide_from_provider_catalog(const char * provider, const char * model_id) {
    return false;
}

/* PoP: list_agentic_models @ agent/models_dev.py:list_agentic_models */
json_t *list_agentic_models(const char * provider) {
    return json_object();
}

