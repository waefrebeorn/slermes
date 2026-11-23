/*
 * PoP: w10_modelsdete_a35568_current_provider_catalog_match @ hermes_cli/models_detect.py:current_provider_catalog_match
 * PoP: w10_modelsdete_a35568_current_provider_owns_vendor   @ hermes_cli/models_detect.py:current_provider_owns_vendor
 */
#include "hermes_json.h"
#include "noc_w10_modelsdete_a35568.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int is_skip(const char *s) {
    return !s || !*s || !strcmp(s, "auto") || !strcmp(s, "openrouter") || !strcmp(s, "custom");
}

/* Python (value or "").strip().lower() — ASCII whitespace is enough for CLI ids. */
static char *strip_lower(const char *s) {
    if (!s) s = "";
    const char *a = s, *b = s + strlen(s);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)b[-1])) b--;
    char *out = (char *)malloc((size_t)(b - a) + 1);
    if (!out) return NULL;
    size_t i = 0;
    for (const char *p = a; p < b; p++, i++) out[i] = (char)tolower((unsigned char)*p);
    out[i] = 0;
    return out;
}

static char *dup_lower(const char *s) {
    char *out = (char *)malloc(strlen(s) + 1);
    if (!out) return NULL;
    size_t i = 0;
    for (const char *p = s; *p; p++, i++) out[i] = (char)tolower((unsigned char)*p);
    out[i] = 0;
    return out;
}

json_t *w10_modelsdete_a35568_current_provider_catalog_match(
        const char *model_name, const char *current_provider,
        const char *normalized_provider, int catalog_error, json_t *catalog) {
    char *provider = strip_lower(current_provider);
    if (!provider) return json_null();
    int skip = is_skip(provider) || !strncmp(provider, "custom:", 7)
            || (normalized_provider && is_skip(normalized_provider));
    char *wanted = strip_lower(model_name);
    if (skip || !wanted || !*wanted) { free(provider); free(wanted); return json_null(); }
    if (catalog_error || !catalog || catalog->type != JSON_ARRAY) {
        free(provider); free(wanted); return json_null();
    }
    json_t *found = NULL;
    /* exact id: mid.lower() == wanted */
    for (size_t i = 0; i < json_array_size(catalog) && !found; i++) {
        json_t *mid = json_array_get(catalog, i);
        if (!mid || mid->type != JSON_STRING || !mid->str_val) continue;
        char *low = dup_lower(mid->str_val);
        if (low && !strcmp(low, wanted)) found = mid;
        free(low);
    }
    /* bare part after vendor/: mid.split("/",1)[1].lower() == wanted */
    if (!found) {
        for (size_t i = 0; i < json_array_size(catalog) && !found; i++) {
            json_t *mid = json_array_get(catalog, i);
            if (!mid || mid->type != JSON_STRING || !mid->str_val) continue;
            const char *slash = strchr(mid->str_val, '/');
            if (!slash || !slash[1]) continue;
            char *low = dup_lower(slash + 1);
            if (low && !strcmp(low, wanted)) found = mid;
            free(low);
        }
    }
    json_t *res = found ? json_copy(found) : json_null();
    free(provider); free(wanted);
    return res;
}

int w10_modelsdete_a35568_current_provider_owns_vendor(
        const char *model_name, const char *current_provider,
        const char *normalized_provider, int is_aggregator,
        const char *vendor_of_model, json_t *native_vendors) {
    char *provider = strip_lower(current_provider);
    if (!provider) return 0;
    int skip = is_skip(provider) || !strncmp(provider, "custom:", 7)
            || (normalized_provider && is_skip(normalized_provider)) || is_aggregator;
    free(provider);
    if (skip || !vendor_of_model || !*vendor_of_model) return 0;
    if (!native_vendors || native_vendors->type != JSON_ARRAY) return 0;
    size_t n = json_array_size(native_vendors);
    if (n == 0) return 0;  /* set() != {vendor} */
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_array_get(native_vendors, i);
        if (!v || v->type != JSON_STRING || !v->str_val || strcmp(v->str_val, vendor_of_model))
            return 0;
    }
    return 1;
}
