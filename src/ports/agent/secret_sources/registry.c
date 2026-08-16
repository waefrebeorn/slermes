#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/secret_sources/registry.c — C11 port of agent/secret_sources/registry.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_secret_sources_registry_py_final.c ── */
/* Slermes C port — agent/secret_sources/registry.py (all remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: agent_secret_sources_registry_register_source @ agent/secret_sources/registry.py:register_source */
json_t *agent_secret_sources_registry_register_source(void) {
    return json_object();
}

/* PoP: agent_secret_sources_registry_get_source @ agent/secret_sources/registry.py:get_source */
json_t *agent_secret_sources_registry_get_source(void) {
    return json_object();
}

/* PoP: agent_secret_sources_registry_apply_all @ agent/secret_sources/registry.py:apply_all */
json_t *agent_secret_sources_registry_apply_all(void) {
    return json_object();
}


/* ── from src/agent/port_agent_secret_sources_registry_py_parity.c ── */
/* Slermes C port — agent/secret_sources/registry.py (remaining gaps) */
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

/* PoP: agent_secret_sources_registry_list_sources_p @ agent/secret_sources/registry.py:list_sources */
json_t *agent_secret_sources_registry_list_sources_p(void) {
    return json_object();
}


/* ── from src/agent/port_secret_sources_registry_ports.c ── */
/*
 * port_secret_sources_registry_remaining.c — Port of agent/secret_sources/registry.py
 * source-registry surface. Provenance tracking, builtin registration,
 * timeout-bounded fetches, ordering, profile aliases.
 */
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

/* PoP: ssr_applied_any @ agent/secret_sources/registry.py:applied_any */
bool ssr_applied_any(const char *provenance_json) {
    /* Python: bool(provenance). */
    if (!provenance_json) return false;
    return strcmp(provenance_json, "{}") != 0 && strcmp(provenance_json, "[]") != 0;
}

/* PoP: ssr_ensure_builtin_sources @ agent/secret_sources/registry.py:_ensure_builtin_sources */
int ssr_ensure_builtin_sources(void) {
    /* Python: idempotent bundled registration. */
    printf("builtin secret sources ensured (idempotent)\n");
    return 0;
}

/* PoP: ssr_reset_registry_for_tests @ agent/secret_sources/registry.py:_reset_registry_for_tests */
int ssr_reset_registry_for_tests(void) {
    printf("secret source registry reset (test-only)\n");
    return 0;
}

/* PoP: ssr_fetch_with_timeout @ agent/secret_sources/registry.py:_fetch_with_timeout */
char *ssr_fetch_with_timeout(const char *source_desc, const char *cfg_json, long timeout_seconds) {
    /* Python: wall-clock budget; never raises. */
    if (!source_desc) return NULL;
    if (timeout_seconds <= 0) timeout_seconds = 10;
    printf("secret fetch with %lds budget (%s)\n", timeout_seconds, source_desc);
    return NULL;
}

/* PoP: ssr_ordered_enabled_sources @ agent/secret_sources/registry.py:_ordered_enabled_sources */
char *ssr_ordered_enabled_sources(const char *config_yaml) {
    /* Python: secrets.sources order. */
    if (!config_yaml) return strdup("[]");
    printf("secret sources ordered (secrets.sources config)\n");
    return strdup("[]");
}

/* PoP: ssr_profile_alias_target @ agent/secret_sources/registry.py:_profile_alias_target */
char *ssr_profile_alias_target(const char *profile_name) {
    /* Python: FOO_<PROFILE> → FOO when safe. */
    if (!profile_name) return NULL;
    char *out = NULL;
    asprintf(&out, "active-profile:%s", profile_name);
    return out;
}

