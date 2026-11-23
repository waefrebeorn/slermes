/*
 * provider_base.c — C port of upstream Hermes agent/provider_base.py.
 *
 * PoP: w6_providerbas_bb26d7_display_name @ agent/provider_base.py:ProviderBase.display_name
 * PoP: w6_providerbas_bb26d7_setup_schema @ agent/provider_base.py:ProviderBase.get_setup_schema
 * PoP: w6_providerbas_bb26d7_catalog_display_name @ agent/provider_base.py:CatalogProviderBase.display_name
 * PoP: w6_providerbas_bb26d7_catalog_is_available @ agent/provider_base.py:CatalogProviderBase.is_available
 * PoP: w6_providerbas_bb26d7_catalog_list_models @ agent/provider_base.py:CatalogProviderBase.list_models
 * PoP: w6_providerbas_bb26d7_catalog_default_model @ agent/provider_base.py:CatalogProviderBase.default_model
 */
#include "noc_w6_providerbas_bb26d7.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

char *w6_providerbas_bb26d7_display_name(const char *name) {
    return strdup(name ? name : "");
}

json_t *w6_providerbas_bb26d7_setup_schema(const char *name) {
    json_t *o = json_object();
    json_set(o, "name", json_string(name ? name : ""));   /* display_name == name */
    json_set(o, "badge", json_string(""));
    json_set(o, "tag", json_string(""));
    json_set(o, "env_vars", json_array());
    return o;
}

/* Python str.title(): uppercase the first cased char of each word, lowercase
 * the rest; any non-alphabetic char ends the word (digits/punctuation/space). */
char *w6_providerbas_bb26d7_catalog_display_name(const char *name) {
    if (!name) return strdup("");
    size_t n = strlen(name);
    char *out = (char *)malloc(n + 1);
    int prev_cased = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)name[i];
        if (isalpha(c)) {
            out[i] = (char)(prev_cased ? tolower(c) : toupper(c));
            prev_cased = 1;
        } else {
            out[i] = (char)c;
            prev_cased = 0;
        }
    }
    out[n] = '\0';
    return out;
}

int w6_providerbas_bb26d7_catalog_is_available(void) { return 1; }

json_t *w6_providerbas_bb26d7_catalog_list_models(void) { return json_array(); }

json_t *w6_providerbas_bb26d7_catalog_default_model(const json_t *models) {
    if (!models || models->type != JSON_ARRAY || json_len(models) == 0) return json_null();
    json_t *first = json_get(models, 0);
    if (!first || first->type != JSON_OBJECT) return json_null();
    json_t *id = json_obj_get(first, "id");
    return id ? json_copy(id) : json_null();
}

