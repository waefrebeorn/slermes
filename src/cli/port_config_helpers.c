/*
 * port_config_helpers.c
 *
 * Pure, portable helper functions ported from hermes_cli/config.py.
 * These contain no file I/O, no networking, and no os.* calls — only
 * string/value coercion and small data-normalization logic. Heavy
 * nested-dict manipulation (deep_merge, _strip_dotted_keys, etc.) stays
 * a genuine REAL_GAP; this file ports the self-contained primitives.
 *
 * Functions:
 *   coerce_ssl_verify            <- _coerce_ssl_verify
 *   coerce_config_version        <- _coerce_config_version
 *   reject_denylisted_env_var    <- _reject_denylisted_env_var
 *   normalize_custom_provider_entry_json <- _normalize_custom_provider_entry
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>

/* ===========================================================================
 *  _coerce_ssl_verify
 * =========================================================================== */

/*
 * PoP: _coerce_ssl_verify @ hermes_cli/config.py:_coerce_ssl_verify
 * Coerce a value to an SSL-verify tri-state. Returns 1 (true), 0 (false),
 * or -1 (unset/None). Mirrors Python: None -> None, bool passthrough,
 * string {"false","0","no","off"} -> false, {"true","1","yes","on"} -> true,
 * anything else -> None. */
int coerce_ssl_verify(const char *value, int is_bool, int bool_val)
{
    if (value == NULL && !is_bool) return -1;
    if (is_bool) return bool_val ? 1 : 0;
    char buf[256];
    size_t L = strlen(value);
    if (L >= sizeof(buf)) L = sizeof(buf) - 1;
    memcpy(buf, value, L); buf[L] = '\0';
    /* strip + lowercase */
    char *p = buf;
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t')) p[--n] = '\0';
    for (size_t i = 0; p[i]; i++) p[i] = (char)tolower((unsigned char)p[i]);
    if (strcmp(p, "false") == 0 || strcmp(p, "0") == 0 ||
        strcmp(p, "no") == 0 || strcmp(p, "off") == 0) return 0;
    if (strcmp(p, "true") == 0 || strcmp(p, "1") == 0 ||
        strcmp(p, "yes") == 0 || strcmp(p, "on") == 0) return 1;
    return -1;
}

/* ===========================================================================
 *  _coerce_config_version
 * =========================================================================== */

/*
 * PoP: _coerce_config_version @ hermes_cli/config.py:_coerce_config_version
 * Return a safe integer config version; invalid/None -> 0, bool -> 0,
 * negative clamped to 0. */
int coerce_config_version(const char *value, int is_bool)
{
    if (is_bool) return 0;
    if (value == NULL) return 0;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') return 0;
    if (v < 0) return 0;
    return (int)v;
}

/* ===========================================================================
 *  _reject_denylisted_env_var
 * =========================================================================== */

static const char *ENV_DENYLIST[] = {
    /* Loader / linker */
    "LD_PRELOAD", "LD_LIBRARY_PATH", "LD_AUDIT", "LD_DEBUG",
    "DYLD_INSERT_LIBRARIES", "DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH",
    "DYLD_FALLBACK_LIBRARY_PATH", "DYLD_FALLBACK_FRAMEWORK_PATH",
    /* Python */
    "PYTHONPATH", "PYTHONHOME", "PYTHONSTARTUP", "PYTHONUSERBASE",
    "PYTHONEXECUTABLE", "PYTHONNOUSERSITE",
    /* Node */
    "NODE_OPTIONS", "NODE_PATH",
    /* General */
    "PATH", "SHELL", "BROWSER", "EDITOR", "VISUAL", "PAGER",
    /* Git */
    "GIT_SSH_COMMAND", "GIT_EXEC_PATH", "GIT_SHELL",
    /* Hermes runtime location */
    "HERMES_HOME", "HERMES_PROFILE", "HERMES_CONFIG", "HERMES_ENV",
    NULL
};

/*
 * PoP: _reject_denylisted_env_var @ hermes_cli/config.py:_reject_denylisted_env_var
 * Returns 0 if key is allowed; returns -1 (and fills err) if key is on the
 * writer denylist (names that influence subprocess execution or the Hermes
 * runtime location). */
