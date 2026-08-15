/* Slermes C11 port — hermes_cli/providers.py */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: normalize_provider_xf @ hermes_cli/providers.py:normalize_provider */
static const char *normalize_provider_xf(const char * name) {
    return NULL;
}

/* PoP: get_provider @ hermes_cli/providers.py:get_provider */
static json_t *get_provider(const char * name, bool allow_network) {
    return json_object();
}

/* PoP: get_label @ hermes_cli/providers.py:get_label */
static const char *get_label(const char * provider_id) {
    return NULL;
}

/* PoP: is_aggregator_xf @ hermes_cli/providers.py:is_aggregator */
static bool is_aggregator_xf(const char * provider) {
    return false;
}

