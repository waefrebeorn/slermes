/*
 * port_agent_provider.c — pure helpers from agent/provider_registry.py.
 *
 * provider_registry.py:lower_key -> name.strip().lower()
 * provider_registry.py:configured_provider_name (config-read half):
 *   raw.strip() content -> "fal" when lowered == NOUS_MANAGED_PROVIDER ("nous"),
 *   else the stripped value; NULL when no non-whitespace content.
 */

#include "port_agent_provider.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *ap_strdup(const char *s, size_t n)
{
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* str(raw or "").strip() lowercased into a malloc'd buffer; returns the token
 * run. caller frees. */
static char *ap_normalized(const char *raw)
{
    const char *p = raw ? raw : "";
    size_t len = strlen(p), s = 0, e = len;
    while (s < len && isspace((unsigned char)p[s]))
        s++;
    while (e > s && isspace((unsigned char)p[e - 1]))
        e--;
    return ap_strdup(p + s, e - s);
}

char *ap_lower_key(const char *name)
{
    char *tok = ap_normalized(name);
    size_t i;
    if (!tok) return NULL;
    for (i = 0; tok[i]; i++)
        tok[i] = (char)tolower((unsigned char)tok[i]);
    return tok;
}

char *ap_configured_provider_name(const char *provider_value)
{
    char *tok = ap_normalized(provider_value);
    size_t i;
    if (!tok) return NULL;
    if (tok[0] == '\0') { /* no non-whitespace content -> None */
        free(tok);
        return NULL;
    }
    /* configured.lower() == "nous" -> "fal". */
    for (i = 0; tok[i]; i++)
        tok[i] = (char)tolower((unsigned char)tok[i]);
    if (strcmp(tok, "nous") == 0) {
        free(tok);
        return ap_strdup("fal", 3);
    }
    return tok;
}
