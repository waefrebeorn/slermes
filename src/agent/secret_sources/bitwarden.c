#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"

/* PoP: agent_secret_sources_bitwarden_u_cache_key_str
 * @ agent/secret_sources/bitwarden.py:_cache_key_str */
/* "|".join(cache_key) over a JSON array of strings. */
char *agent_secret_sources_bitwarden_u_cache_key_str(const char *arg) {
    json_t *a = arg ? json_parse(arg, NULL) : NULL;
    if (!a || !json_is_array(a)) { if (a) json_free(a); return strdup(""); }
    size_t cap = 64, w = 0; char *out = malloc(cap); out[0] = 0;
    for (size_t i = 0; i < a->c.count; i++) {
        json_t *it = a->c.items[i];
        const char *s = (it && json_is_string(it) && it->str_val) ? it->str_val : "";
        size_t need = w + strlen(s) + 2;
        if (need > cap) { cap = need * 2; out = realloc(out, cap); }
        if (i) out[w++] = '|';
        memcpy(out + w, s, strlen(s)); w += strlen(s); out[w] = 0;
    }
    json_free(a);
    return out;
}
