/*
 * port_auth_providers.c — pure helpers from hermes_cli/auth_commands.py.
 *
 * CUSTOM_POOL_PREFIX = "custom:"; OPENROUTER_BASE_URL = "https://openrouter.ai/api/v1".
 */

#include "port_auth_providers.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define CUSTOM_POOL_PREFIX "custom:"
#define OPENROUTER_BASE_URL "https://openrouter.ai/api/v1"

static char *ap_trimdup(const char *s)
{
    const char *b, *e;
    size_t n;
    char *o;
    if (!s) return strdup("");
    b = s;
    while (*b && isspace((unsigned char)*b)) b++;
    e = b + strlen(b);
    while (e > b && isspace((unsigned char)e[-1])) e--;
    n = (size_t)(e - b);
    o = malloc(n + 1);
    if (o) { memcpy(o, b, n); o[n] = '\0'; }
    return o ? o : strdup("");
}

char *cpa_provider_base_url(const char *provider, const char *registry_base_url,
                            const char *configured_base_url, const char *custom_base_url)
{
    if (!provider)
        return strdup("");
    if (strcmp(provider, "openrouter") == 0)
        return strdup(OPENROUTER_BASE_URL);
    if (strncmp(provider, CUSTOM_POOL_PREFIX, strlen(CUSTOM_POOL_PREFIX)) == 0)
        return ap_trimdup(custom_base_url);
    if (configured_base_url && configured_base_url[0])
        return ap_trimdup(configured_base_url);
    return ap_trimdup(registry_base_url);
}

int cpa_is_known_provider(const char *provider, int in_registry, int configured)
{
    if (!provider)
        return 0;
    if (in_registry)
        return 1;
    if (strcmp(provider, "openrouter") == 0)
        return 1;
    if (strncmp(provider, CUSTOM_POOL_PREFIX, strlen(CUSTOM_POOL_PREFIX)) == 0)
        return 1;
    return configured ? 1 : 0;
}

/* ---- _resolve_custom_provider_input ---- */

/* strip + lowercase + spaces-to-dashes (matches _normalize_custom_pool_name and
 * the input normalization). Returns malloc'd. */
static char *apa_norm_token(const char *s, int dash_spaces)
{
    const char *b = s ? s : "";
    size_t len = strlen(b), cap, j, i;
    char *o;
    while (len > 0 && isspace((unsigned char)b[0])) { b++; len--; }
    while (len > 0 && isspace((unsigned char)b[len - 1])) len--;
    cap = len + 1;
    o = malloc(cap);
    if (!o) return NULL;
    for (i = 0, j = 0; i < len; i++) {
        unsigned char c = (unsigned char)b[i];
        if (c == ' ' && dash_spaces) c = '-';
        o[j++] = (char)tolower(c);
    }
    o[j] = '\0';
    return o;
}

char *apa_resolve_custom_provider_input(const char *raw, const char *entries_json,
                                        const char *cp_prefix)
{
    const char *prefix = cp_prefix ? cp_prefix : "custom:";
    char *norm = apa_norm_token(raw, 1); /* strip/lower/space->dash */
    json_t *arr = NULL;
    char *best = NULL;

    if (!norm) return NULL;
    if (norm[0] == '\0') { free(norm); return NULL; }
    if (strncmp(norm, prefix, strlen(prefix)) == 0)
        return norm; /* normalized custom: id, owned copy */

    arr = entries_json ? json_parse(entries_json, NULL) : NULL;
    if (arr && arr->type == JSON_ARRAY) {
        for (size_t e = 0; e < arr->c.count && !best; e++) {
            const json_t *ent = arr->c.items[e];
            if (!ent || ent->type != JSON_OBJECT) continue;
            const json_t *pk = json_obj_get(ent, "provider_key");
            const json_t *nm = json_obj_get(ent, "name");
            const json_t *pl = json_obj_get(ent, "pool_key");
            char *pk_low = apa_norm_token(pk && pk->type == JSON_STRING ? pk->str_val : "", 0);
            char *nm_norm = apa_norm_token(nm && nm->type == JSON_STRING ? nm->str_val : "", 1);
            if (!pk_low || !nm_norm) { free(pk_low); free(nm_norm); continue; }
            if (pk_low[0] && strcmp(pk_low, norm) == 0) {
                best = strdup(pk_low);
            } else if (strcmp(nm_norm, norm) == 0) {
                if (pk_low[0])
                    best = strdup(pk_low);
                else
                    best = strdup(pl && pl->type == JSON_STRING && pl->str_val ? pl->str_val : "");
            }
            free(pk_low);
            free(nm_norm);
        }
    }
    if (arr) json_free(arr);
    free(norm);
    return best;
}
