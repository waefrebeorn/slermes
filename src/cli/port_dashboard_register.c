/*
 * port_dashboard_register.c
 *
 * Pure helpers ported from hermes_cli/dashboard_register.py - registering a
 * self-hosted dashboard OAuth client with Nous Portal.
 *
 * The module's real work is urllib calls, .env persistence and random name
 * generation. What is portable is URL resolution:
 *
 *   dr_resolve_portal_base_url <- dashboard_register.py:_resolve_portal_base_url
 *   dr_public_url_from_redirect <- dashboard_register.py:_public_url_from_redirect
 *
 * The auth-state lookup becomes a parameter, because a C body should not own
 * the credential store.
 */

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* ===========================================================================
 *  _public_url_from_redirect  (dashboard_register.py:_public_url_from_redirect)
 *
 *      parsed = urlparse(redirect_uri or "")
 *      if parsed.scheme in ("http", "https") and parsed.netloc:
 *          return f"{parsed.scheme}://{parsed.netloc}"
 *      return ""
 *
 *  The runtime appends /auth/callback to HERMES_DASHBOARD_PUBLIC_URL, so the
 *  raw URI would double the path - only the origin is wanted. Returns a NEW
 *  string the caller frees ("" included).
 * =========================================================================== */

/* PoP: _public_url_from_redirect @ hermes_cli/dashboard_register.py:_public_url_from_redirect */
char *dr_public_url_from_redirect(const char *redirect_uri)
{
    const char *s = (redirect_uri && redirect_uri[0]) ? redirect_uri : "";

    /* scheme: [A-Za-z][A-Za-z0-9+.-]* up to ':' */
    const char *colon = strchr(s, ':');
    if (!colon || colon == s) return strdup("");

    size_t slen = (size_t)(colon - s);
    if (!((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= 'A' && s[0] <= 'Z')))
        return strdup("");
    for (size_t i = 1; i < slen; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.'))
            return strdup("");
    }

    char scheme[16];
    if (slen >= sizeof scheme) return strdup("");
    memcpy(scheme, s, slen);
    scheme[slen] = '\0';
    if (strcasecmp(scheme, "http") != 0 && strcasecmp(scheme, "https") != 0)
        return strdup("");

    /* netloc: up to the first '/', '?' or '#' */
    const char *host = colon + 1;
    while (*host == '/') host++;               /* skip the "//" */
    const char *end = host;
    while (*end && *end != '/' && *end != '?' && *end != '#') end++;
    if (end == host) return strdup("");

    size_t hlen = (size_t)(end - host);
    /* IPv6 literal: keep the brackets, they are part of netloc */
    size_t need = slen + 3 + hlen + 1;
    char *out = (char *)malloc(need);
    if (!out) return NULL;
    memcpy(out, scheme, slen);
    out[slen] = ':'; out[slen + 1] = '/'; out[slen + 2] = '/';
    memcpy(out + slen + 3, host, hlen);
    out[slen + 3 + hlen] = '\0';
    return out;
}

/* ===========================================================================
 *  _resolve_portal_base_url  (dashboard_register.py:_resolve_portal_base_url)
 *
 *      if isinstance(override, str) and override.strip():
 *          return override.rstrip("/")
 *      base = <stored auth state portal_base_url>
 *      chosen = base if isinstance(base, str) and base.strip() else DEFAULT
 *      return chosen.rstrip("/")
 *
 *  `stored_base` stands in for get_provider_auth_state("nous") and
 *  `default_base` for DEFAULT_NOUS_PORTAL_URL. rstrip("/") removes EVERY
 *  trailing slash, not just one. Returns a NEW string the caller frees.
 * =========================================================================== */

/* PoP: _resolve_portal_base_url @ hermes_cli/dashboard_register.py:_resolve_portal_base_url */
char *dr_resolve_portal_base_url(const char *override,
                                 const char *stored_base,
                                 const char *default_base)
{
    const char *chosen = NULL;

    if (override) {
        const char *p = override;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p) chosen = override;
    }
    if (!chosen) {
        if (stored_base) {
            const char *p = stored_base;
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p) chosen = stored_base;
        }
    }
    if (!chosen) chosen = (default_base && default_base[0]) ? default_base : "";

    /* rstrip("/") */
    size_t len = strlen(chosen);
    while (len > 0 && chosen[len - 1] == '/') len--;

    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, chosen, len);
    out[len] = '\0';
    return out;
}
