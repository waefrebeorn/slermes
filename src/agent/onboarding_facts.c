/*
 * onboarding_facts.c — C port of upstream Hermes agent/onboarding_facts.py.
 *
 * read_facts() gathers host data then applies pure transformations:
 *   os            = _OS.get(machine["os_family"], "linux")   # darwin/win32 map
 *   facts_source  = "remote" if facts_source == "remote" else "local"
 *   locale        = locale or host["locale"]
 * everything else is passed through. suggested_name() is summary()["full_name"].
 * The gate injects the gathered values, so only this deterministic logic is
 * compared (host measurement is environment state, not portable behaviour).
 *
 * PoP: onfacts_read_facts @ agent/onboarding_facts.py:read_facts
 * PoP: onfacts_os_family @ agent/onboarding_facts.py:read_facts
 * PoP: onfacts_facts_source @ agent/onboarding_facts.py:read_facts
 * PoP: onfacts_locale @ agent/onboarding_facts.py:read_facts
 * PoP: onfacts_suggested_name @ agent/onboarding_facts.py:suggested_name
 */
#include "noc_onfacts.h"
#include <stdlib.h>
#include <string.h>

/* _OS = {"darwin": "macos", "win32": "windows"} ... .get(..., "linux") */
const char *onfacts_os_family(const char *os_family) {
    if (!os_family) return "linux";
    if (!strcmp(os_family, "darwin")) return "macos";
    if (!strcmp(os_family, "win32")) return "windows";
    return "linux";
}

const char *onfacts_facts_source(const char *src) {
    return (src && !strcmp(src, "remote")) ? "remote" : "local";
}

/* Python `locale or host_locale` -- falsy strings (""), and None, fall back. */
const char *onfacts_locale(const char *locale, const char *host_locale) {
    return (locale && locale[0]) ? locale : host_locale;
}

/* PoP: onfacts_read_facts @ agent/onboarding_facts.py:read_facts */
void onfacts_read_facts(const onfacts_inputs_t *in, onfacts_facts_t *out) {
    memset(out, 0, sizeof(*out));
    if (!in) return;
    out->os = strdup(onfacts_os_family(in->os_family));
    out->arch = strdup(in->native_arch ? in->native_arch : "");
    out->machine_kind = strdup(in->machine_kind ? in->machine_kind : "computer");
    out->nvidia_platform = in->nvidia_platform ? strdup(in->nvidia_platform) : NULL;
    out->facts_source = strdup(onfacts_facts_source(in->facts_source));
    const char *loc = onfacts_locale(in->locale, in->host_locale);
    out->locale = loc ? strdup(loc) : NULL;
    out->plugin_count = in->plugin_count;
    out->plugins = (char **)calloc(in->plugin_count ? in->plugin_count : 1, sizeof(char *));
    for (size_t i = 0; i < in->plugin_count; i++)
        out->plugins[i] = strdup(in->plugins[i] ? in->plugins[i] : "");
    out->tours = in->tours ? 1 : 0;
}

void onfacts_facts_free(onfacts_facts_t *f) {
    if (!f) return;
    free(f->os); free(f->arch); free(f->machine_kind);
    free(f->nvidia_platform); free(f->facts_source); free(f->locale);
    for (size_t i = 0; i < f->plugin_count; i++) free(f->plugins[i]);
    free(f->plugins);
    memset(f, 0, sizeof(*f));
}

/* PoP: onfacts_suggested_name @ agent/onboarding_facts.py:suggested_name */
char *onfacts_suggested_name(const char *full_name) {
    return full_name ? strdup(full_name) : NULL;
}
