/*
 * port_auth_helpers.c
 *
 * Pure, portable helper functions ported from hermes_cli/auth.py.
 * Most auth.py pure helpers (has_usable_secret, format_auth_error,
 * _coerce_ttl_seconds, _is_expiring, _optional_base_url, *_pkce_*,
 * _resolve_*_base_url, etc.) are already implemented in port_auth_na.c, so
 * this file ports only the ones NOT yet present, avoiding link collisions.
 * No network, no auth.json load (callers pass the store JSON in).
 *
 * Module prefix used by the scanner for hermes_cli/auth.py is "auth_".
 *
 * C name <- python name (auth_ prefix):
 *   auth_xai_callback_cors_origin, auth_ssh_user_at_host,
 *   auth_is_terminal_nous_refresh_error, auth_is_source_suppressed
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>

/* ---- lowercase helper ---- */
static void lc(char *s)
{
    for (; *s; s++) if (isupper((unsigned char)*s)) *s = (char)tolower((unsigned char)*s);
}

/* ---------------------------------------------------------------------- */
/* PoP: auth_xai_callback_cors_origin @ hermes_cli/auth.py:_xai_callback_cors_origin
 * Returns malloc'd allowed origin, or "" (empty) when not allowed. */
char *auth_xai_callback_cors_origin(const char *origin)
{
    if (!origin) return strdup("");
    if (strcmp(origin, "https://accounts.x.ai") == 0) return strdup(origin);
    if (strcmp(origin, "https://auth.x.ai") == 0) return strdup(origin);
    return strdup("");
}

/* ---------------------------------------------------------------------- */
/* PoP: auth_ssh_user_at_host @ hermes_cli/auth.py:_ssh_user_at_host
 * Returns "user@hostname" using USER/LOGNAME env + gethostname. */
char *auth_ssh_user_at_host(void)
{
    char host[256];
    if (gethostname(host, sizeof(host)) != 0) host[0] = '\0';
    if (!host[0]) strcpy(host, "<this-host>");
    const char *user = getenv("USER");
    if (!user) user = getenv("LOGNAME");
    if (!user) user = "<user>";
    char *out = malloc(strlen(user) + strlen(host) + 2);
    sprintf(out, "%s@%s", user, host);
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: auth_is_terminal_nous_refresh_error @ hermes_cli/auth.py:_is_terminal_nous_refresh_error
 * provider/code/relogin passed explicitly (no AuthError object). */
int auth_is_terminal_nous_refresh_error(const char *provider, const char *code, int relogin_required)
{
    if (!provider || !code) return 0;
    char p[64]; strncpy(p, provider, sizeof(p)-1); p[sizeof(p)-1]='\0'; lc(p);
    if (strcmp(p, "nous") != 0) return 0;
    if (!relogin_required) return 0;
    char c[64]; strncpy(c, code, sizeof(c)-1); c[sizeof(c)-1]='\0'; lc(c);
    if (strcmp(c, "invalid_grant")==0 || strcmp(c,"invalid_token")==0 || strcmp(c,"refresh_token_reused")==0)
        return 1;
    return 0;
}

/* ---------------------------------------------------------------------- */
/* PoP: auth_is_source_suppressed @ hermes_cli/auth.py:is_source_suppressed
 * Takes the auth.json "suppressed_sources" object as JSON (caller loads it).
 * Returns 1 if (provider_id -> list) contains source. */
int auth_is_source_suppressed(const char *provider_id, const char *source, const char *suppressed_json)
{
    if (!provider_id || !source || !suppressed_json) return 0;
    json_t *root = json_parse(suppressed_json, NULL);
    if (!root || root->type != JSON_OBJECT) { if (root) json_free(root); return 0; }
    json_t *prov = json_object_get(root, provider_id);
    if (!prov || prov->type != JSON_ARRAY) { json_free(root); return 0; }
    int found = 0;
    for (size_t i = 0; i < json_array_size(prov); i++) {
        json_t *e = json_array_get(prov, i);
        if (e && e->type == JSON_STRING && strcmp(json_string_value(e), source) == 0) { found = 1; break; }
    }
    json_free(root);
    return found;
}
