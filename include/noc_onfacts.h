#ifndef NOC_ONFACTS_H
#define NOC_ONFACTS_H
#include <stddef.h>

/* Inputs that agent/onboarding_facts.py reads from the host, supplied by the
 * gate so the mapping/default logic is deterministic. NULL c-strings model
 * Python None where the field is Optional. */
typedef struct {
    const char *os_family;
    const char *native_arch;
    const char *machine_kind;
    const char *nvidia_platform;  /* NULL => None */
    const char *host_locale;      /* NULL => None */
    const char *facts_source;
    const char *locale;           /* NULL/"" => fallback to host_locale */
    const char **plugins;
    size_t plugin_count;
    int tours;
} onfacts_inputs_t;

typedef struct {
    char *os, *arch, *machine_kind, *nvidia_platform, *facts_source, *locale;
    char **plugins;
    size_t plugin_count;
    int tours;
} onfacts_facts_t;

/* PoP: onfacts_read_facts @ agent/onboarding_facts.py:read_facts */
void onfacts_read_facts(const onfacts_inputs_t *in, onfacts_facts_t *out);
void onfacts_facts_free(onfacts_facts_t *f);

/* PoP: onfacts_os_family @ agent/onboarding_facts.py:read_facts -- the _OS map */
const char *onfacts_os_family(const char *os_family);   /* static string */
/* PoP: onfacts_facts_source @ agent/onboarding_facts.py:read_facts */
const char *onfacts_facts_source(const char *src);      /* static string */
/* PoP: onfacts_locale @ agent/onboarding_facts.py:read_facts -- `or` fallback */
const char *onfacts_locale(const char *locale, const char *host_locale);

/* PoP: onfacts_suggested_name @ agent/onboarding_facts.py:suggested_name
 * full_name NULL models summary()["full_name"] is None. Caller frees. */
char *onfacts_suggested_name(const char *full_name);

#endif /* NOC_ONFACTS_H */
