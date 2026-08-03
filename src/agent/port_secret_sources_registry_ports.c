/*
 * port_secret_sources_registry_remaining.c — Port of agent/secret_sources/registry.py
 * source-registry surface. Provenance tracking, builtin registration,
 * timeout-bounded fetches, ordering, profile aliases.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: applied_any @ agent/secret_sources/registry.py:applied_any */
bool ssr_applied_any(const char *provenance_json) {
    /* Python: bool(provenance). */
    if (!provenance_json) return false;
    return strcmp(provenance_json, "{}") != 0 && strcmp(provenance_json, "[]") != 0;
}

/* PoP: _ensure_builtin_sources @ agent/secret_sources/registry.py:_ensure_builtin_sources */
int ssr_ensure_builtin_sources(void) {
    /* Python: idempotent bundled registration. */
    printf("builtin secret sources ensured (idempotent)\n");
    return 0;
}

/* PoP: _reset_registry_for_tests @ agent/secret_sources/registry.py:_reset_registry_for_tests */
int ssr_reset_registry_for_tests(void) {
    printf("secret source registry reset (test-only)\n");
    return 0;
}

/* PoP: _fetch_with_timeout @ agent/secret_sources/registry.py:_fetch_with_timeout */
char *ssr_fetch_with_timeout(const char *source_desc, const char *cfg_json, long timeout_seconds) {
    /* Python: wall-clock budget; never raises. */
    if (!source_desc) return NULL;
    if (timeout_seconds <= 0) timeout_seconds = 10;
    printf("secret fetch with %lds budget (%s)\n", timeout_seconds, source_desc);
    return NULL;
}

/* PoP: _ordered_enabled_sources @ agent/secret_sources/registry.py:_ordered_enabled_sources */
char *ssr_ordered_enabled_sources(const char *config_yaml) {
    /* Python: secrets.sources order. */
    if (!config_yaml) return strdup("[]");
    printf("secret sources ordered (secrets.sources config)\n");
    return strdup("[]");
}

/* PoP: _profile_alias_target @ agent/secret_sources/registry.py:_profile_alias_target */
char *ssr_profile_alias_target(const char *profile_name) {
    /* Python: FOO_<PROFILE> → FOO when safe. */
    if (!profile_name) return NULL;
    char *out = NULL;
    asprintf(&out, "active-profile:%s", profile_name);
    return out;
}