int reject_denylisted_env_var(const char *key, char *err, size_t errsz)
{
    if (err) err[0] = '\0';
    if (!key) return -1;
    char buf[256];
    size_t L = strlen(key);
    if (L >= sizeof(buf)) L = sizeof(buf) - 1;
    memcpy(buf, key, L); buf[L] = '\0';
    for (size_t i = 0; buf[i]; i++) buf[i] = (char)toupper((unsigned char)buf[i]);
    for (int i = 0; ENV_DENYLIST[i]; i++) {
        if (strcmp(buf, ENV_DENYLIST[i]) == 0) {
            if (err) snprintf(err, errsz,
                "Environment variable '%s' is on the writer denylist. "
                "Names that influence subprocess execution (LD_PRELOAD, "
                "PYTHONPATH, PATH, EDITOR, ...) or Hermes runtime location "
                "(HERMES_HOME, HERMES_PROFILE, ...) cannot be persisted via "
                "the env writer. If you really need this, edit "
                "~/.hermes/.env directly.", key);
            return -1;
        }
    }
    return 0;
}

/* ===========================================================================
 *  _normalize_custom_provider_entry
 *  Returns a malloc'd JSON object string (the normalized entry), or NULL
 *  when the entry is not a dict / has no usable base_url / no name.
 *  Caller frees. Mirrors the Python normalizer's field extraction.
 * =========================================================================== */

/* Minimal inline URL sanity check: requires scheme and host (no spaces). */
static int url_has_scheme_and_host(const char *u)
{
    const char *p = u;
    /* scheme */
    if (!isalpha((unsigned char)*p)) return 0;
    while (*p && (isalnum((unsigned char)*p) || *p == '+' || *p == '-' || *p == '.')) p++;
    if (*p != ':') return 0;
    p++;
    if (strncmp(p, "//", 2) != 0) return 0;
    p += 2;
    /* host: at least one non-space char */
    if (!*p || *p == ' ') return 0;
    for (; *p && *p != ' ' && *p != '/'; p++) { /* scan host */ }
    return 1;
}

/* Recognize ${VAR} or {region}-style template placeholders. */
static int has_template_placeholder(const char *s)
{
    for (const char *p = s; *p; p++) {
        if (*p == '{') {
            const char *q = p + 1;
            while (*q && *q != '}') q++;
            if (*q == '}') return 1;
        }
        if (*p == '$' && p[1] == '{') return 1;
    }
    return 0;
}

/*
 * PoP: _normalize_custom_provider_entry @ hermes_cli/config.py:_normalize_custom_provider_entry
 * Accepts a JSON object (the raw entry) plus an optional provider_key.
 * Performs the camelCase alias mapping, strips unknown keys (best-effort),
 * validates base_url (allowing unresolved template placeholders), and emits
 * the normalized entry as a malloc'd JSON string. Returns NULL if the entry
 * is not an object, has no valid base_url, or no name. Caller frees. */
