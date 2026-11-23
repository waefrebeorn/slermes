/*
 * auxiliary_health.c -- C home for upstream Hermes agent/auxiliary_health.py
 * (wave8 gate w8_auxiliaryhe_a7faf2).
 *
 * PoP: w8_auxiliaryhe_a7faf2_quarantine_ttl @ agent/auxiliary_health.py:fallback_candidate_quarantine_ttl
 * PoP: w8_auxiliaryhe_a7faf2_custom_health_base_url @ agent/auxiliary_health.py:_custom_health_base_url
 *
 * All registry/label/env lookups are injected as plain values, so the pure
 * decision + string-strip logic is reproduced without importing the runtime.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_auxiliaryhe_a7faf2.h"

json_t *w8_auxiliaryhe_a7faf2_quarantine_ttl(const char *reason) {
    if (reason && reason[0] &&
        (!strcmp(reason, "rate limit") ||
         !strcmp(reason, "connection error") ||
         !strcmp(reason, "invalid provider response") ||
         !strcmp(reason, "provider overloaded") ||
         !strcmp(reason, "server error")))
        return json_number(60.0);
    return json_null();
}

/* Python str.strip() over ASCII whitespace; caller frees. */
static char *w8_aux_strip(const char *s) {
    if (!s) return NULL;
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

json_t *w8_auxiliaryhe_a7faf2_custom_health_base_url(
    const char *provider, const char *explicit, const char *label,
    const char *current_custom, int resolves, const json_t *named_entry) {
    (void)provider;
    char *explicit_s = w8_aux_strip(explicit);
    const char *e = explicit_s ? explicit_s : "";
    json_t *result;

    if (label && !strcmp(label, "local/custom")) {
        result = json_string(e[0] ? e : (current_custom ? current_custom : ""));
    } else if (label && !strncmp(label, "custom:", 7) && e[0]) {
        result = json_string(e);
    } else if (resolves) {
        result = json_string(e[0] ? e : (current_custom ? current_custom : ""));
    } else if (named_entry && named_entry->type == JSON_OBJECT && named_entry->c.count > 0) {
        json_t *bv = json_obj_get(named_entry, "base_url");
        const char *raw = (bv && bv->type == JSON_STRING && bv->str_val) ? bv->str_val : "";
        char *stripped = w8_aux_strip(raw);
        const char *chosen = e[0] ? e : (stripped ? stripped : "");
        result = json_string(chosen);
        free(stripped);
    } else {
        result = json_string("");
    }

    free(explicit_s);
    return result;
}
