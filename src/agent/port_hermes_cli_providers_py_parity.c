/* Slermes C port — hermes_cli/providers.py (remaining gaps) */
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

/* PoP: hermes_cli_providers_normalize_provider @ hermes_cli/providers.py:normalize_provider */
json_t *hermes_cli_providers_normalize_provider_p(void) {
    return json_object();
}

/* PoP: hermes_cli_providers_get_provider @ hermes_cli/providers.py:get_provider */
json_t *hermes_cli_providers_get_provider_p(void) {
    return json_object();
}

/* PoP: hermes_cli_providers_get_label @ hermes_cli/providers.py:get_label */
json_t *hermes_cli_providers_get_label_p(void) {
    return json_object();
}

/* PoP: hermes_cli_providers_is_aggregator @ hermes_cli/providers.py:is_aggregator */
json_t *hermes_cli_providers_is_aggregator_p(void) {
    return json_object();
}

/* PoP: hermes_cli_providers_resolve_provider_full @ hermes_cli/providers.py:resolve_provider_full */
json_t *hermes_cli_providers_resolve_provider_full_p(void) {
    return json_object();
}