char *normalize_custom_provider_entry_json(const char *entry_json, const char *provider_key)
{
    if (!entry_json) return NULL;
    json_t *entry = json_parse(entry_json, NULL);
    if (!entry || entry->type != JSON_OBJECT) {
        if (entry) json_free(entry);
        return NULL;
    }

    /* camelCase -> snake_case alias map */
    const char *CAMEL[] = {
        "apiKey", "api_key", "baseUrl", "base_url", "apiMode", "api_mode",
        "keyEnv", "key_env", "apiKeyEnv", "key_env", "defaultModel", "default_model",
        "contextLength", "context_length", "rateLimitDelay", "rate_limit_delay", NULL
    };
    for (int i = 0; CAMEL[i]; i += 2) {
        json_t *v = json_object_get(entry, CAMEL[i]);
        if (v && !json_object_get(entry, CAMEL[i+1])) {
            json_object_set(entry, CAMEL[i+1], v);
        }
    }
    /* api_key_env alias for key_env */
    {
        json_t *ake = json_object_get(entry, "api_key_env");
        if (ake && !json_object_get(entry, "key_env")) {
            json_object_set(entry, "key_env", ake);
        }
    }

    /* base_url resolution: prefer base_url/url/api; accept template placeholders
     * without URL validation, otherwise require scheme+host. */
    const char *url_keys[] = {"base_url", "url", "api"};
    const char *base_url = NULL;
    for (int i = 0; i < 3; i++) {
        json_t *v = json_object_get(entry, url_keys[i]);
        if (v && v->type == JSON_STRING) {
            const char *cand = json_string_value(v);
            if (cand && cand[0]) {
                if (has_template_placeholder(cand) || url_has_scheme_and_host(cand)) {
                    base_url = cand;
                    break;
                }
            }
        }
    }
    if (!base_url) { json_free(entry); return NULL; }

    /* name */
    const char *name = NULL;
    json_t *rn = json_object_get(entry, "name");
    if (rn && rn->type == JSON_STRING && json_string_value(rn)[0]) {
        name = json_string_value(rn);
    } else if (provider_key && provider_key[0]) {
        name = provider_key;
    }
    if (!name || !name[0]) { json_free(entry); return NULL; }

    /* Build normalized object */
    json_t *out = json_object();
    json_object_set(out, "name", json_string(name));
    json_object_set(out, "base_url", json_string(base_url));
    if (provider_key && provider_key[0]) {
        json_object_set(out, "provider_key", json_string(provider_key));
    }
    const char *copy_str[] = {"api_key", "key_env", "api_mode", "model",
                              "ssl_ca_cert", "ssl_verify"};
    /* api_mode also from transport */
    json_t *am = json_object_get(entry, "api_mode");
    if ((!am || am->type != JSON_STRING) ) am = json_object_get(entry, "transport");
    if (am && am->type == JSON_STRING && json_string_value(am)[0]) {
        json_object_set(out, "api_mode", am);
    }
    for (int i = 0; i < 6; i++) {
        json_t *v = json_object_get(entry, copy_str[i]);
        if (v && v->type == JSON_STRING && json_string_value(v)[0]) {
            json_object_set(out, copy_str[i], v);
        }
    }
    /* model also from default_model */
    json_t *mn = json_object_get(entry, "model");
    if ((!mn || mn->type != JSON_STRING) ) mn = json_object_get(entry, "default_model");
    if (mn && mn->type == JSON_STRING && json_string_value(mn)[0]) {
        json_object_set(out, "model", mn);
    }
    /* context_length: positive int */
    json_t *cl = json_object_get(entry, "context_length");
    if (cl && cl->type == JSON_NUMBER && json_number_value(cl) > 0) {
        json_object_set(out, "context_length", cl);
    }
    /* rate_limit_delay: non-negative number */
    json_t *rd = json_object_get(entry, "rate_limit_delay");
    if (rd && (rd->type == JSON_NUMBER) && json_number_value(rd) >= 0) {
        json_object_set(out, "rate_limit_delay", rd);
    }
    /* discover_models: bool */
    json_t *dm = json_object_get(entry, "discover_models");
    if (dm && dm->type == JSON_BOOL) {
        json_object_set(out, "discover_models", dm);
    }
    /* extra_body: dict */
    json_t *eb = json_object_get(entry, "extra_body");
    if (eb && eb->type == JSON_OBJECT) {
        json_object_set(out, "extra_body", eb);
    }
    /* models: dict or list -> dict shape */
    json_t *models = json_object_get(entry, "models");
    if (models && models->type == JSON_OBJECT && json_object_size(models) > 0) {
        json_object_set(out, "models", models);
    } else if (models && models->type == JSON_ARRAY) {
        json_t *md = json_object();
        for (size_t i = 0; i < json_array_size(models); i++) {
            json_t *m = json_array_get(models, i);
            if (m && m->type == JSON_STRING && json_string_value(m)[0]) {
                json_object_set(md, json_string_value(m), json_object());
            }
        }
        if (json_object_size(md) > 0) json_object_set(out, "models", md);
        json_free(md);
    }

    char *result = json_dumps(out, 0);
    json_free(out);
    json_free(entry);
    return result;
}
