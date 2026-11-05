/*
 * port_auth_providers.c — pure helpers from hermes_cli/auth_commands.py.
 *
 * CUSTOM_POOL_PREFIX = "custom:"; OPENROUTER_BASE_URL = "https://openrouter.ai/api/v1".
 */

#include "port_auth_providers.h"

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
