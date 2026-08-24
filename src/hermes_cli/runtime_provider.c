/**
 * port_runtime_provider.c — Port of Python: cli.py (runtime provider helpers)
 *
 * Real C implementations for runtime provider functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include "port_provider_registry.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: getenv */
char *getenv_fn(const char *name, const char *default_val)
{
    if (!name) {
        hermes_log(LOG_WARNING, "port", "getenv_fn: null name");
        return default_val ? strdup(default_val) : NULL;
    }
    const char *val = getenv(name);
    if (val) {
        hermes_log(LOG_DEBUG, "port", "getenv_fn: %s=%s", name, val);
        return strdup(val);
    }
    hermes_log(LOG_DEBUG, "port", "getenv_fn: %s not set, using default", name);
    return default_val ? strdup(default_val) : NULL;
}

/* Port of Python: _getenv */
char *_getenv(void *ctx, void *name, void *default_val)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_getenv: null context");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "port", "_getenv: called");
    if (name) {
        hermes_log(LOG_DEBUG, "port", "_getenv: name is set");
    }
    if (default_val) {
        hermes_log(LOG_DEBUG, "port", "_getenv: default_val is set");
    }
    return getenv_fn((const char *)name, (const char *)default_val);
}

/* Port of Python: canonical_custom_identity.
 * Real implementation: delegates to the config-backed provider registry, which
 * reverse-looks-up base_url → custom:<name>, then the configured provider when
 * it names a real ``providers:``/``custom_providers:`` entry. (The model-based
 * recovery source is available via provider_canonical_custom_identity's 3rd
 * arg; this 2-arg ABI passes model=NULL, matching the original call sites.) */
char *canonical_custom_identity(const char *base_url, const char *config_provider)
{
    return provider_canonical_custom_identity(base_url, config_provider, NULL);
}

/* PoP: _canonical_custom_identity @ hermes_cli/runtime_provider.py:canonical_custom_identity */
char *_canonical_custom_identity(void *ctx, void *base_url, void *config_provider)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "canonical_custom_identity: null context");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "port", "canonical_custom_identity: called");
    return canonical_custom_identity(
        base_url ? (const char *)base_url : NULL,
        config_provider ? (const char *)config_provider : NULL);
}

/* ── runtime_provider.py resolution surface ───────────────────────────── */

#include "slermes_home.h"
#include "yaml.h"
#include <ctype.h>
#include "auth_helpers.h"

/* config.yaml → json_t (owned by caller; free with json_free). Same load
 * shape as the provider registry's config-backed scans. */
static json_t *rp_load_config(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/config.yaml", slermes_home());
    char *yerr = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &yerr);
    if (yerr) free(yerr);
    if (!doc) return NULL;
    char *js = yaml_to_json_string(doc, "");
    yaml_free(doc);
    if (!js) return NULL;
    char *jerr = NULL;
    json_t *cfg = json_parse(js, &jerr);
    free(js);
    if (jerr) free(jerr);
    return cfg;
}

/* strip + lowercase into a malloc'd string ("" for NULL). */
static char *rp_strip_lower(const char *s) {
    if (!s) return strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++)
        out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

/* PoP: resolve_requested_provider @ hermes_cli/runtime_provider.py:resolve_requested_provider
 * Resolve the provider request from explicit arg → config model.provider →
 * HERMES_INFERENCE_PROVIDER env → "auto". The persisted config selection is
 * deliberately preferred over any stale shell/.env override so chat uses the
 * endpoint the user last saved. Returns malloc'd lowercase id; caller frees. */
char *runtime_provider_resolve_requested(const char *requested) {
    /* 1. explicit argument wins */
    char *req = rp_strip_lower(requested);
    if (req && *req) return req;
    free(req);

    /* 2. config model.provider */
    json_t *cfg = rp_load_config();
    if (cfg) {
        const json_t *model_cfg = json_obj_get(cfg, "model");
        if (model_cfg && json_is_object(model_cfg)) {
            json_t *p = json_obj_get(model_cfg, "provider");
            if (p && json_is_string(p)) {
                char *cp = rp_strip_lower(json_string_value(p));
                if (cp && *cp) { json_free(cfg); return cp; }
                free(cp);
            }
        }
        json_free(cfg);
    }

    /* 3. env override (profile-scoped getenv in Python; plain env here) */
    char *env_p = rp_strip_lower(getenv("HERMES_INFERENCE_PROVIDER"));
    if (env_p && *env_p) return env_p;
    free(env_p);

    return strdup("auto");
}

/* PoP: format_runtime_provider_error @ hermes_cli/runtime_provider.py:format_runtime_provider_error
 * AuthError instances route through auth.py:format_auth_error; everything else
 * formats via str(error). In C the exception type is an explicit pointer:
 * non-NULL `error` means AuthError; otherwise `fallback` is str(error).
 * Returns malloc'd message; caller frees. */
char *runtime_provider_format_error(const auth_error_t *error,
                                    const char *fallback) {
    if (error) return auth_format_error(error);
    return strdup(fallback ? fallback : "");
}
