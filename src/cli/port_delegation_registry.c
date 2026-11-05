/*
 * port_delegation_registry.c — pure helpers from agent/delegation_registry.py.
 */

#include "port_delegation_registry.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool dr_list_has(const char *list, const char *name)
{
    const char *p = list ? list : "";
    size_t nlen = strlen(name);
    while (*p) {
        while (*p && (isspace((unsigned char)*p) || *p == ','))
            p++;
        if (!*p)
            break;
        const char *tok = p;
        while (*p && !isspace((unsigned char)*p) && *p != ',')
            p++;
        size_t tlen = (size_t)(p - tok);
        if (tlen == nlen && strncmp(tok, name, nlen) == 0)
            return true;
    }
    return false;
}

bool dr_bundled_disabled(const char *name, const char *disabled_list)
{
    char q[80];
    size_t n = name ? strlen(name) : 0;
    if (!name || n == 0)
        return false;
    if (n > sizeof(q) - 11)
        return false;
    memcpy(q, "delegation/", 11);
    memcpy(q + 11, name, n + 1); /* prefix + name + NUL */
    if (dr_list_has(disabled_list, name))
        return true;
    return dr_list_has(disabled_list, q);
}

char *dr_backend_status_reason(const char *name, bool available,
                               bool registered)
{
    static const char DEFAULT_DISABLED[] =
        "No default delegation strategy is available: the bundled plugin "
        "delegation/default is disabled. Remove it from plugins.disabled in "
        "config.yaml.";
    static const char NOT_INSTALLED[] =
        "Delegation backend '%s' is not installed: no enabled plugin provides "
        "it. Install or enable the plugin that does (`hermes plugins list`), "
        "or switch back to the default backend (`hermes config set "
        "delegation.backend default`).";
    static const char UNAVAILABLE[] =
        "Delegation backend '%s' is unavailable here (its plugin reports it "
        "cannot run). Fix its setup, or switch back to the default backend: "
        "`/delegation default` for this session, or `hermes config set "
        "delegation.backend default`.";
    const char *nm = name ? name : "";
    char buf[512];
    size_t len;
    char *out;

    if (available)
        return strdup("");
    if (!registered && strcmp(nm, "default") == 0) {
        len = sizeof(DEFAULT_DISABLED) - 1;
        memcpy(buf, DEFAULT_DISABLED, len);
    } else if (!registered) {
        len = (size_t)snprintf(buf, sizeof(buf), NOT_INSTALLED, nm);
    } else {
        len = (size_t)snprintf(buf, sizeof(buf), UNAVAILABLE, nm);
    }
    out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, buf, len);
    out[len] = '\0';
    return out;
}
