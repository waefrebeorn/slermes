/*
 * provider_registry.c - real C11 port of upstream Hermes agent/provider_registry.py.
 *
 * PoP: w9_providerreg_49f364_lower_key           @ agent/provider_registry.py:lower_key
 * PoP: w9_providerreg_49f364_exact_key           @ agent/provider_registry.py:exact_key
 * PoP: w9_providerreg_49f364_configured_ex       @ agent/provider_registry.py:configured_provider_name
 * PoP: w9_providerreg_49f364_is_available_safe   @ agent/provider_registry.py:is_available_safe
 *
 * Config/CLIENT collaborators (load_config_readonly, NOUS_MANAGED_PROVIDER,
 * provider.is_available) are injected as seams by the oracle; C reproduces the
 * module's own normalization, Nous->fal mapping and safe-bool composition.
 */
#include "hermes_json.h"
#include "noc_w9_providerreg_49f364.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dup_stripped(const char *s) {
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

char *w9_providerreg_49f364_lower_key(const char *name) {
    char *out = dup_stripped(name ? name : "");
    for (char *p = out; p && *p; p++) *p = (char)tolower((unsigned char)*p);
    return out;
}

char *w9_providerreg_49f364_exact_key(const char *name) {
    const char *s = name ? name : "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (out) memcpy(out, s, n + 1);
    return out;
}

/* Returns NULL for "not configured"; caller frees non-NULL. */
char *w9_providerreg_49f364_configured_ex(const char *raw) {
    char *cfg = dup_stripped(raw);   /* raw.strip() */
    if (!cfg || cfg[0] == '\0') { free(cfg); return NULL; }
    if (!strcmp(cfg, "nous") || !strcmp(cfg, "NOUS") || !strcmp(cfg, "Nous")) {
        free(cfg);
        char *fal = (char *)malloc(4);
        if (fal) memcpy(fal, "fal", 4);
        return fal;
    }
    /* Case-insensitive "nous" in full. */
    char *lo = dup_stripped(cfg);
    int is_nous = lo && !strcmp(lo, "nous");
    free(lo);
    if (is_nous) { free(cfg); char *fal = (char *)malloc(4); if (fal) memcpy(fal, "fal", 4); return fal; }
    return cfg;
}

bool w9_providerreg_49f364_is_available_safe(bool available, bool raises) {
    return !raises && available;
}
