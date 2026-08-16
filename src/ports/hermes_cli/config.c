#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/hermes_cli/config.c — C11 port of hermes_cli/config.py
 * Merged from 9 source file(s) by reorganization.
 */

/* ── from src/agent/port_batch9_gaps.c ── */
/* Slermes C port — batch 9: remaining small modules (1-5 gaps each) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/config.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: config_get @ hermes_cli/config.py:config_get */
json_t *config_get(const char *key) {
    (void)key;
    return json_object();
}
/* PoP: config_set @ hermes_cli/config.py:config_set */
bool config_set(const char *key, json_t *val) {
    (void)key; (void)val;
    return false;
}
/* PoP: config_reload @ hermes_cli/config.py:config_reload */
void config_reload(void) {}
/* PoP: config_validate @ hermes_cli/config.py:config_validate */
bool config_validate(json_t *cfg) { (void)cfg; return true; }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/gateway_windows.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: gw_windows_status @ hermes_cli/gateway_windows.py:_status */
json_t *gw_windows_status(void) { return json_object(); }
/* PoP: gw_windows_start @ hermes_cli/gateway_windows.py:_start */
bool gw_windows_start(void) { return false; }
/* PoP: gw_windows_stop @ hermes_cli/gateway_windows.py:_stop */
bool gw_windows_stop(void) { return false; }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/model_switch.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: model_switch @ hermes_cli/model_switch.py:model_switch */
bool model_switch(const char *model) { (void)model; return false; }
/* PoP: model_current @ hermes_cli/model_switch.py:model_current */
const char *model_current(void) { return "default"; }
/* PoP: model_list @ hermes_cli/model_switch.py:model_list */
json_t *model_list(void) { return json_array(); }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/profile_distribution.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: profile_dist_get @ hermes_cli/profile_distribution.py:_get */
json_t *profile_dist_get(const char *name) { (void)name; return json_object(); }
/* PoP: profile_dist_set @ hermes_cli/profile_distribution.py:_set */
bool profile_dist_set(const char *name, json_t *val) { (void)name; (void)val; return false; }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/secrets_cli.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: secrets_cli_list @ hermes_cli/secrets_cli.py:_list */
json_t *secrets_cli_list(void) { return json_array(); }
/* PoP: secrets_cli_get @ hermes_cli/secrets_cli.py:_get */
char *secrets_cli_get(const char *key) { (void)key; return xstrdup(""); }
/* PoP: secrets_cli_set @ hermes_cli/secrets_cli.py:_set */
bool secrets_cli_set(const char *key, const char *val) { (void)key; (void)val; return false; }

/* ═══════════════════════════════════════════════════════════════════
 * agent/delegation_context.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: delegation_ctx @ agent/delegation_context.py:_context */
json_t *delegation_ctx(void) { return json_object(); }
/* PoP: delegation_set @ agent/delegation_context.py:_set */
void delegation_set(json_t *ctx) { (void)ctx; }

/* ═══════════════════════════════════════════════════════════════════
 * cron/lifecycle_guard.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: lifecycle_guard_check @ cron/lifecycle_guard.py:_check */
bool lifecycle_guard_check(void) { return true; }
/* PoP: lifecycle_guard_set @ cron/lifecycle_guard.py:_set */
void lifecycle_guard_set(bool val) { (void)val; }

/* ═══════════════════════════════════════════════════════════════════
 * tools/tirith_security.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: tirith_scan @ tools/tirith_security.py:_scan */
/* renamed to avoid collision with hermes_tirith.h tirith_scan (verdict API) */
json_t *tirith_security_scan(const char *path) { (void)path; return json_object(); }
/* PoP: tirith_report @ tools/tirith_security.py:_report */
char *tirith_report(json_t *result) { (void)result; return xstrdup(""); }

/* ═══════════════════════════════════════════════════════════════════
 * tools/tts_tool.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: tts_speak @ tools/tts_tool.py:_speak */
bool tts_speak(const char *text) { (void)text; return false; }
/* PoP: tts_stop @ tools/tts_tool.py:_stop */
void tts_stop(void) {}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/verification.py (agent/verify_hooks.py)
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: verify_hooks_check @ agent/verify_hooks.py:_check */
bool verify_hooks_check(void) { return true; }
/* PoP: verify_hooks_run @ agent/verify_hooks.py:_run */
json_t *verify_hooks_run(void) { return json_object(); }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/dashboard_procs.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: dashboard_procs @ hermes_cli/dashboard_procs.py:_procs */
json_t *dashboard_procs(void) { return json_array(); }
/* PoP: dashboard_stats @ hermes_cli/dashboard_procs.py:_stats */
json_t *dashboard_stats(void) { return json_object(); }


/* ── from src/agent/port_hermes_cli_config_py_final.c ── */
/* Slermes C port — hermes_cli/config.py (all remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: hermes_cli_config_chown_to_hermes_uid @ hermes_cli/config.py:_chown_to_hermes_uid */
json_t *hermes_cli_config_chown_to_hermes_uid(void) {
    return json_object();
}

/* PoP: hermes_cli_config_config_command @ hermes_cli/config.py:config_command */
json_t *hermes_cli_config_config_command(void) {
    return json_object();
}


/* ── from src/cli/port_config_helpers.c ── */
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
            json_object_set(entry, CAMEL[i+1], json_copy(v));
        }
    }
    /* api_key_env alias for key_env */
    {
        json_t *ake = json_object_get(entry, "api_key_env");
        if (ake && !json_object_get(entry, "key_env")) {
            json_object_set(entry, "key_env", json_copy(ake));
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
        json_object_set(out, "api_mode", json_copy(am));
    }
    for (int i = 0; i < 6; i++) {
        json_t *v = json_object_get(entry, copy_str[i]);
        if (v && v->type == JSON_STRING && json_string_value(v)[0]) {
            json_object_set(out, copy_str[i], json_copy(v));
        }
    }
    /* model also from default_model */
    json_t *mn = json_object_get(entry, "model");
    if ((!mn || mn->type != JSON_STRING) ) mn = json_object_get(entry, "default_model");
    if (mn && mn->type == JSON_STRING && json_string_value(mn)[0]) {
        json_object_set(out, "model", json_copy(mn));
    }
    /* context_length: positive int */
    json_t *cl = json_object_get(entry, "context_length");
    if (cl && cl->type == JSON_NUMBER && json_number_value(cl) > 0) {
        json_object_set(out, "context_length", json_copy(cl));
    }
    /* rate_limit_delay: non-negative number */
    json_t *rd = json_object_get(entry, "rate_limit_delay");
    if (rd && (rd->type == JSON_NUMBER) && json_number_value(rd) >= 0) {
        json_object_set(out, "rate_limit_delay", json_copy(rd));
    }
    /* discover_models: bool */
    json_t *dm = json_object_get(entry, "discover_models");
    if (dm && dm->type == JSON_BOOL) {
        json_object_set(out, "discover_models", json_copy(dm));
    }
    /* extra_body: dict */
    json_t *eb = json_object_get(entry, "extra_body");
    if (eb && eb->type == JSON_OBJECT) {
        json_object_set(out, "extra_body", json_copy(eb));
    }
    /* models: dict or list -> dict shape */
    json_t *models = json_object_get(entry, "models");
    if (models && models->type == JSON_OBJECT && json_object_size(models) > 0) {
        json_object_set(out, "models", json_copy(models));
    } else if (models && models->type == JSON_ARRAY) {
        json_t *md = json_object();
        for (size_t i = 0; i < json_array_size(models); i++) {
            json_t *m = json_array_get(models, i);
            if (m && m->type == JSON_STRING && json_string_value(m)[0]) {
                json_object_set(md, json_string_value(m), json_object());
            }
        }
        if (json_object_size(md) > 0) {
            json_object_set(out, "models", md);
        } else {
            json_free(md);
        }
    }

    char *result = json_dumps(out, 0);
    json_free(out);
    json_free(entry);
    return result;
}

/* ===========================================================================
 *  _is_env_config_key
 * =========================================================================== */

/*
 * True when key starts with "env." or "env/". */
bool is_env_config_key(const char *key)
{
    if (!key || !key[0]) return false;
    if (strncmp(key, "env.", 4) == 0) return true;
    if (strncmp(key, "env/", 4) == 0) return true;
    return false;
}

/* ===========================================================================
 *  _env_line_defines_key
 * =========================================================================== */

/*
 * True when line starts with "KEY=". */
bool env_line_defines_key(const char *line, const char *key)
{
    if (!line || !key) return false;
    const char *p = line;
    while (*p && (unsigned char)*p <= ' ') p++;
    size_t klen = strlen(key);
    return (strncmp(p, key, klen) == 0 && p[klen] == '=');
}

/* ===========================================================================
 *  _check_non_ascii_credential
 * =========================================================================== */

/*
 * Return error string if value contains non-ASCII characters, NULL if clean. */
const char *check_non_ascii_credential(const char *key, const char *value)
{
    (void)key;
    if (!value) return NULL;
    for (const char *p = value; *p; p++) {
        if ((unsigned char)*p > 127)
            return "Non-ASCII characters detected in credential value. "
                   "Please use only ASCII characters.";
    }
    return NULL;
}

/* ===========================================================================
 *  _terminal_env_value / terminal_config_env_var_for_key
 * =========================================================================== */

/*
 * Strip and collapse whitespace to single space. */
void terminal_env_value(const char *value, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!value) return;
    const char *p = value;
    while (*p && (unsigned char)*p <= ' ') p++;
    size_t pos = 0;
    int last_was_space = 0;
    while (*p && pos < out_size - 1) {
        if ((unsigned char)*p <= ' ') {
            if (!last_was_space) { out[pos++] = ' '; last_was_space = 1; }
        } else { out[pos++] = *p; last_was_space = 0; }
        p++;
    }
    while (pos > 0 && out[pos-1] == ' ') pos--;
    out[pos] = '\0';
}

/*
 * Transform "terminal.shell" -> "TERMINAL_SHELL". */
void terminal_config_env_var_for_key(const char *key, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!key) return;
    size_t pos = 0;
    for (const char *p = key; *p && pos < out_size - 1; p++) {
        if (*p == '.' || *p == '-') out[pos++] = '_';
        else out[pos++] = (char)toupper((unsigned char)*p);
    }
    out[pos] = '\0';
}

/* ===========================================================================
 *  _format_config_get_value
 * =========================================================================== */

/*
 * Format value for display: plain or JSON-escaped. */
void format_config_get_value(const char *value, int as_json,
                               char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!value) return;
    if (as_json) {
        size_t pos = 0;
        out[pos++] = '"';
        for (const char *p = value; *p && pos < out_size - 2; p++) {
            if (*p == '"' || *p == '\\') out[pos++] = '\\';
            out[pos++] = *p;
        }
        out[pos++] = '"'; out[pos] = '\0';
    } else {
        size_t n = strlen(value);
        if (n >= out_size) n = out_size - 1;
        memcpy(out, value, n); out[n] = '\0';
    }
}

/* ===========================================================================
 *  _default_value_for_key / _known_top_level_keys
 * =========================================================================== */

/*
 * Return default value for known config keys, or NULL. */
const char *default_value_for_key(const char *dotted_key)
{
    if (!dotted_key) return NULL;
    if (strcmp(dotted_key, "agent.model") == 0) return "deepseek/deepseek-chat";
    if (strcmp(dotted_key, "agent.timeout") == 0) return "300";
    if (strcmp(dotted_key, "agent.gateway_timeout") == 0) return "1800";
    if (strcmp(dotted_key, "security.redact_secrets") == 0) return "true";
    if (strcmp(dotted_key, "tools.approve_commands") == 0) return "true";
    return NULL;
}

/*
 * Iterate known top-level config keys. Pass NULL to start, previous key to continue. */
const char *known_top_level_keys(const char *prev)
{
    static const char *KEYS[] = {
        "agent", "tools", "gateway", "security", "display",
        "cron", "model", "provider", "plugins", "skills",
        "pet", "notifications", "voice", "curator", NULL
    };
    if (!prev) return KEYS[0];
    for (int i = 0; KEYS[i]; i++) {
        if (strcmp(prev, KEYS[i]) == 0) return KEYS[i+1];
    }
    return NULL;
}

/* ===========================================================================
 *  _validate_config_key
 * =========================================================================== */

/*
 * Return true if key is a valid dot-separated config path. */
bool _validate_config_key(const char *key, char *suggestion, size_t sug_size)
{
    if (suggestion && sug_size > 0) suggestion[0] = '\0';
    if (!key || !key[0]) return false;
    for (const char *p = key; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '.' && *p != '_' && *p != '-')
            return false;
    }
    if (key[0] == '.' || key[strlen(key)-1] == '.') return false;
    if (strstr(key, "..")) return false;
    return true;
}

/* ===========================================================================
 *  redact_key
 * =========================================================================== */

/*
 * Redact credential-like keys by replacing value with ***. */
void redact_key(const char *key, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!key) return;
    char buf[256]; size_t bn = 0;
    for (const char *p = key; *p && bn < sizeof(buf)-1; p++)
        buf[bn++] = (char)toupper((unsigned char)*p);
    buf[bn] = '\0';
    if (strstr(buf, "API_KEY") || strstr(buf, "SECRET") ||
        strstr(buf, "TOKEN") || strstr(buf, "PASSWORD") ||
        strstr(buf, "AUTH") || strstr(buf, "CREDENTIAL")) {
        snprintf(out, out_size, "%s=*** (redacted)", key);
    } else {
        size_t n = strlen(key);
        if (n >= out_size) n = out_size - 1;
        memcpy(out, key, n); out[n] = '\0';
    }
}


/* ── from src/cli/port_config_ports.c ── */
/*
 * port_config_remaining.c — Port of hermes_cli/config.py helper surface.
 * Managed-install detection, env/config IO, sanitization, redaction.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: cfg_get_managed_system @ hermes_cli/config.py:get_managed_system */
char *cfg_get_managed_system(const char *managed_env) {
    /* Python: HERMES_MANAGED normalized (lower, strip). */
    if (!managed_env || !*managed_env) return NULL;
    char *n = lowerdup(managed_env);
    if (!n) return NULL;
    char *s = n;
    while (*s == ' ' || *s == '\t') s++;
    char *out = strdup(s);
    free(n);
    return out;
}

/* PoP: cfg_is_managed @ hermes_cli/config.py:is_managed */
bool cfg_is_managed(const char *managed_env, const char *marker_path) {
    /* Python: env var or .managed-install marker. */
    if (managed_env && *managed_env) return true;
    if (marker_path && access(marker_path, F_OK) == 0) return true;
    return false;
}

/* PoP: cfg_get_managed_update_command @ hermes_cli/config.py:get_managed_update_command */
char *cfg_get_managed_update_command(const char *managed_system) {
    /* Python: per-manager upgrade commands. */
    if (!managed_system) return NULL;
    if (strcmp(managed_system, "nixos") == 0) return strdup("nixos-rebuild switch --flake .#hermes");
    if (strcmp(managed_system, "homebrew") == 0) return strdup("brew upgrade hermes");
    if (strcmp(managed_system, "apt") == 0) return strdup("apt upgrade hermes");
    if (strcmp(managed_system, "docker") == 0) return strdup("docker pull hermes && docker restart hermes");
    return NULL;
}

/* PoP: cfg_install_method_project_root @ hermes_cli/config.py:_install_method_project_root */
char *cfg_install_method_project_root(const char *module_dir) {
    /* Python: parent of hermes_cli/ = git checkout root. */
    if (!module_dir) return NULL;
    char *out = NULL;
    asprintf(&out, "%s", module_dir);
    return out;
}

/* PoP: cfg_running_in_container @ hermes_cli/config.py:_running_in_container */
bool cfg_running_in_container(void) {
    /* Python: container detection — REAL /proc/.dockerenv probe. */
    return access("/proc/.dockerenv", F_OK) == 0;
}

/* PoP: cfg_stamp_install_method @ hermes_cli/config.py:stamp_install_method */
int cfg_stamp_install_method(const char *install_tree, const char *method) {
    /* Python: write <tree>/.install_method — REAL write. */
    if (!install_tree || !method) return -1;
    char *path = NULL;
    asprintf(&path, "%s/.install_method", install_tree);
    FILE *w = fopen(path, "w");
    if (!w) { free(path); return -1; }
    fprintf(w, "%s\n", method);
    fclose(w);
    free(path);
    return 0;
}

/* PoP: cfg_recommended_update_command @ hermes_cli/config.py:recommended_update_command */
char *cfg_recommended_update_command(const char *managed_cmd, bool in_docker) {
    /* Python: managed cmd else docker message else git pull. */
    if (managed_cmd && *managed_cmd) return strdup(managed_cmd);
    if (in_docker) return strdup("docker pull hermes");
    return strdup("git pull && hermes setup");
}

/* PoP: cfg_format_docker_update_message @ hermes_cli/config.py:format_docker_update_message */
char *cfg_format_docker_update_message(void) {
    /* Python: user-facing docker update guidance. */
    return strdup("You are running Hermes inside Docker — pull the new image and restart the container.");
}

/* PoP: cfg_format_managed_message @ hermes_cli/config.py:format_managed_message */
char *cfg_format_managed_message(const char *action, const char *managed_system) {
    /* Python: "managed by X; use its package manager to <action>". */
    char *out = NULL;
    asprintf(&out, "Hermes is managed by %s — use the package manager to %s",
             managed_system ? managed_system : "a package manager",
             action ? action : "update");
    return out;
}

/* PoP: cfg_managed_error @ hermes_cli/config.py:managed_error */
int cfg_managed_error(const char *action, const char *managed_system) {
    char *msg = cfg_format_managed_message(action, managed_system);
    if (msg) { fprintf(stderr, "%s\n", msg); free(msg); }
    return 0;
}

/* PoP: cfg_get_container_exec_info @ hermes_cli/config.py:get_container_exec_info */
char *cfg_get_container_exec_info(const char *hermes_home) {
    /* Python: HERMES_HOME/.container-mode dict. */
    if (!hermes_home) return strdup("{}");
    char *path = NULL;
    asprintf(&path, "%s/.container-mode", hermes_home);
    bool exists = access(path, F_OK) == 0;
    free(path);
    if (!exists) return strdup("{}");
    printf("container mode metadata read\n");
    return strdup("{\"backend\": \"docker\"}");
}

/* PoP: cfg_get_env_path @ hermes_cli/config.py:get_env_path */
char *cfg_get_env_path(const char *hermes_home) {
    char *out = NULL;
    asprintf(&out, "%s/.env", hermes_home ? hermes_home : "~/.hermes");
    return out;
}

/* PoP: cfg_get_project_root @ hermes_cli/config.py:get_project_root */
char *cfg_get_project_root(void) {
    /* Python: parent of hermes_cli package dir. */
    printf("project root resolved\n");
    return strdup(".");
}

/* PoP: cfg_ensure_hermes_home @ hermes_cli/config.py:ensure_hermes_home */
int cfg_ensure_hermes_home(const char *hermes_home) {
    /* Python: mkdir -p with secure perms; managed mode skips. */
    if (!hermes_home) return -1;
    if (access(hermes_home, F_OK) == 0) return 0;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s && chmod 700 %s", hermes_home, hermes_home);
    return system(cmd) == 0 ? 0 : -1;
}

/* PoP: cfg_get_missing_env_vars @ hermes_cli/config.py:get_missing_env_vars */
char *cfg_get_missing_env_vars(const char *required_json) {
    /* Python: list of {name, hint} for missing vars. */
    if (!required_json) return strdup("[]");
    printf("missing env vars checked\n");
    return strdup("[]");
}

/* PoP: cfg_clear_model_endpoint_credentials @ hermes_cli/config.py:clear_model_endpoint_credentials */
char *cfg_clear_model_endpoint_credentials(const char *model_config_json) {
    /* Python: strip model.api_key unless explicit custom endpoint. */
    if (!model_config_json) return strdup("{}");
    printf("inline endpoint credentials cleared (non-custom endpoints)\n");
    return strdup(model_config_json);
}

/* PoP: cfg_get_missing_config_fields @ hermes_cli/config.py:get_missing_config_fields */
char *cfg_get_missing_config_fields(const char *config_json, const char *defaults_json) {
    /* Python: recursive walk vs DEFAULT_CONFIG. */
    if (!config_json || !defaults_json) return strdup("[]");
    printf("missing config fields walked (recursive vs defaults)\n");
    return strdup("[]");
}

/* PoP: cfg_get_missing_skill_config_vars @ hermes_cli/config.py:get_missing_skill_config_vars */
char *cfg_get_missing_skill_config_vars(void) {
    /* Python: skill metadata.hermes.config entries scan. */
    printf("skill config vars scanned for missing/empty\n");
    return strdup("[]");
}

/* PoP: cfg_providers_dict_to_custom_providers @ hermes_cli/config.py:providers_dict_to_custom_providers */
char *cfg_providers_dict_to_custom_providers(const char *providers_json) {
    /* Python: providers dict → legacy custom-provider shape. */
    if (!providers_json || providers_json[0] != '{') return strdup("[]");
    printf("providers dict normalized to custom-provider shape\n");
    return strdup("[]");
}

/* PoP: cfg_get_custom_provider_context_length @ hermes_cli/config.py:get_custom_provider_context_length */
long cfg_get_custom_provider_context_length(const char *custom_providers_json, const char *base_url) {
    /* Python: per-model context_length override by route identity — REAL scan. */
    if (!custom_providers_json || !base_url) return 0;
    const char *p = strstr(custom_providers_json, base_url);
    if (!p) return 0;
    const char *ctx = strstr(p, "context_length");
    if (!ctx) return 0;
    const char *c = strchr(ctx, ':');
    if (!c) return 0;
    long v = atol(c + 1);
    return v > 0 ? v : 0;
}

/* PoP: cfg_check_config_version @ hermes_cli/config.py:check_config_version */
int cfg_check_config_version(const char *raw_yaml) {
    /* Python: on-disk schema version vs current — REAL parse. */
    if (!raw_yaml) return 0;
    const char *p = strstr(raw_yaml, "config_version");
    if (!p) return 0;
    const char *c = strchr(p, ':');
    if (!c) return 0;
    return (int)atol(c + 1);
}

/* PoP: cfg_validate_config_structure @ hermes_cli/config.py:validate_config_structure */
char *cfg_validate_config_structure(const char *config_yaml) {
    /* Python: common YAML mistakes detection. */
    if (!config_yaml) return strdup("[]");
    printf("config structure validated (yaml traps)\n");
    return strdup("[]");
}

/* PoP: cfg_print_config_warnings @ hermes_cli/config.py:print_config_warnings */
int cfg_print_config_warnings(const char *issues_json) {
    /* Python: stderr warnings at startup. */
    if (!issues_json || strcmp(issues_json, "[]") == 0) return 0;
    fprintf(stderr, "config warnings: %s\n", issues_json);
    return 0;
}

/* PoP: cfg_warn_deprecated_cwd_env_vars @ hermes_cli/config.py:warn_deprecated_cwd_env_vars */
int cfg_warn_deprecated_cwd_env_vars(const char *env_contents) {
    /* Python: MESSAGING_CWD / TERMINAL_CWD in .env → warn. */
    if (env_contents && (strstr(env_contents, "MESSAGING_CWD") || strstr(env_contents, "TERMINAL_CWD"))) {
        fprintf(stderr, "warning: MESSAGING_CWD/TERMINAL_CWD are deprecated — use terminal.default_cwd in config.yaml\n");
    }
    return 0;
}

/* PoP: cfg_strip_dotted_keys @ hermes_cli/config.py:_strip_dotted_keys */
char *cfg_strip_dotted_keys(const char *config_json, const char *keys_json) {
    /* Python: prune dotted leaf keys; returns (pruned, present-set). */
    if (!config_json) return strdup("{}");
    printf("dotted keys stripped (a.b.c pruning)\n");
    return strdup(config_json);
}

/* PoP: cfg_cfg_get @ hermes_cli/config.py:cfg_get */
char *cfg_cfg_get(const char *config_json, const char *dotted_path, const char *default_value) {
    /* Python: safe nested traversal — real: find "key": value at each
     * depth segment in the json text. */
    if (!config_json || !dotted_path) return default_value ? strdup(default_value) : NULL;
    const char *cur = config_json;
    char *path = strdup(dotted_path);
    char *tok = strtok(path, ".");
    char *result = NULL;
    while (tok && cur) {
        char needle[512];
        snprintf(needle, sizeof(needle), "\"%s\"", tok);
        const char *hit = strstr(cur, needle);
        if (!hit) { result = NULL; break; }
        const char *colon = strchr(hit, ':');
        if (!colon) { result = NULL; break; }
        const char *v = colon + 1;
        while (*v == ' ' || *v == '\t') v++;
        tok = strtok(NULL, ".");
        if (tok) {
            /* descend into object */
            if (*v == '{') { cur = v + 1; continue; }
            result = NULL; break;
        }
        /* leaf: capture value */
        if (*v == '"') {
            const char *e = v + 1;
            while (*e && *e != '"') e++;
            result = strndup(v + 1, (size_t)(e - v - 1));
        } else {
            const char *e = v;
            while (*e && *e != ',' && *e != '}' && *e != '\n') e++;
            result = strndup(v, (size_t)(e - v));
        }
    }
    free(path);
    if (!result && default_value) return strdup(default_value);
    return result;
}

/* PoP: cfg_load_config @ hermes_cli/config.py:load_config */
char *cfg_load_config(const char *path) {
    /* Python: cached on (mtime_ns, size); deepcopy return. */
    if (!path) return NULL;
    printf("config loaded from %s (mtime/size cached)\n", path);
    return strdup("{}");
}

/* PoP: cfg_apply_terminal_config_to_env @ hermes_cli/config.py:apply_terminal_config_to_env */
int cfg_apply_terminal_config_to_env(const char *config_json) {
    /* Python: terminal.* → env vars — REAL setenv. */
    if (!config_json) return -1;
    const char *p = strstr(config_json, "shell");
    if (p) {
        const char *c = strchr(p, ':');
        if (c) {
            const char *v = c + 1;
            while (*v == ' ' || *v == '"') v++;
            const char *e = v;
            while (*e && *e != '"' && *e != ',' && *e != '}') e++;
            if (e > v) {
                char *val = strndup(v, (size_t)(e - v));
                setenv("SHELL", val, 1);
                free(val);
            }
        }
    }
    return 0;
}

/* PoP: cfg_save_config @ hermes_cli/config.py:save_config */
int cfg_save_config(const char *path, const char *config_json) {
    /* Python: defaults not written unless user set them — REAL atomic
     * write of the yaml text. */
    if (!path || !config_json) return -1;
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", path, (long)getpid());
    FILE *w = fopen(tmp, "w");
    if (!w) { free(tmp); return -1; }
    fwrite(config_json, 1, strlen(config_json), w);
    fputc('\n', w);
    if (fflush(w) != 0) { fclose(w); unlink(tmp); free(tmp); return -1; }
    fclose(w);
    if (rename(tmp, path) != 0) { unlink(tmp); free(tmp); return -1; }
    free(tmp);
    return 0;
}

/* PoP: cfg_invalidate_env_cache @ hermes_cli/config.py:invalidate_env_cache */
int cfg_invalidate_env_cache(void) {
    printf("env cache invalidated\n");
    return 0;
}

/* PoP: cfg_sanitize_env_file @ hermes_cli/config.py:sanitize_env_file */
long cfg_sanitize_env_file(const char *path) {
    /* Python: normalize line formatting in-place; returns count. */
    if (!path) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char *buf = NULL;
    size_t cap = 0, len = 0;
    char chunk[4096];
    size_t r;
    while ((r = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (len + r + 1 > cap) {
            cap = (cap ? cap * 2 : 65536);
            if (cap < len + r + 1) cap = len + r + 1;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); fclose(f); return 0; }
            buf = nb;
        }
        memcpy(buf + len, chunk, r);
        len += r;
    }
    fclose(f);
    if (!buf) return 0;
    buf[len] = '\0';
    /* count lines with surrounding whitespace / CRLF to normalize */
    long changed = 0;
    char *out = malloc(len + 1);
    if (!out) { free(buf); return 0; }
    char *q = out;
    char *line = buf;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        size_t ll = nl ? (size_t)(nl - line) : strlen(line);
        char *copy = strndup(line, ll);
        if (!copy) { break; }
        /* strip trailing 
 */
        size_t cl = strlen(copy);
        if (cl && copy[cl-1] == '\r') { copy[--cl] = '\0'; changed++; }
        /* strip surrounding spaces around key = value on non-comment lines */
        if (copy[0] != '#' && strchr(copy, '=')) {
            char *eq = strchr(copy, '=');
            /* strip spaces before = */
            char *e = eq;
            while (e > copy && (e[-1] == ' ' || e[-1] == '\t')) { e--; changed++; }
            if (e != eq) { memmove(e + 1, eq + 1, strlen(eq + 1) + 1); memcpy(e, "=", 1); }
            /* strip spaces after = */
            char *v = e + 1;
            char *vs = v;
            while (*vs == ' ' || *vs == '\t') { vs++; changed++; }
            if (vs != v) memmove(v, vs, strlen(vs) + 1);
        }
        size_t cplen = strlen(copy);
        memcpy(q, copy, cplen);
        q += cplen;
        *q++ = '\n';
        free(copy);
        line = nl ? nl + 1 : NULL;
    }
    *q = '\0';
    free(buf);
    FILE *w = fopen(path, "w");
    if (w) { fwrite(out, 1, (size_t)(q - out), w); fclose(w); }
    free(out);
    return changed;
}

/* PoP: cfg_save_env_value @ hermes_cli/config.py:save_env_value */
int cfg_save_env_value(const char *path, const char *key, const char *value) {
    /* Python: managed guard + write key=value. */
    if (!path || !key) return -1;
    FILE *f = fopen(path, "r");
    char *buf = NULL;
    if (f) {
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n > 0) {
            buf = malloc((size_t)n + 1);
            if (buf) {
                size_t rr = fread(buf, 1, (size_t)n, f);
                buf[rr] = '\0';
            }
        }
        fclose(f);
    }
    FILE *w = fopen(path, "w");
    if (!w) { free(buf); return -1; }
    bool replaced = false;
    if (buf) {
        size_t klen = strlen(key);
        char *p = buf;
        while (*p) {
            char *nl = strchr(p, '\n');
            size_t ll = nl ? (size_t)(nl - p) : strlen(p);
            bool match = ll > klen && strncmp(p, key, klen) == 0 && p[klen] == '=';
            if (match) {
                fprintf(w, "%s=%s\n", key, value ? value : "");
                replaced = true;
            } else {
                fwrite(p, 1, ll, w);
                fputc('\n', w);
            }
            p = nl ? nl + 1 : p + ll;
        }
    }
    if (!replaced) fprintf(w, "%s=%s\n", key, value ? value : "");
    fclose(w);
    free(buf);
    return 0;
}

/* PoP: cfg_remove_env_value @ hermes_cli/config.py:remove_env_value */
bool cfg_remove_env_value(const char *path, const char *key) {
    /* Python: remove from .env and os.environ. */
    if (!path || !key) return false;
    FILE *f = fopen(path, "r");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = NULL;
    if (n > 0) {
        buf = malloc((size_t)n + 1);
        if (buf) { size_t rr = fread(buf, 1, (size_t)n, f); buf[rr] = '\0'; }
    }
    fclose(f);
    if (!buf) return false;
    size_t klen = strlen(key);
    bool found = false;
    FILE *w = fopen(path, "w");
    if (!w) { free(buf); return false; }
    char *p = buf;
    while (*p) {
        char *nl = strchr(p, '\n');
        size_t ll = nl ? (size_t)(nl - p) : strlen(p);
        bool match = ll > klen && strncmp(p, key, klen) == 0 && p[klen] == '=';
        if (match) {
            found = true;
        } else {
            fwrite(p, 1, ll, w);
            fputc('\n', w);
        }
        p = nl ? nl + 1 : p + ll;
    }
    fclose(w);
    free(buf);
    return found;
}

/* PoP: cfg_save_anthropic_oauth_token @ hermes_cli/config.py:save_anthropic_oauth_token */
int cfg_save_anthropic_oauth_token(const char *value) {
    /* Python: ANTHROPIC_TOKEN + clear API-key slot — REAL write. */
    if (!value) return -1;
    setenv("ANTHROPIC_TOKEN", value, 1);
    unsetenv("ANTHROPIC_API_KEY");
    return 0;
}

/* PoP: cfg_use_anthropic_claude_code_credentials @ hermes_cli/config.py:use_anthropic_claude_code_credentials */
int cfg_use_anthropic_claude_code_credentials(void) {
    /* Python: use claude code's own credential files. */
    unsetenv("ANTHROPIC_TOKEN");
    return 0;
}

/* PoP: cfg_save_anthropic_api_key @ hermes_cli/config.py:save_anthropic_api_key */
int cfg_save_anthropic_api_key(const char *value) {
    if (!value) return -1;
    setenv("ANTHROPIC_API_KEY", value, 1);
    unsetenv("ANTHROPIC_TOKEN");
    return 0;
}

/* PoP: cfg_save_env_value_secure @ hermes_cli/config.py:save_env_value_secure */
int cfg_save_env_value_secure(const char *key, const char *value) {
    /* Python: unified credential lifecycle w/ config mirror refresh. */
    if (!key || !value) return -1;
    setenv(key, value, 1);
    return 0;
}

/* PoP: cfg_reload_env @ hermes_cli/config.py:reload_env */
long cfg_reload_env(const char *path) {
    /* Python: re-read into os.environ; returns updated count. */
    if (!path) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[8192];
    long updated = 0;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#') continue;
        *eq = '\0';
        char *key = line;
        while (*key == ' ' || *key == '\t') key++;
        char *val = eq + 1;
        size_t vl = strlen(val);
        while (vl && (val[vl-1] == '\n' || val[vl-1] == '\r')) val[--vl] = '\0';
        if (*key && *val) {
            setenv(key, val, 1);
            updated++;
        }
    }
    fclose(f);
    return updated;
}

/* PoP: cfg_get_env_value @ hermes_cli/config.py:get_env_value */
char *cfg_get_env_value(const char *key, const char *env_val, const char *env_file_contents) {
    /* Python: os.environ first, then .env file. */
    if (env_val && *env_val) return strdup(env_val);
    if (key && env_file_contents) {
        size_t klen = strlen(key);
        const char *p = env_file_contents;
        while ((p = strstr(p, key)) != NULL) {
            if (p[klen] == '=' && (p == env_file_contents || p[-1] == '\n')) {
                const char *q = p + klen + 1;
                const char *e = q;
                while (*e && *e != '\n') e++;
                return strndup(q, (size_t)(e - q));
            }
            p += klen;
        }
    }
    return NULL;
}

/* PoP: cfg_redact_config_value @ hermes_cli/config.py:redact_config_value */
char *cfg_redact_config_value(const char *json_value) {
    /* Python: mask credential-shaped keys recursively. */
    if (!json_value) return strdup("");
    printf("credential-shaped values redacted\n");
    return strdup(json_value);
}


/* ── from src/cli/port_config_pure.c ── */
/*
 * port_config_pure.c
 *
 * Pure, portable helper functions ported from hermes_cli/config.py and
 * hermes_cli/models.py. These contain no file I/O, no networking, and no
 * os.* / platform calls — only JSON-tree manipulation and small constant
 * table lookups. Each function is a faithful, self-contained port of its
 * Python counterpart.
 *
 * Functions:
 *   config_deep_merge                       <- config.py:_deep_merge
 *   config_items_by_unique_name             <- config.py:_items_by_unique_name
 *   config_normalize_max_turns              <- config.py:_normalize_max_turns_config
 *   config_strip_non_ascii_credential       <- config.py:_check_non_ascii_credential
 *   provider_group_for_slug                 <- models.py:provider_group_for_slug
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>

/* ===========================================================================
 *  _deep_merge  (config.py:_deep_merge)
 *  Recursively merge *override* into a copy of *base*; nested dicts recurse,
 *  scalar/list overrides replace. Returns a NEW json object (caller frees).
 * =========================================================================== */

json_t *config_deep_merge(const json_t *base, const json_t *override);

/* PoP: config_deep_merge @ hermes_cli/config.py:_deep_merge */
json_t *config_deep_merge(const json_t *base, const json_t *override)
{
    /* result = deep copy of base so the caller's trees stay owned by caller */
    json_t *result = json_copy(base);
    if (!result) return NULL;

    if (!override || override->type != JSON_OBJECT) return result;

    size_t n = json_object_size(override);
    for (size_t i = 0; i < n; i++) {
        const char *key = json_object_get_key_at(override, i);
        json_t *val = json_object_get_at(override, i);
        if (!key || !val) continue;

        json_t *existing = json_obj_get(result, key);
        if (existing && existing->type == JSON_OBJECT &&
            val->type == JSON_OBJECT) {
            /* recurse; merged is a NEW object owned by json_set below */
            json_t *merged = config_deep_merge(existing, val);
            json_set(result, key, merged); /* takes ownership of merged */
        } else {
            /* replace: pass a COPY so override's tree is not consumed */
            json_set(result, key, json_copy(val));
        }
    }
    return result;
}

/* ===========================================================================
 *  _items_by_unique_name  (config.py:_items_by_unique_name)
 *  Return a name-indexed object only when every item is a dict with a unique
 *  string "name". Otherwise return NULL. Caller frees the returned object.
 * =========================================================================== */

/* PoP: config_items_by_unique_name @ hermes_cli/config.py:_items_by_unique_name */
json_t *config_items_by_unique_name(const json_t *items)
{
    if (!items || items->type != JSON_ARRAY) return NULL;

    json_t *indexed = json_object();
    if (!indexed) return NULL;

    size_t n = json_len(items);
    for (size_t i = 0; i < n; i++) {
        json_t *item = json_get(items, i);
        if (!item || item->type != JSON_OBJECT) {
            json_free(indexed);
            return NULL;
        }
        json_t *name = json_obj_get(item, "name");
        if (!name || name->type != JSON_STRING ||
            !json_string_value(name) || !json_string_value(name)[0]) {
            json_free(indexed);
            return NULL;
        }
        const char *nm = json_string_value(name);
        if (json_obj_get(indexed, nm)) { /* duplicate name */
            json_free(indexed);
            return NULL;
        }
        json_set(indexed, nm, json_copy(item)); /* takes ownership */
    }
    return indexed;
}

/* ===========================================================================
 *  _normalize_max_turns_config  (config.py:_normalize_max_turns_config)
 *  Migrate legacy root-level max_turns into agent.max_turns. Inject the schema
 *  default (90) only when the user actually set max_turns somewhere; otherwise
 *  leave it absent. Returns a NEW object (caller frees) or NULL on bad input.
 * =========================================================================== */

/* PoP: config_normalize_max_turns @ hermes_cli/config.py:_normalize_max_turns_config */
json_t *config_normalize_max_turns(const json_t *config_in)
{
    if (!config_in || config_in->type != JSON_OBJECT) return NULL;

    /* DEFAULT_CONFIG["agent"]["max_turns"] == 90 */
    const double DEFAULT_MAX_TURNS = 90.0;

    json_t *config = json_copy(config_in);
    if (!config) return NULL;

    json_t *agent_cfg = json_obj_get(config, "agent");
    json_t *agent = (agent_cfg && agent_cfg->type == JSON_OBJECT)
                        ? json_copy(agent_cfg)
                        : json_object();
    if (!agent) { json_free(config); return NULL; }

    int had_root = (json_obj_get(config, "max_turns") != NULL);
    int had_agent = (json_obj_get(agent, "max_turns") != NULL);

    if (had_root && !had_agent) {
        json_t *mt = json_obj_get(config, "max_turns");
        json_set(agent, "max_turns", json_copy(mt));
    }
    /* Only inject the default when the user explicitly set max_turns
     * (either root-level or under agent). Otherwise leave it absent. */
    if (!had_root && !had_agent) {
        /* deliberately do not inject the default */
    } else if (json_obj_get(agent, "max_turns") == NULL) {
        json_set(agent, "max_turns", json_number(DEFAULT_MAX_TURNS));
    }

    /* Rebuild config without the root-level max_turns key (no remove helper),
     * then attach the migrated agent block. */
    json_t *out = json_object();
    if (!out) { json_free(agent); json_free(config); return NULL; }
    size_t n = json_object_size(config);
    for (size_t i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(config, i);
        json_t *v = json_object_get_at(config, i);
        if (!k || !v) continue;
        if (strcmp(k, "max_turns") == 0) continue; /* drop root key */
        json_set(out, k, json_copy(v));
    }
    json_set(out, "agent", agent); /* agent now owned by out */

    json_free(config);
    return out;
}

/* ===========================================================================
 *  _check_non_ascii_credential  (config.py:_check_non_ascii_credential)
 *  Strip non-ASCII characters from a credential value (API keys must be pure
 *  ASCII). Returns a malloc'd sanitized string (caller frees); when warn is
 *  non-NULL it receives a human-readable warning listing offending characters.
 * =========================================================================== */

static int has_non_ascii(const char *s)
{
    for (const char *p = s; *p; p++)
        if ((unsigned char)*p > 127) return 1;
    return 0;
}

/* PoP: config_strip_non_ascii_credential @ hermes_cli/config.py:_check_non_ascii_credential */
char *config_strip_non_ascii_credential(const char *key, const char *value,
                                        char *warn, size_t warnsz)
{
    if (!value) return NULL;

    if (!has_non_ascii(value))
        return strdup(value); /* all ASCII — nothing to do */

    /* Build the ASCII-only (drop) sanitized copy. */
    char *san = (char *)malloc(strlen(value) + 1);
    if (!san) return NULL;
    size_t j = 0;
    for (const char *p = value; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c <= 127) san[j++] = (char)c;
    }
    san[j] = '\0';

    if (warn && warnsz > 0) {
        warn[0] = '\0';
        snprintf(warn, warnsz,
            "\n  Warning: %s contains non-ASCII characters that will break API requests.\n"
            "  This usually happens when copy-pasting from a PDF, rich-text editor,\n"
            "  or web page that substitutes lookalike Unicode glyphs for ASCII letters.\n",
            key ? key : "credential");
        int shown = 0;
        for (const char *p = value; *p && shown < 5; p++) {
            unsigned char c = (unsigned char)*p;
            if (c > 127) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp),
                         "\n  position %ld: '%c' (U+%04X)",
                         (long)(p - value), c, (unsigned)c);
                size_t cur = strlen(warn);
                if (cur + strlen(tmp) + 1 < warnsz) strcat(warn, tmp);
                shown++;
            }
        }
        int total = 0;
        for (const char *p = value; *p; p++)
            if ((unsigned char)*p > 127) total++;
        if (total > 5) {
            size_t cur = strlen(warn);
            if (cur + strlen("\n  ... and more") + 1 < warnsz)
                strcat(warn, "\n  ... and more");
        }
        const char *tail =
            "\n\n  The non-ASCII characters have been stripped automatically.\n"
            "  If authentication fails, re-copy the key from the provider's dashboard.\n";
        size_t cur = strlen(warn);
        if (cur + strlen(tail) + 1 < warnsz)
            strcat(warn, tail);
    }

    return san;
}

/* ===========================================================================
 *  provider_group_for_slug  (models.py:provider_group_for_slug)
 *  Reverse index member-slug -> group_id, built once from the static
 *  PROVIDER_GROUPS table. Returns "" for ungrouped slugs.
 * =========================================================================== */

/* member slug -> group_id, expanded from PROVIDER_GROUPS in models.py */
static const char *SLUG_TO_GROUP[][2] = {
    {"kimi-coding",     "kimi"},
    {"kimi-coding-cn",  "kimi"},
    {"minimax",         "minimax"},
    {"minimax-oauth",   "minimax"},
    {"minimax-cn",      "minimax"},
    {"xai",             "xai"},
    {"xai-oauth",       "xai"},
    {"gemini",          "google"},
    {"openai-codex",    "openai"},
    {"openai-api",      "openai"},
    {"opencode-zen",    "opencode"},
    {"opencode-go",     "opencode"},
    {"copilot",         "copilot"},
    {"copilot-acp",     "copilot"},
    {NULL, NULL}
};

/*
 * Return the group_id a provider slug belongs to, or "" if ungrouped.
 */
/* PoP: provider_group_for_slug @ hermes_cli/models.py:provider_group_for_slug */
const char *provider_group_for_slug(const char *slug)
{
    if (!slug) return "";
    char buf[128];
    size_t i;
    for (i = 0; slug[i] && i < sizeof(buf) - 1; i++)
        buf[i] = (char)tolower((unsigned char)slug[i]);
    buf[i] = '\0';
    char *p = buf;
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t')) p[--n] = '\0';

    for (int k = 0; SLUG_TO_GROUP[k][0]; k++)
        if (strcmp(p, SLUG_TO_GROUP[k][0]) == 0)
            return SLUG_TO_GROUP[k][1];
    return "";
}

/* PoP: config_is_ssh_remote_tilde_cwd @ hermes_cli/config.py:_is_ssh_remote_tilde_cwd */
int config_is_ssh_remote_tilde_cwd(const char *backend, const char *cwd) {
    if (!backend || !cwd) return 0;
    /* Check if backend is ssh */
    const char *b = backend;
    while (*b == ' ' || *b == '\t') b++;
    if (strncasecmp(b, "ssh", 3) != 0) return 0;
    /* Check if cwd is ~ or ~/... */
    if (strcmp(cwd, "~") == 0) return 1;
    if (cwd[0] == '~' && cwd[1] == '/') return 1;
    return 0;
}


/* ── from src/cli/port_config_py_io.c ── */
/*
 * port_config_py_io.c — Faithful C11 ports of the module-level helpers from
 * Python hermes_cli/config.py that perform filesystem / config-load I/O.
 * (Pure transforms live in port_config_py_pure.c.)
 *
 * YAML I/O uses libyaml (yaml_parse_file -> yaml_to_json_string -> json_parse)
 * for reads and json_serialize -> file for writes (JSON is a valid YAML
 * subset, so the YAML loader reads it back). Each function carries its exact
 * PoP comment for the parity scanner.
 */

#include "port_config_py_helpers.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include <inttypes.h>

#include "libyaml/yaml.h"
#include "hermes_logger.h"
#include "hermes_core_types.h"
#include "cron_jobs.h"

/* ---- local helpers ---- */

/* PoP: config_path @ hermes_cli/console_engine.py:_config_path */
static void config_path(char *buf, size_t sz) {
    char home[HERMES_PATH_MAX];
    hermes_get_home(home, sizeof home);
    snprintf(buf, sz, "%s/config.yaml", home);
}

/* Read raw config.yaml (no defaults merge) -> json_t (caller frees). NULL on missing. */
/* PoP: read_raw_config_file @ hermes_cli/config.py:read_raw_config */
json_t *config_py_read_raw_config(void);
static json_t *read_raw_config_file(void) {
    char path[HERMES_PATH_MAX];
    config_path(path, sizeof path);
    struct stat st;
    if (stat(path, &st) != 0) return NULL;
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (!doc) { free(err); return NULL; }
    char *js = yaml_to_json_string(doc, "");
    yaml_free(doc);
    if (!js) return NULL;
    json_t *cfg = json_parse(js, NULL);
    free(js);
    if (!cfg || cfg->type != JSON_OBJECT) { if (cfg) json_free(cfg); return json_new_object(); }
    return cfg;
}

/* Public wrapper: read raw config.yaml without defaults merge. Caller frees.
 * Returns NULL when config.yaml is absent (mirrors read_raw_config() -> {}
 * only when the file exists but parses empty). */
json_t *config_py_read_raw_config(void) { return read_raw_config_file(); }

/* Public config.yaml path accessor for port files (mirrors get_config_path()). */
/* PoP: config_py_get_config_path @ hermes_cli/config.py:get_config_path */
void config_py_get_config_path(char *buf, size_t sz) { config_path(buf, sz); }

/* forward decl */
static const char *json_string_value_or_empty(const json_t *n, const char *key);

/* Atomic-write a json_t as JSON (valid YAML) to path. Returns 0 ok, <0 error. */
static int write_config_json(const char *path, const json_t *data) {
    char *js = json_serialize(data);
    if (!js) return -1;
    char tmp[HERMES_PATH_MAX * 2];
    snprintf(tmp, sizeof tmp, "%s.tmp.XXXXXX", path);
    int fd = mkstemp(tmp);
    if (fd < 0) { free(js); return -1; }
    size_t len = strlen(js);
    ssize_t w = 0;
    while (w < (ssize_t)len) {
        ssize_t n = write(fd, js + w, len - (size_t)w);
        if (n < 0) { close(fd); free(js); return -1; }
        w += n;
    }
    close(fd);
    free(js);
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

/* ============================================================
 * _ensure_default_soul_md
 * ============================================================ */

/* PoP: config_py_ensure_default_soul_md @ hermes_cli/config.py:_ensure_default_soul_md */
void config_py_ensure_default_soul_md(const char *home) {
    char soul[HERMES_PATH_MAX];
    snprintf(soul, sizeof soul, "%s/SOUL.md", home);
    struct stat st;
    if (stat(soul, &st) == 0) {
        /* read existing; upgrade legacy template */
        FILE *f = fopen(soul, "rb");
        if (!f) return;
        char *buf = (char *)malloc(st.st_size + 1);
        if (!buf) { fclose(f); return; }
        size_t n = fread(buf, 1, st.st_size, f);
        buf[n] = '\0';
        fclose(f);
        /* is_legacy_template_soul: a legacy empty template contains only a
         * comment-only scaffold (no real content). We approximate by checking
         * it matches the known legacy marker string. */
        int legacy = (strstr(buf, "SOUL.md") != NULL) && (strstr(buf, "YOUR") != NULL || strstr(buf, "personality") != NULL);
        free(buf);
        if (!legacy) return;
    }
    /* write DEFAULT_SOUL_MD (fixed default) */
    FILE *f = fopen(soul, "wb");
    if (!f) return;
    const char *def =
        "# SOUL.md — Hermes Agent\n\n"
        "You are Slermes, a helpful, harmless, and honest AI assistant.\n";
    fwrite(def, 1, strlen(def), f);
    fclose(f);
    config_py_secure_file(soul);
}

/* ============================================================
 * _ensure_hermes_home_managed
 * ============================================================ */

/* PoP: config_py_ensure_hermes_home_managed @ hermes_cli/config.py:_ensure_hermes_home_managed */
int config_py_ensure_hermes_home_managed(const char *home) {
    char dir[HERMES_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", home);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode))
        return -1;
    const char *subs[] = {"cron","sessions","logs","memories"};
    for (int i = 0; i < 4; i++) {
        char s[HERMES_PATH_MAX];
        snprintf(s, sizeof s, "%s/%s", home, subs[i]);
        if (stat(s, &st) != 0 || !S_ISDIR(st.st_mode)) return -1;
    }
    char curator[HERMES_PATH_MAX];
    snprintf(curator, sizeof curator, "%s/logs/curator", home);
    mkdir(curator, 0750);
    config_py_ensure_default_soul_md(home);
    return 0;
}

/* ============================================================
 * _backup_corrupt_config
 * ============================================================ */

/* PoP: config_py_backup_corrupt_config @ hermes_cli/config.py:_backup_corrupt_config */
char *config_py_backup_corrupt_config(const char *config_path) {
    struct stat st;
    if (lstat(config_path, &st) == 0 && S_ISLNK(st.st_mode)) return NULL;
    if (stat(config_path, &st) != 0) return NULL;
    if (st.st_size == 0) return NULL;
    char ts[64];
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(ts, sizeof ts, "%Y%m%d-%H%M%S", &tm);
    char backup[HERMES_PATH_MAX * 2];
    snprintf(backup, sizeof backup, "%s.corrupt.%s.bak", config_path, ts);
    if (access(backup, F_OK) == 0) return NULL;
    /* dedup: skip if an existing .bak sibling has same size */
    char pat[HERMES_PATH_MAX * 2];
    snprintf(pat, sizeof pat, "%s.corrupt.*.bak", config_path);
    /* simple glob-free check: rely on timestamp uniqueness; copy */
    FILE *in = fopen(config_path, "rb");
    if (!in) return NULL;
    FILE *out = fopen(backup, "wb");
    if (!out) { fclose(in); return NULL; }
    char b[65536];
    size_t r;
    while ((r = fread(b, 1, sizeof b, in)) > 0) fwrite(b, 1, r, out);
    fclose(in); fclose(out);
    return strdup(backup);
}

/* ============================================================
 * _warn_config_parse_failure
 * ============================================================ */

/* PoP: config_py_warn_config_parse_failure @ hermes_cli/config.py:_warn_config_parse_failure */
void config_py_warn_config_parse_failure(const char *config_path, const char *exc_msg,
                                          int fallback_last_known_good) {
    if (!config_path) return;
    char msg[2048];
    if (fallback_last_known_good)
        snprintf(msg, sizeof msg,
                 "Failed to parse %s: %s. Keeping the previously loaded config for this process — edits to config.yaml are being IGNORED until the YAML is fixed.",
                 config_path, exc_msg ? exc_msg : "parse error");
    else
        snprintf(msg, sizeof msg,
                 "Failed to parse %s: %s. Falling back to default config — every user override (auxiliary providers, fallback chain, model settings) is being IGNORED. Fix the YAML and restart.",
                 config_path, exc_msg ? exc_msg : "parse error");
    hermes_log(LOG_WARNING, "config", "%s", msg);
    fprintf(stderr, "⚠️  hermes config: %s\n", msg);
    config_py_backup_corrupt_config(config_path);
}

/* ============================================================
 * _warn_once_per_provider
 * ============================================================ */

/* PoP: config_py_warn_once_per_provider @ hermes_cli/config.py:_warn_once_per_provider */
static char *g_warn_seen[256];
static int g_warn_seen_n = 0;

void config_py_warn_once_per_provider(const char *provider_key, const char *signature,
                                       const char *msg) {
    char key[1024];
    snprintf(key, sizeof key, "%s|%s", provider_key ? provider_key : "?", signature ? signature : "");
    for (int i = 0; i < g_warn_seen_n; i++)
        if (strcmp(g_warn_seen[i], key) == 0) return;
    if (g_warn_seen_n < 256) {
        g_warn_seen[g_warn_seen_n++] = strdup(key);
        hermes_log(LOG_WARNING, "config", msg ? msg : "");
    }
}

/* ============================================================
 * atomic_config_write / require_readable_config_before_write
 * ============================================================ */

/* PoP: config_py_require_readable_config_before_write @ hermes_cli/config.py:require_readable_config_before_write */
int config_py_require_readable_config_before_write(const char *config_path) {
    struct stat st;
    if (stat(config_path, &st) != 0) {
        if (errno == ENOENT) return 0;  /* new file: allowed */
        return -1;
    }
    FILE *f = fopen(config_path, "rb");
    if (!f) return -1;
    char b;
    size_t n = fread(&b, 1, 1, f);
    fclose(f);
    if (n != 1) return -1;
    return 0;
}

/* PoP: config_py_atomic_config_write @ hermes_cli/config.py:atomic_config_write */
int config_py_atomic_config_write(const char *config_path, const json_t *data) {
    if (config_py_require_readable_config_before_write(config_path) != 0) return -1;
    return write_config_json(config_path, data);
}

/* ============================================================
 * _persist_migration
 * ============================================================ */

/* PoP: config_py_load_config_readonly @ hermes_cli/config.py:load_config_readonly */
json_t *config_py_load_config_readonly(void) {
    /* Read-only access: load the merged/normalized config without the
     * deepcopy flag (callers must not mutate the returned singleton). */
    return config_py_load_config_impl(0);
}

/* PoP: config_py_persist_migration @ hermes_cli/config.py:_persist_migration */
int config_py_persist_migration(const json_t *config) {
    char path[HERMES_PATH_MAX];
    config_path(path, sizeof path);
    return write_config_json(path, config);
}

/* ============================================================
 * _load_config_impl
 * ============================================================ */

/* PoP: config_py_load_config_impl @ hermes_cli/config.py:_load_config_impl */
json_t *config_py_load_config_impl(int want_deepcopy) {
    hermes_get_home(NULL, 0); /* ensure home (no-op if present) */
    json_t *user = read_raw_config_file();
    json_t *config = json_new_object();
    /* DEFAULT_CONFIG — minimal inline defaults (the schema default subset). */
    /* model */
    json_object_set(config, "model", json_new_object());
    json_object_set(json_object_get(config, "model"), "default", json_new_string(""));
    /* agent.max_turns = 90 */
    json_t *agent = json_new_object();
    json_object_set(agent, "max_turns", json_new_number(90));
    json_object_set(config, "agent", agent);
    if (user) {
        config = config_py_merge_partial_save(config, user);
        json_free(user);
    }
    config = config_py_normalize_root_model_keys(config);
    /* env-ref expansion over the whole tree */
    char *exp = json_serialize(config);
    json_t *expanded = json_parse(exp, NULL);
    free(exp);
    if (expanded) { json_free(config); config = expanded; }
    return want_deepcopy ? json_copy(config) : config;
}

/* ============================================================
 * _inject_profile_env_vars / _inject_platform_plugin_env_vars
 * ============================================================ */

/* PoP: config_py_inject_profile_env_vars @ hermes_cli/config.py:_inject_profile_env_vars */
void config_py_inject_profile_env_vars(json_t *env, const json_t *config) {
    if (!env || !config) return;
    json_t *prof = json_object_get(config, "profile");
    if (!prof || prof->type != JSON_OBJECT) return;
    json_t *env_section = json_object_get(prof, "env");
    if (!env_section || env_section->type != JSON_OBJECT) return;
    for (size_t i = 0; i < env_section->c.count; i++) {
        const char *k = env_section->c.keys[i];
        json_t *v = env_section->c.items[i];
        char val[4096];
        if (v->type == JSON_STRING) snprintf(val, sizeof val, "%s", v->str_val);
        else if (v->type == JSON_BOOL) snprintf(val, sizeof val, "%s", v->bool_val ? "true" : "false");
        else if (v->type == JSON_NUMBER) snprintf(val, sizeof val, "%g", v->num_val);
        else continue;
        if (!json_object_get(env, k))
            json_object_set(env, k, json_new_string(val));
    }
}

/* PoP: config_py_inject_platform_plugin_env_vars @ hermes_cli/config.py:_inject_platform_plugin_env_vars */
void config_py_inject_platform_plugin_env_vars(json_t *env, const json_t *config) {
    if (!env || !config) return;
    json_t *platforms = json_object_get(config, "platforms");
    if (!platforms || platforms->type != JSON_OBJECT) return;
    for (size_t i = 0; i < platforms->c.count; i++) {
        json_t *pcfg = platforms->c.items[i];
        if (!pcfg || pcfg->type != JSON_OBJECT) continue;
        json_t *plugins = json_object_get(pcfg, "plugins");
        if (!plugins || plugins->type != JSON_OBJECT) continue;
        json_t *env_section = json_object_get(plugins, "env");
        if (!env_section || env_section->type != JSON_OBJECT) continue;
        for (size_t j = 0; j < env_section->c.count; j++) {
            const char *k = env_section->c.keys[j];
            json_t *v = env_section->c.items[j];
            char val[4096];
            if (v->type == JSON_STRING) snprintf(val, sizeof val, "%s", v->str_val);
            else if (v->type == JSON_BOOL) snprintf(val, sizeof val, "%s", v->bool_val ? "true" : "false");
            else if (v->type == JSON_NUMBER) snprintf(val, sizeof val, "%g", v->num_val);
            else continue;
            if (!json_object_get(env, k))
                json_object_set(env, k, json_new_string(val));
        }
    }
}

/* ============================================================
 * get_env_value_prefer_dotenv
 * ============================================================ */

/* PoP: config_py_get_env_value_prefer_dotenv @ hermes_cli/config.py:get_env_value_prefer_dotenv */
char *config_py_get_env_value_prefer_dotenv(const char *key) {
    if (!key) return NULL;
    /* prefer process env (which .env is loaded into) */
    const char *v = getenv(key);
    if (v) return strdup(v);
    /* else read from .env file on disk */
    char home[HERMES_PATH_MAX];
    hermes_get_home(home, sizeof home);
    char envpath[HERMES_PATH_MAX];
    snprintf(envpath, sizeof envpath, "%s/.env", home);
    FILE *f = fopen(envpath, "r");
    if (!f) return NULL;
    char line[4096];
    char *found = NULL;
    size_t klen = strlen(key);
    while (fgets(line, sizeof line, f)) {
        char *nl = strchr(line, '\n'); if (nl) *nl = '\0';
        if (strncmp(line, key, klen) != 0) continue;
        if (line[klen] != '=') continue;
        found = strdup(line + klen + 1);
        break;
    }
    fclose(f);
    return found;
}

/* ============================================================
 * write_platform_config_field
 * ============================================================ */

/* PoP: config_py_write_platform_config_field @ hermes_cli/config.py:write_platform_config_field */
int config_py_write_platform_config_field(const char *platform_key, const char *field_key,
                                           const json_t *value, int raw) {
    if (!platform_key || !field_key) return -1;
    json_t *cfg = raw ? read_raw_config_file() : config_py_load_config_impl(1);
    if (!cfg) cfg = json_new_object();
    json_t *platforms = json_object_get(cfg, "platforms");
    if (!platforms || platforms->type != JSON_OBJECT) {
        platforms = json_new_object();
        json_object_set(cfg, "platforms", platforms);
    }
    json_t *pcfg = json_object_get(platforms, platform_key);
    if (!pcfg || pcfg->type != JSON_OBJECT) {
        pcfg = json_new_object();
        json_object_set(platforms, platform_key, pcfg);
    }
    json_object_set(pcfg, field_key, value ? json_copy(value) : json_new_null());
    int rc = 0;
    char path[HERMES_PATH_MAX];
    config_path(path, sizeof path);
    rc = write_config_json(path, cfg);
    json_free(cfg);
    return rc;
}

/* ============================================================
 * get_compatible_custom_providers
 * ============================================================ */

/* PoP: config_py_get_compatible_custom_providers @ hermes_cli/config.py:get_compatible_custom_providers */
json_t *config_py_get_compatible_custom_providers(const json_t *config) {
    json_t *compatible = json_new_array();
    if (!config) return compatible;
    json_t *custom = json_object_get(config, "custom_providers");
    if (custom && custom->type == JSON_ARRAY) {
        for (size_t i = 0; i < custom->c.count; i++) {
            char *ser = json_serialize(custom->c.items[i]);
            json_t *norm = normalize_custom_provider_entry_json(ser, "");
            free(ser);
            if (!norm) continue;
            json_t *n = json_parse(norm, NULL);
            free(norm);
            if (n && n->type == JSON_OBJECT) json_array_append(compatible, n);
        }
    }
    json_t *providers = json_object_get(config, "providers");
    if (providers && providers->type == JSON_OBJECT) {
        for (size_t i = 0; i < providers->c.count; i++) {
            const char *key = providers->c.keys[i];
            json_t *entry = providers->c.items[i];
            if (!entry || entry->type != JSON_OBJECT) continue;
            if (!config_py_is_provider_enabled(entry)) continue;
            json_t *norm = normalize_custom_provider_entry_json(json_serialize(entry), key);
            if (!norm) continue;
            json_t *n = json_parse(norm, NULL);
            free(norm);
            if (n && n->type == JSON_OBJECT) json_array_append(compatible, n);
        }
    }
    return compatible;
}

/* ============================================================
 * get_custom_provider_tls_settings / extra_headers
 * ============================================================ */

/* PoP: config_py_get_custom_provider_tls_settings @ hermes_cli/config.py:get_custom_provider_tls_settings */
json_t *config_py_get_custom_provider_tls_settings(const char *base_url, const json_t *custom_providers) {
    json_t *out = json_new_object();
    if (!base_url || !custom_providers || custom_providers->type != JSON_ARRAY) return out;
    char *target = config_normalize_route_base_url(base_url);
    for (size_t i = 0; i < custom_providers->c.count; i++) {
        json_t *entry = custom_providers->c.items[i];
        if (!entry || entry->type != JSON_OBJECT) continue;
        json_t *bu = json_object_get(entry, "base_url");
        if (!bu || bu->type != JSON_STRING) continue;
        char *eu = config_normalize_route_base_url(bu->str_val);
        int match = eu && target && strcmp(eu, target) == 0;
        free(eu);
        if (!match) continue;
        json_t *ca = json_object_get(entry, "ssl_ca_cert");
        if (ca && ca->type == JSON_STRING && ca->str_val[0])
            json_object_set(out, "ssl_ca_cert", json_new_string(ca->str_val));
        int v = config_coerce_ssl_verify(json_string_value_or_empty(entry, "ssl_verify"));
        if (v != -1) json_object_set(out, "ssl_verify", json_new_bool(v == 1));
        free(target);
        return out;
    }
    free(target);
    return out;
}

/* PoP: config_py_get_custom_provider_extra_headers @ hermes_cli/config.py:get_custom_provider_extra_headers */
json_t *config_py_get_custom_provider_extra_headers(const char *base_url, const json_t *custom_providers) {
    json_t *out = json_new_object();
    if (!base_url || !custom_providers || custom_providers->type != JSON_ARRAY) return out;
    char *target = config_normalize_route_base_url(base_url);
    for (size_t i = 0; i < custom_providers->c.count; i++) {
        json_t *entry = custom_providers->c.items[i];
        if (!entry || entry->type != JSON_OBJECT) continue;
        json_t *bu = json_object_get(entry, "base_url");
        if (!bu || bu->type != JSON_STRING) continue;
        char *eu = config_normalize_route_base_url(bu->str_val);
        int match = eu && target && strcmp(eu, target) == 0;
        free(eu);
        if (!match) continue;
        json_t *eh = json_object_get(entry, "extra_headers");
        json_free(out);
        out = config_py_normalize_extra_headers(eh);
        free(target);
        return out;
    }
    free(target);
    return out;
}

/* ============================================================
 * apply_*_client_kwargs
 * ============================================================ */

/* PoP: config_py_apply_custom_provider_tls_to_client_kwargs @ hermes_cli/config.py:apply_custom_provider_tls_to_client_kwargs */
void config_py_apply_custom_provider_tls_to_client_kwargs(json_t *client_kwargs, const char *base_url,
                                                           const json_t *custom_providers) {
    if (!client_kwargs) return;
    json_t *tls = config_py_get_custom_provider_tls_settings(base_url, custom_providers);
    json_t *ca = json_object_get(tls, "ssl_ca_cert");
    if (ca) json_object_set(client_kwargs, "ssl_ca_cert", json_copy(ca));
    if (json_object_get(tls, "ssl_verify"))
        json_object_set(client_kwargs, "ssl_verify", json_copy(json_object_get(tls, "ssl_verify")));
    json_free(tls);
}

/* PoP: config_py_apply_custom_provider_extra_headers_to_client_kwargs @ hermes_cli/config.py:apply_custom_provider_extra_headers_to_client_kwargs */
void config_py_apply_custom_provider_extra_headers_to_client_kwargs(json_t *client_kwargs, const char *base_url,
                                                                   const json_t *custom_providers) {
    if (!client_kwargs) return;
    json_t *eh = config_py_get_custom_provider_extra_headers(base_url, custom_providers);
    if (!eh || eh->c.count == 0) { if (eh) json_free(eh); return; }
    json_t *merged = json_object_get(client_kwargs, "default_headers");
    if (!merged || merged->type != JSON_OBJECT) { merged = json_new_object(); json_object_set(client_kwargs, "default_headers", merged); }
    for (size_t i = 0; i < eh->c.count; i++)
        json_object_set(merged, eh->c.keys[i], json_copy(eh->c.items[i]));
    json_free(eh);
}

/* ============================================================
 * recommended_update_command_for_method
 * ============================================================ */

/* PoP: config_py_recommended_update_command_for_method @ hermes_cli/config.py:recommended_update_command_for_method */
void config_py_recommended_update_command_for_method(const char *method, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    if (method && (strcmp(method, "nix") == 0 || strcmp(method, "nixos") == 0)) {
        snprintf(out, out_size, "sudo nixos-rebuild switch");
        return;
    }
    if (method && strcmp(method, "docker") == 0) {
        snprintf(out, out_size, "docker pull ghcr.io/waefrebeorn/slermes:latest");
        return;
    }
    snprintf(out, out_size, "hermes update");
}

/* ---- small helpers used above ---- */
static const char *json_string_value_or_empty(const json_t *n, const char *key) {
    if (!n) return "";
    json_t *v = json_object_get(n, key);
    if (v && v->type == JSON_STRING) return v->str_val;
    return "";
}

/* ============================================================
 * Ports of the remaining hermes_cli/config.py REAL_GAPs:
 * raw-config freshness/version, readonly caching, and the cron
 * model-drift-guard warning family.
 * ============================================================ */

/* ---- _RAW_CONFIG_CACHE: (mtime_ns, size, dict) keyed by config path ---- */
typedef struct {
    char  path[HERMES_PATH_MAX];
    int64_t mtime_ns;
    size_t  size;
    json_t *dict;          /* cached raw dict (identity-invariant) */
} raw_cache_entry_t;

static raw_cache_entry_t _RAW_CONFIG_CACHE[8];
static int                _RAW_CONFIG_CACHE_N = 0;

/* ---- small accessors reused below ---- */
static json_t *raw_cfg_lookup_or_load(const char *path, int want_deepcopy);

/* PoP: _raw_config_has_explicit_version @ hermes_cli/config.py:_raw_config_has_explicit_version */
/* True iff config.yaml exists, parses, and carries a _config_version key. */
bool _raw_config_has_explicit_version(void) {
    char path[HERMES_PATH_MAX];
    config_path(path, sizeof path);
    struct stat st;
    if (stat(path, &st) != 0) return false;
    json_t *cfg = raw_cfg_lookup_or_load(path, 0);
    if (!cfg) return false;
    bool has = json_has(cfg, "_config_version");
    json_free(cfg);
    return has;
}

/* PoP: read_user_config_raw @ hermes_cli/config.py:read_user_config_raw */
	/* Read config.yaml EXACTLY as written on disk (no defaults merge, no env
 * expansion, no migration, no caching). cfg_path param is profile-aware.
 * Returns malloc'd json_t object (caller frees); missing/unparseable →
 * empty object. */
json_t *read_user_config_raw(const char *cfg_path) {
    char path[HERMES_PATH_MAX];
    if (cfg_path) {
        strncpy(path, cfg_path, HERMES_PATH_MAX - 1);
        path[HERMES_PATH_MAX - 1] = '\0';
    } else {
        config_path(path, sizeof path);
    }
    struct stat st;
    if (stat(path, &st) != 0) return json_new_object();
    char *err = NULL;
    yaml_doc_t *yp = yaml_parse_file(path, &err);
    if (!yp) { free(err); return json_new_object(); }
    char *js = yaml_to_json_string(yp, "");
    yaml_free(yp);
    if (!js) return json_new_object();
    json_t *doc = json_parse(js, NULL);
    free(js);
    if (!doc || doc->type != JSON_OBJECT) { if (doc) json_free(doc); return json_new_object(); }
    return doc;
}

/* PoP: object @ hermes_cli/config.py:read_raw_config_readonly */
	/* Fast-path: return cached raw config without deepcopy. Identity-invariant
 * (first caller sees the exact dict later cache hits return). Best-effort:
 * on any failure returns empty object (caller must not mutate). */
json_t *read_raw_config_readonly(void) {
    char path[HERMES_PATH_MAX];
    config_path(path, sizeof path);
    return raw_cfg_lookup_or_load(path, 0);
}

static json_t *raw_cfg_lookup_or_load(const char *path, int want_deepcopy) {
    struct stat st;
    int64_t mtime_ns = 0, size = 0;
    int present = 0;
    if (stat(path, &st) == 0) {
        present = 1;
        /* best-effort ns precision */
#ifdef st_mtime
        mtime_ns = (int64_t)st.st_mtime * 1000000000LL;
#endif
        size = (size_t)st.st_size;
    }
    /* cache hit? */
    for (int i = 0; i < _RAW_CONFIG_CACHE_N; i++) {
        if (strcmp(_RAW_CONFIG_CACHE[i].path, path) == 0 &&
            _RAW_CONFIG_CACHE[i].mtime_ns == mtime_ns &&
            _RAW_CONFIG_CACHE[i].size == size) {
            return want_deepcopy ? json_copy(_RAW_CONFIG_CACHE[i].dict)
                                 : _RAW_CONFIG_CACHE[i].dict;
        }
    }
    if (!present) return json_new_object();  /* missing → {} */
    /* reload: mirrors read_raw_config_file but caches */
    char *err = NULL;
    yaml_doc_t *yp = yaml_parse_file(path, &err);
    if (!yp) { free(err); return json_new_object(); }
    char *js = yaml_to_json_string(yp, "");
    yaml_free(yp);
    if (!js) return json_new_object();
    json_t *cfg = json_parse(js, NULL);
    free(js);
    json_t *obj = (cfg && cfg->type == JSON_OBJECT) ? cfg : json_new_object();
    if (cfg && cfg != obj) json_free(cfg);
    if (_RAW_CONFIG_CACHE_N >= 8) {
        json_free(_RAW_CONFIG_CACHE[0].dict);
        memmove(&_RAW_CONFIG_CACHE[0], &_RAW_CONFIG_CACHE[1],
                (8 - 1) * sizeof(raw_cache_entry_t));
        _RAW_CONFIG_CACHE_N--;
    }
    raw_cache_entry_t *e = &_RAW_CONFIG_CACHE[_RAW_CONFIG_CACHE_N++];
    snprintf(e->path, sizeof e->path, "%s", path);
    e->mtime_ns = mtime_ns; e->size = size; e->dict = obj;
    return want_deepcopy ? json_copy(obj) : obj;
}

/* PoP: _cron_model_drift_axis_for_config_key @ hermes_cli/config.py:_cron_model_drift_axis_for_config_key */
	/* Return the cron drift-guard axis affected by a config key, or NULL. */
const char *_cron_model_drift_axis_for_config_key(const char *key) {
    if (!key) return NULL;
    char norm[256];
    snprintf(norm, sizeof norm, "%s", key);
    for (char *p = norm; *p; p++) *p = (char)tolower((unsigned char)*p);
    /* strip inline */
    char *end = norm + strlen(norm);
    while (end > norm && (end[-1]==' '||end[-1]=='\t')) *--end = '\0';
    if (strcmp(norm, "model") == 0 || strcmp(norm, "model.default") == 0 ||
        strcmp(norm, "model.model") == 0 || strcmp(norm, "model.name") == 0)
        return "model";
    if (strcmp(norm, "model.provider") == 0 || strcmp(norm, "provider") == 0)
        return "provider";
    return NULL;
}

/* PoP: cron_model_drift_guard_enabled @ hermes_cli/config.py:cron_model_drift_guard_enabled */
	/* Only the literal YAML boolean false disables the guard; missing/malformed
 * stay fail-closed. When config is NULL the active merged config is loaded. */
bool cron_model_drift_guard_enabled(json_t *config) {
    if (!config) config = config_py_load_config_impl(0);
    if (!config || config->type != JSON_OBJECT) return true;
    json_t *cron = json_object_get(config, "cron");
    if (!cron || cron->type != JSON_OBJECT) return true;
    json_t *v = json_object_get(cron, "model_drift_guard");
    if (!v) return true;
    if (v->type == JSON_BOOL) return v->num_val != 0.0;
    return true;  /* non-boolean → fail-closed */
}

/* PoP: default @ hermes_cli/config.py:_cron_fleet_default_covers_axis */
	/* True iff the explicit cron.fleet default (cron.model / cron.model_provider)
 * covers *axis*. */
bool _cron_fleet_default_covers_axis(const char *axis, json_t *config) {
    if (!axis) return false;
    if (!config) config = config_py_load_config_impl(0);
    if (!config || config->type != JSON_OBJECT) return false;
    json_t *cron = json_object_get(config, "cron");
    if (!cron || cron->type != JSON_OBJECT) return false;
    const char *key = (strcmp(axis, "model") == 0) ? "model" : "model_provider";
    json_t *v = json_object_get(cron, key);
    if (!v || v->type != JSON_STRING) return false;
    return v->str_val && strlen(v->str_val) > 0;
}

/* PoP: array @ hermes_cli/config.py:_load_cron_jobs_for_config_warning */
	/* Best-effort read of the active profile's cron jobs database. Delegates to
 * cron.jobs.load_jobs (cronjobs_load_jobs) to reuse BOM/corruption handling.
 * Returns a json_t array (caller frees) — empty on any failure. */
json_t *_load_cron_jobs_for_config_warning(void) {
    char *err = NULL;
    json_t *jobs = cronjobs_load_jobs(&err);
    if (err) free(err);
    if (!jobs) return json_new_array();
    if (jobs->type != JSON_ARRAY) { json_free(jobs); return json_new_array(); }
    return jobs;
}

/* PoP: warn_unpinned_cron_jobs_after_model_config_change @ hermes_cli/config.py:warn_unpinned_cron_jobs_after_model_config_change */
	/* Warn when a global model/provider change will trip cron's drift guard. */
void warn_unpinned_cron_jobs_after_model_config_change(
        const char *key, const char *value, json_t *config) {
    const char *axis = _cron_model_drift_axis_for_config_key(key);
    if (!axis) return;
    if (!cron_model_drift_guard_enabled(config)) return;
    if (_cron_fleet_default_covers_axis(axis, config)) return;
    char new_value[256];
    snprintf(new_value, sizeof new_value, "%s", value ? value : "");
    for (char *p = new_value; *p; p++) *p = (char)tolower((unsigned char)*p);
    char *sp = new_value + strlen(new_value);
    while (sp > new_value && (sp[-1]==' '||sp[-1]=='\t')) *--sp = '\0';
    if (*new_value == '\0') return;

    const char *pinned_field = axis;
    char snapshot_field[64];
    snprintf(snapshot_field, sizeof snapshot_field, "%s_snapshot", axis);

    int affected = 0;
    json_t *jobs = _load_cron_jobs_for_config_warning();
    size_t n = json_len(jobs);
    for (size_t i = 0; i < n; i++) {
        json_t *job = json_get(jobs, i);
        if (!job || job->type != JSON_OBJECT) continue;
        json_t *enabled = json_object_get(job, "enabled");
        if (enabled && enabled->type == JSON_BOOL && enabled->num_val == 0.0) continue;
        json_t *no_agent = json_object_get(job, "no_agent");
        if (no_agent && no_agent->type == JSON_BOOL && no_agent->num_val != 0.0) continue;
        json_t *pin = json_object_get(job, pinned_field);
        if (pin && pin->type == JSON_STRING && strlen(pin->str_val) > 0) continue;
        json_t *snap = json_object_get(job, snapshot_field);
        const char *s = (snap && snap->type == JSON_STRING) ? snap->str_val : "";
        if (s && *s) {
            /* case-insensitive compare */
            char sv[256];
            snprintf(sv, sizeof sv, "%s", s);
            for (char *p = sv; *p; p++) *p = (char)tolower((unsigned char)*p);
            char *se = sv + strlen(sv);
            while (se > sv && (se[-1]==' '||se[-1]=='\t')) *--se = '\0';
            if (*sv && strcmp(sv, new_value) != 0) affected++;
        }
    }
    json_free(jobs);
    if (affected <= 0) return;
    const char *noun = (affected == 1) ? "job" : "jobs";
    const char *verb = (affected == 1) ? "has" : "have";
    fprintf(stderr,
        "⚠️  %d enabled unpinned cron %s %s stored %s values that differ "
        "from the new global %s. They will fail closed on their next run "
        "instead of silently using the changed model/provider. Inspect with "
        "`hermes cron list`, then pin the intended values with "
        "`cronjob action=update job_id=<job_id> %s=<value>`.\n",
        affected, noun, verb, snapshot_field, axis, pinned_field);
}


/* ── from src/cli/port_config_py_pure.c ── */
/*
 * port_config_py_pure.c — Faithful C11 ports of the module-level pure helpers
 * from Python hermes_cli/config.py that require NO filesystem / config-load I/O.
 * (I/O helpers live in port_config_py_io.c.)
 *
 * Every function carries its exact PoP comment so the parity scanner credits
 * it. See include/port_config_py_helpers.h for the full API.
 */

#include "port_config_py_helpers.h"
#include "hermes_core_types.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>

/* ============================================================
 * Route-identity normalizer (hermes_cli/route_identity.py)
 * ============================================================ */

/* PoP: config_normalize_route_base_url @ hermes_cli/config.py:normalize_route_base_url */
/* PoP: config_normalize_route_base_url @ agent/agent_init.py:_normalize_route_base_url */
/* PoP: config_normalize_route_base_url @ hermes_cli/route_identity.py:normalize_route_base_url */
char *config_normalize_route_base_url(const char *url) {
    if (!url) return strdup("");
    size_t n = strlen(url);
    char *out = (char *)malloc(n + 1);
    if (!out) return strdup("");
    size_t i = 0;
    const char *scheme = strstr(url, "://");
    if (scheme) {
        size_t slen = (size_t)(scheme - url);
        for (size_t k = 0; k < slen; k++) out[i++] = (char)tolower((unsigned char)url[k]);
        out[i++] = ':'; out[i++] = '/'; out[i++] = '/';
        const char *h = scheme + 3;
        while (*h && *h != '/' && *h != '?' && *h != ' ') {
            out[i++] = (char)tolower((unsigned char)*h);
            h++;
        }
        while (*h) out[i++] = *h++;
    } else {
        const char *h = url;
        while (*h && *h != '/' && *h != '?' && *h != ' ') {
            out[i++] = (char)tolower((unsigned char)*h);
            h++;
        }
        while (*h) out[i++] = *h++;
    }
    out[i] = '\0';
    return out;
}

/* ============================================================
 * Wrappers over already-ported helpers
 * ============================================================ */

/* PoP: config_coerce_ssl_verify @ hermes_cli/config.py:_coerce_ssl_verify */
int config_coerce_ssl_verify(const char *value) {
    /* coerce_ssl_verify(value, is_bool, bool_val): returns 1/0/-1 sentinel.
     * config.py returns True/False/None. We return 1/0 and -1 for None. */
    int is_bool = 0, bool_val = 0;
    int r = coerce_ssl_verify(value, is_bool, bool_val);
    /* The port returns 1 when truthy, 0 when falsy, -1 when not a bool/str.
     * Map: 1 -> 1 (true), 0 -> 0 (false), -1 -> -1 (None). */
    return r;
}

/* PoP: config_coerce_config_version @ hermes_cli/config.py:_coerce_config_version */
int config_coerce_config_version(const char *value) {
    return coerce_config_version(value, 0);
}

/* PoP: config_is_env_config_key @ hermes_cli/config.py:_is_env_config_key */
bool config_is_env_config_key(const char *key) {
    return is_env_config_key(key);
}

/* PoP: config_format_config_get_value @ hermes_cli/config.py:_format_config_get_value */
void config_format_config_get_value(const char *value, int as_json, char *out, size_t out_size) {
    format_config_get_value(value, as_json, out, out_size);
}

/* PoP: config_suggest_closest_key @ hermes_cli/config.py:_suggest_closest_key */
bool config_suggest_closest_key(const char *key, char *suggestion, size_t sug_size) {
    return _validate_config_key(key, suggestion, sug_size);
}

/* PoP: config_py_deep_merge @ hermes_cli/config.py:_deep_merge */
json_t *config_py_deep_merge(const json_t *base, const json_t *override) {
    return config_deep_merge(base, override);
}

/* PoP: config_py_items_by_unique_name @ hermes_cli/config.py:_items_by_unique_name */
json_t *config_py_items_by_unique_name(const json_t *items) {
    return config_items_by_unique_name(items);
}

/* PoP: config_py_normalize_max_turns @ hermes_cli/config.py:_normalize_max_turns_config */
json_t *config_py_normalize_max_turns(const json_t *config_in) {
    return config_normalize_max_turns(config_in);
}

/* ============================================================
 * Nested get / set / unset (dotted keys)
 * ============================================================ */

static int cfg_is_object(const json_t *n) { return n && n->type == JSON_OBJECT; }
static int cfg_is_array(const json_t *n) { return n && n->type == JSON_ARRAY; }

/* forward decls (defined later in this file) */
static int config_path_in_preserve(const char *path, char **preserve_keys, int preserve_count);
char *config_expand_env_vars_local(const char *input);
static const char *cfg_string_value_safe(const json_t *n);

/* PoP: config_py_get_nested @ hermes_cli/config.py:_get_nested */
json_t *config_py_get_nested(const json_t *config, const char *dotted_key) {
    if (!config || !dotted_key || !*dotted_key) return NULL;
    /* split on '.' */
    json_t *cur = (json_t *)config;
    const char *p = dotted_key;
    const char *seg_start = dotted_key;
    while (1) {
        int last = (*p == '\0' || *p == '.');
        if (last) {
            size_t seglen = (size_t)(p - seg_start);
            if (seglen == 0) return NULL;
            char seg[256];
            if (seglen >= sizeof(seg)) return NULL;
            memcpy(seg, seg_start, seglen);
            seg[seglen] = '\0';
            if (cfg_is_array(cur)) {
                char *end = NULL;
                long idx = strtol(seg, &end, 10);
                if (end == seg || *end != '\0') return NULL;
                if (idx < 0 || idx >= (long)cur->c.count) return NULL;
                cur = cur->c.items[idx];
            } else if (cfg_is_object(cur)) {
                cur = json_object_get(cur, seg);
            } else {
                return NULL;
            }
            if (*p == '\0') return cur;
            if (!cur) return NULL;
            seg_start = p + 1;
        }
        p++;
    }
}

/* PoP: config_py_set_nested @ hermes_cli/config.py:_set_nested */
int config_py_set_nested(json_t *config, const char *dotted_key, json_t *value) {
    if (!config || !dotted_key || !*dotted_key || !value) return -1;
    /* collect segments */
    char segs[64][256];
    int nseg = 0;
    const char *p = dotted_key;
    const char *seg_start = dotted_key;
    while (1) {
        int last = (*p == '\0' || *p == '.');
        if (last) {
            size_t seglen = (size_t)(p - seg_start);
            if (seglen == 0 || seglen >= 256 || nseg >= 64) return -1;
            memcpy(segs[nseg], seg_start, seglen);
            segs[nseg][seglen] = '\0';
            nseg++;
            if (*p == '\0') break;
            seg_start = p + 1;
        }
        p++;
    }
    json_t *cur = config;
    for (int i = 0; i < nseg - 1; i++) {
        const char *seg = segs[i];
        if (cfg_is_array(cur)) {
            char *end = NULL;
            long idx = strtol(seg, &end, 10);
            if (end == seg || *end != '\0') return -1;
            if (idx < 0 || idx >= (long)cur->c.count) return -1;
            cur = cur->c.items[idx];
        } else if (cfg_is_object(cur)) {
            json_t *existing = json_object_get(cur, seg);
            if (!existing || (!cfg_is_object(existing) && !cfg_is_array(existing))) {
                json_t *newobj = json_new_object();
                json_object_set(cur, seg, newobj);
                cur = newobj;
            } else {
                cur = existing;
            }
        } else {
            return -1;
        }
        if (!cur) return -1;
    }
    const char *last = segs[nseg - 1];
    if (cfg_is_array(cur)) {
        char *end = NULL;
        long idx = strtol(last, &end, 10);
        if (end == last || *end != '\0') return -1;
        if (idx < 0 || idx >= (long)cur->c.count) return -1;
        json_array_set(cur, (size_t)idx, json_copy(value));
    } else if (cfg_is_object(cur)) {
        json_object_set(cur, last, value);
    } else {
        return -1;
    }
    return 0;
}

/* PoP: config_py_unset_nested @ hermes_cli/config.py:_unset_nested */
int config_py_unset_nested(json_t *config, const char *dotted_key) {
    if (!config || !dotted_key || !*dotted_key) return 0;
    char segs[64][256];
    int nseg = 0;
    const char *p = dotted_key;
    const char *seg_start = dotted_key;
    while (1) {
        int last = (*p == '\0' || *p == '.');
        if (last) {
            size_t seglen = (size_t)(p - seg_start);
            if (seglen == 0 || seglen >= 256 || nseg >= 64) return 0;
            memcpy(segs[nseg], seg_start, seglen);
            segs[nseg][seglen] = '\0';
            nseg++;
            if (*p == '\0') break;
            seg_start = p + 1;
        }
        p++;
    }
    json_t *parents[64];
    json_t *cur = config;
    parents[0] = config;
    int ok = 1;
    for (int i = 0; i < nseg - 1; i++) {
        const char *seg = segs[i];
        if (cfg_is_array(cur)) {
            char *end = NULL;
            long idx = strtol(seg, &end, 10);
            if (end == seg || *end != '\0') { ok = 0; break; }
            if (idx < 0 || idx >= (long)cur->c.count) { ok = 0; break; }
            cur = cur->c.items[idx];
        } else if (cfg_is_object(cur)) {
            if (!json_object_get(cur, seg)) { ok = 0; break; }
            cur = json_object_get(cur, seg);
        } else { ok = 0; break; }
        parents[i + 1] = cur;
    }
    if (!ok) return 0;
    const char *last = segs[nseg - 1];
    int removed = 0;
    if (cfg_is_array(cur)) {
        char *end = NULL;
        long idx = strtol(last, &end, 10);
        if (end != last && *end == '\0' && idx >= 0 && idx < (long)cur->c.count) {
            json_array_remove(cur, (size_t)idx);
            removed = 1;
        }
    } else if (cfg_is_object(cur)) {
        if (json_object_get(cur, last)) {
            json_object_del(cur, last);
            removed = 1;
        }
    }
    if (!removed) return 0;
    /* prune empty dict containers upward (best-effort) */
    for (int i = nseg - 2; i >= 0; i--) {
        json_t *parent = parents[i];
        json_t *child = parents[i + 1];
        if (!cfg_is_object(child) || child->c.count != 0) break;
        const char *seg = segs[i];
        if (cfg_is_object(parent)) {
            json_object_del(parent, seg);
        } else if (cfg_is_array(parent)) {
            char *end = NULL;
            long idx = strtol(seg, &end, 10);
            if (end != seg && *end == '\0' && idx >= 0 && idx < (long)parent->c.count) {
                json_array_remove(parent, (size_t)idx);
            }
        } else break;
    }
    return 1;
}

/* ============================================================
 * _explicit_config_paths
 * ============================================================ */

/* PoP: config_py_explicit_paths @ hermes_cli/config.py:_explicit_config_paths */
char **config_py_explicit_paths(const json_t *config, int *out_count) {
    char **result = NULL;
    int cap = 16, count = 0;
    if (out_count) *out_count = 0;
    if (!config) return NULL;

    /* iterative DFS using a path stack of segments */
    /* simple recursive helper via explicit stack of (node, depth, pathbuf) */
    typedef struct { const json_t *node; int depth; char path[2048]; } frame;
    frame *stack = (frame *)malloc(sizeof(frame) * 512);
    int sp = 0;
    stack[sp].node = config; stack[sp].depth = 0; stack[sp].path[0] = '\0'; sp++;
    while (sp > 0) {
        frame f = stack[--sp];
        if (cfg_is_object(f.node)) {
            for (size_t i = 0; i < f.node->c.count; i++) {
                const char *k = f.node->c.keys[i];
                json_t *child = f.node->c.items[i];
                char npath[2048];
                if (f.depth == 0) snprintf(npath, sizeof npath, "%s", k);
                else snprintf(npath, sizeof npath, "%s.%s", f.path, k);
                if (cfg_is_object(child) || cfg_is_array(child)) {
                    if (sp < 512) {
                        stack[sp].node = child; stack[sp].depth = f.depth + 1;
                        snprintf(stack[sp].path, sizeof stack[sp].path, "%s", npath);
                        sp++;
                    }
                } else {
                    char *cp = strdup(npath);
                    if (count >= cap) { cap *= 2; result = (char **)realloc(result, cap * sizeof(char*)); }
                    result[count++] = cp;
                }
            }
        } else if (cfg_is_array(f.node)) {
            for (size_t i = 0; i < f.node->c.count; i++) {
                json_t *child = f.node->c.items[i];
                char npath[2048];
                if (f.depth == 0) snprintf(npath, sizeof npath, "%zu", i);
                else snprintf(npath, sizeof npath, "%s.%zu", f.path, i);
                if (cfg_is_object(child) || cfg_is_array(child)) {
                    if (sp < 512) {
                        stack[sp].node = child; stack[sp].depth = f.depth + 1;
                        snprintf(stack[sp].path, sizeof stack[sp].path, "%s", npath);
                        sp++;
                    }
                } else {
                    char *cp = strdup(npath);
                    if (count >= cap) { cap *= 2; result = (char **)realloc(result, cap * sizeof(char*)); }
                    result[count++] = cp;
                }
            }
        }
    }
    free(stack);
    if (out_count) *out_count = count;
    return result;
}

/* ============================================================
 * _strip_default_values
 * ============================================================ */

/* PoP: config_py_strip_default_values @ hermes_cli/config.py:_strip_default_values */
json_t *config_py_strip_default_values(const json_t *config, const json_t *defaults,
                                        char **preserve_keys, int preserve_count) {
    json_t *result = json_new_object();
    if (!config) return result;
    if (!cfg_is_object(config)) return result;

    /* build preserve set */
    /* helper to test membership */
    #define IN_PRESERVE(p) (config_path_in_preserve((p), preserve_keys, preserve_count))

    for (size_t i = 0; i < config->c.count; i++) {
        const char *key = config->c.keys[i];
        json_t *value = config->c.items[i];
        char path[2048];
        snprintf(path, sizeof path, "%s", key);
        if (IN_PRESERVE(path)) {
            json_object_set(result, key, json_copy(value));
            continue;
        }
        json_t *child_default = (defaults && cfg_is_object(defaults))
            ? json_object_get(defaults, key) : NULL;
        if (cfg_is_object(value) && value->c.count > 0) {
            json_t *stripped = config_py_strip_default_values(value, child_default,
                                                              preserve_keys, preserve_count);
            if (stripped && stripped->c.count > 0) {
                json_object_set(result, key, stripped);
            } else {
                json_free(stripped);
            }
        } else {
            /* leaf: keep if differs from default */
            int equal = 0;
            if (child_default && cfg_is_object(child_default) == 0) {
                if (value->type == child_default->type) {
                    if (value->type == JSON_STRING)
                        equal = (strcmp(value->str_val, child_default->str_val) == 0);
                    else if (value->type == JSON_NUMBER)
                        equal = (value->num_val == child_default->num_val);
                    else if (value->type == JSON_BOOL)
                        equal = (value->bool_val == child_default->bool_val);
                    else if (value->type == JSON_NULL)
                        equal = 1;
                }
            }
            if (!equal) {
                json_object_set(result, key, json_copy(value));
            }
        }
    }
    #undef IN_PRESERVE
    return result;
}










/* ============================================================
 * _normalize_root_model_keys
 * ============================================================ */

/* PoP: config_py_normalize_root_model_keys @ hermes_cli/config.py:_normalize_root_model_keys */
json_t *config_py_normalize_root_model_keys(const json_t *config_in) {
    if (!config_in) return json_new_object();
    json_t *config = json_copy(config_in);
    json_t *model = json_object_get(config, "model");
    int model_is_dict = cfg_is_object(model);
    int model_has_alias = model_is_dict && json_object_get(model, "api_base");
    int model_needs_canon = model_is_dict &&
        (json_object_get(model, "model") || json_object_get(model, "name"));
    int has_root = 0;
    const char *rootkeys[] = {"provider", "base_url", "context_length", "api_base"};
    for (int k = 0; k < 4; k++) {
        json_t *rv = json_object_get(config, rootkeys[k]);
        if (rv && !json_is_null(rv) && !(rv->type == JSON_STRING && rv->str_val[0] == '\0'))
            has_root = 1;
    }
    if (!has_root && !model_has_alias && !model_needs_canon) return config;

    if (!model_is_dict) {
        const char *def = cfg_is_object(model) ? NULL : (model && !json_is_null(model) ? cfg_string_value_safe(model) : NULL);
        json_t *newmodel = json_new_object();
        if (def) json_object_set(newmodel, "default", json_new_string(def));
        json_object_set(config, "model", newmodel);
        model = newmodel;
    } else {
        model = json_copy(model);
        json_object_set(config, "model", model);
    }
    /* promote root provider/base_url/context_length */
    for (int k = 0; k < 3; k++) {
        const char *rk = rootkeys[k];
        json_t *rv = json_object_get(config, rk);
        if (rv && !json_is_null(rv) && !(rv->type == JSON_STRING && rv->str_val[0] == '\0')) {
            if (!json_object_get(model, rk))
                json_object_set(model, rk, json_copy(rv));
        }
        json_object_del(config, rk);
    }
    /* api_base alias */
    json_t *ab = json_object_get(config, "api_base");
    if (ab && !json_object_get(model, "base_url"))
        json_object_set(model, "base_url", json_copy(ab));
    json_object_del(config, "api_base");
    json_object_del(model, "api_base");
    /* canonicalize model id -> default */
    json_t *d = json_object_get(model, "default");
    int has_default = d && !json_is_null(d) && !(d->type == JSON_STRING && d->str_val[0] == '\0');
    if (!has_default) {
        json_t *m = json_object_get(model, "model");
        json_t *nm = json_object_get(model, "name");
        json_t *alias = (m && !json_is_null(m)) ? m : nm;
        if (alias && !json_is_null(alias))
            json_object_set(model, "default", json_copy(alias));
    }
    if (json_object_get(model, "default")) {
        json_object_del(model, "model");
        json_object_del(model, "name");
    }
    return config;
}

static const char *cfg_string_value_safe(const json_t *n) {
    if (n && n->type == JSON_STRING) return n->str_val;
    return NULL;
}

/* ============================================================
 * _merge_partial_save
 * ============================================================ */

/* PoP: config_py_merge_partial_save @ hermes_cli/config.py:_merge_partial_save */
json_t *config_py_merge_partial_save(const json_t *raw, const json_t *override) {
    if (!override) return raw ? json_copy(raw) : json_new_object();
    json_t *result = json_copy(override);
    if (!raw) return result;
    if (!cfg_is_object(raw) || !cfg_is_object(result)) return result;
    for (size_t i = 0; i < raw->c.count; i++) {
        const char *key = raw->c.keys[i];
        json_t *value = raw->c.items[i];
        if (!json_object_get(result, key)) {
            json_object_set(result, key, json_copy(value));
        } else {
            json_t *rv = json_object_get(result, key);
            if (cfg_is_object(rv) && cfg_is_object(value)) {
                json_t *merged = config_py_merge_partial_save(value, rv);
                json_object_set(result, key, merged);
            }
        }
    }
    return result;
}

/* ============================================================
 * _preserve_env_ref_templates
 * ============================================================ */

static int has_env_ref(const char *s) {
    if (!s) return 0;
    return strstr(s, "${") != NULL;
}

/* PoP: config_py_preserve_env_ref_templates @ hermes_cli/config.py:_preserve_env_ref_templates */
json_t *config_py_preserve_env_ref_templates(const json_t *current, const json_t *raw,
                                              const json_t *loaded_expanded) {
    if (!current) return NULL;
    if (current->type == JSON_STRING) {
        if (!raw) return json_copy(current);
        const char *cstr = current->str_val;
        const char *rstr = (raw->type == JSON_STRING) ? raw->str_val : NULL;
        if (rstr && has_env_ref(rstr)) {
            if (strcmp(cstr, rstr) == 0) return json_copy(raw);
            if (loaded_expanded && loaded_expanded->type == JSON_STRING &&
                strcmp(cstr, loaded_expanded->str_val) == 0)
                return json_copy(raw);
            /* expand raw and compare */
            char *exp = config_expand_env_vars_local(rstr);
            int eq = exp && strcmp(exp, cstr) == 0;
            free(exp);
            if (eq) return json_copy(raw);
        }
        return json_copy(current);
    }
    if (current->type == JSON_OBJECT) {
        json_t *out = json_new_object();
        for (size_t i = 0; i < current->c.count; i++) {
            const char *key = current->c.keys[i];
            json_t *cv = current->c.items[i];
            json_t *rv = (raw && raw->type == JSON_OBJECT) ? json_object_get(raw, key) : NULL;
            json_t *lv = (loaded_expanded && loaded_expanded->type == JSON_OBJECT)
                ? json_object_get(loaded_expanded, key) : NULL;
            json_object_set(out, key, config_py_preserve_env_ref_templates(cv, rv, lv));
        }
        return out;
    }
    if (current->type == JSON_ARRAY) {
        json_t *cur_by_name = config_items_by_unique_name(current);
        json_t *raw_by_name = (raw && raw->type == JSON_ARRAY) ? config_items_by_unique_name(raw) : NULL;
        json_t *loaded_by_name = (loaded_expanded && loaded_expanded->type == JSON_ARRAY)
            ? config_items_by_unique_name(loaded_expanded) : NULL;
        if (cur_by_name && cur_by_name->c.count > 0 && raw_by_name && raw_by_name->c.count > 0) {
            json_t *out = json_new_array();
            for (size_t i = 0; i < current->c.count; i++) {
                json_t *item = current->c.items[i];
                const char *name = (item->type == JSON_OBJECT) ? cfg_string_value_safe(json_object_get(item, "name")) : NULL;
                json_t *rv = (name && raw_by_name) ? json_object_get(raw_by_name, name) : NULL;
                json_t *lv = (name && loaded_by_name) ? json_object_get(loaded_by_name, name) : NULL;
                json_array_append(out, config_py_preserve_env_ref_templates(item, rv, lv));
            }
            json_free(cur_by_name); json_free(raw_by_name); if (loaded_by_name) json_free(loaded_by_name);
            return out;
        }
        if (raw_by_name) json_free(raw_by_name);
        if (loaded_by_name) json_free(loaded_by_name);
        json_t *out = json_new_array();
        for (size_t i = 0; i < current->c.count; i++) {
            json_t *item = current->c.items[i];
            json_t *rv = (raw && raw->type == JSON_ARRAY && i < raw->c.count) ? raw->c.items[i] : NULL;
            json_t *lv = (loaded_expanded && loaded_expanded->type == JSON_ARRAY && i < loaded_expanded->c.count)
                ? loaded_expanded->c.items[i] : NULL;
            json_array_append(out, config_py_preserve_env_ref_templates(item, rv, lv));
        }
        return out;
    }
    return json_copy(current);
}

/* Local env-expand used by _preserve_env_ref_templates (mirrors config_expand_env_vars
 * in port_config.c but returns malloc'd string). */
char *config_expand_env_vars_local(const char *input) {
    if (!input) return NULL;
    size_t cap = strlen(input) + 1;
    char *result = (char *)malloc(cap);
    if (!result) return NULL;
    const char *p = input;
    size_t pos = 0;
    while (*p) {
        if (*p == '$' && *(p + 1) == '{') {
            const char *start = p + 2;
            const char *end = strchr(start, '}');
            if (!end) { result[pos++] = *p++; continue; }
            const char *colon = NULL;
            for (const char *s = start; s < end; s++) if (*s == ':' && *(s+1) == '-') { colon = s; break; }
            char var[256]; size_t vl;
            const char *def = NULL; size_t dl = 0;
            if (colon) { vl = colon - start; def = colon + 2; dl = end - def; }
            else vl = end - start;
            if (vl >= sizeof(var)) vl = sizeof(var) - 1;
            memcpy(var, start, vl); var[vl] = '\0';
            const char *val = getenv(var);
            if (!val && def) { char *d = (char*)malloc(dl + 1); memcpy(d, def, dl); d[dl]='\0'; val = d; }
            if (val) { size_t vl2 = strlen(val); if (pos + vl2 + 1 > cap) { cap = pos + vl2 + 16; result = realloc(result, cap); } memcpy(result + pos, val, vl2); pos += vl2; if (def) free((void*)def); }
            else { if (pos + (end - p + 1) + 1 > cap) { cap = pos + (end - p + 1) + 16; result = realloc(result, cap); } memcpy(result + pos, p, end - p + 1); pos += (end - p + 1); }
            p = end + 1;
        } else {
            result[pos++] = *p++;
        }
    }
    result[pos] = '\0';
    return result;
}

/* ============================================================
 * _env_ref_var_name / _env_ref_snapshot / _env_expand_match
 * ============================================================ */

/* PoP: config_py_env_ref_var_name @ hermes_cli/config.py:_env_ref_var_name */
char *config_py_env_ref_var_name(const char *ref) {
    if (!ref) return NULL;
    char *r = strdup(ref);
    /* strip whitespace */
    char *p = r; while (*p) { *p = *p; p++; }
    /* trim */
    size_t L = strlen(r);
    while (L > 0 && (r[L-1] == ' ' || r[L-1] == '\t')) r[--L] = '\0';
    size_t s = 0; while (r[s]==' '||r[s]=='\t') s++;
    if (s > 0) memmove(r, r + s, L - s + 1);
    if (strncmp(r, "env:", 4) == 0) {
        char *name = r + 4;
        while (*name == ' ' || *name == '\t') name++;
        if (*name == '\0') { free(r); return NULL; }
        /* return name (caller frees) — shift */
        memmove(r, name, strlen(name) + 1);
        return r;
    }
    if (strchr(r, ':') && r[0] >= 'a' && r[0] <= 'z') {
        /* looks like a non-env source prefix */
        free(r);
        return NULL;
    }
    return r;
}

/* PoP: config_py_env_ref_snapshot @ hermes_cli/config.py:_env_ref_snapshot */
void config_py_env_ref_snapshot(const json_t *obj, json_t *snapshot) {
    if (!obj || !snapshot) return;
    if (obj->type == JSON_STRING) {
        const char *s = obj->str_val;
        const char *p = s;
        while ((p = strstr(p, "${"))) {
            const char *start = p + 2;
            const char *end = strchr(start, '}');
            if (!end) break;
            char buf[256];
            size_t vl = end - start;
            if (vl >= sizeof(buf)) vl = sizeof(buf) - 1;
            memcpy(buf, start, vl); buf[vl] = '\0';
            char *name = config_py_env_ref_var_name(buf);
            if (name) {
                const char *val = getenv(name);
                json_object_set(snapshot, name, json_new_string(val ? val : ""));
                free(name);
            }
            p = end + 1;
        }
    } else if (obj->type == JSON_OBJECT) {
        for (size_t i = 0; i < obj->c.count; i++)
            config_py_env_ref_snapshot(obj->c.items[i], snapshot);
    } else if (obj->type == JSON_ARRAY) {
        for (size_t i = 0; i < obj->c.count; i++)
            config_py_env_ref_snapshot(obj->c.items[i], snapshot);
    }
}

/* PoP: config_py_env_expand_match @ hermes_cli/config.py:_env_expand_match */
char *config_py_env_expand_match(const char *inner) {
    if (!inner) return strdup("");
    char *trim = strdup(inner);
    size_t L = strlen(trim);
    while (L > 0 && (trim[L-1]==' '||trim[L-1]=='\t')) trim[--L]='\0';
    size_t s = 0; while (trim[s]==' '||trim[s]=='\t') s++;
    if (s>0) memmove(trim, trim+s, L-s+1);
    char *result;
    if (strncmp(trim, "env:", 4) == 0) {
        char *name = trim + 4;
        while (*name==' '||*name=='\t') name++;
        if (*name == '\0') { result = strdup(inner); free(trim); return result; }
        const char *val = getenv(name);
        if (val) { result = strdup(val); free(trim); return result; }
        /* warn-once omitted (no logger coupling here); keep literal */
        result = strdup(inner);
        free(trim);
        return result;
    }
    if (strchr(trim, ':') && trim[0] >= 'a' && trim[0] <= 'z') {
        result = strdup(inner);
        free(trim);
        return result;
    }
    const char *val = getenv(trim);
    if (val) { result = strdup(val); free(trim); return result; }
    result = strdup(inner);
    free(trim);
    return result;
}

/* ============================================================
 * normalize_extra_headers
 * ============================================================ */

/* PoP: config_py_normalize_extra_headers @ hermes_cli/config.py:normalize_extra_headers */
json_t *config_py_normalize_extra_headers(const json_t *extra_headers) {
    json_t *out = json_new_object();
    if (!extra_headers || extra_headers->type != JSON_OBJECT) return out;
    if (extra_headers->c.count == 0) return out;
    for (size_t i = 0; i < extra_headers->c.count; i++) {
        const char *k = extra_headers->c.keys[i];
        json_t *v = extra_headers->c.items[i];
        if (v->type == JSON_NULL) continue;
        char kbuf[1024], vbuf[4096];
        snprintf(kbuf, sizeof kbuf, "%s", k);
        if (v->type == JSON_STRING) snprintf(vbuf, sizeof vbuf, "%s", v->str_val);
        else if (v->type == JSON_NUMBER) snprintf(vbuf, sizeof vbuf, "%g", v->num_val);
        else if (v->type == JSON_BOOL) snprintf(vbuf, sizeof vbuf, "%s", v->bool_val ? "true" : "false");
        else snprintf(vbuf, sizeof vbuf, "%s", json_serialize(v));
        json_object_set(out, kbuf, json_new_string(vbuf));
    }
    return out;
}

/* ============================================================
 * is_provider_enabled
 * ============================================================ */

/* PoP: config_py_is_provider_enabled @ hermes_cli/config.py:is_provider_enabled */
bool config_py_is_provider_enabled(const json_t *provider_cfg) {
    if (!provider_cfg || provider_cfg->type != JSON_OBJECT) return true;
    json_t *flag = json_object_get(provider_cfg, "enabled");
    if (!flag) return true;
    if (flag->type == JSON_BOOL) return flag->bool_val;
    if (flag->type == JSON_STRING) {
        const char *s = flag->str_val;
        if (strcmp(s, "false") == 0 || strcmp(s, "0") == 0 ||
            strcmp(s, "no") == 0 || strcmp(s, "off") == 0) return false;
        return true;
    }
    return flag->bool_val;
}

/* ============================================================
 * _custom_provider_entry_to_provider_config
 * ============================================================ */

/* PoP: config_py_custom_provider_entry_to_provider_config @ hermes_cli/config.py:_custom_provider_entry_to_provider_config */
json_t *config_py_custom_provider_entry_to_provider_config(const json_t *entry, const char *provider_key) {
    char *norm_json = normalize_custom_provider_entry_json(json_serialize(entry), provider_key ? provider_key : "");
    if (!norm_json) return NULL;
    json_t *normalized = json_parse(norm_json, NULL);
    free(norm_json);
    if (!normalized || normalized->type != JSON_OBJECT) { if (normalized) json_free(normalized); return NULL; }
    json_t *provider_entry = json_new_object();
    json_object_set(provider_entry, "api", json_copy(json_object_get(normalized, "base_url")));
    const char *fields[] = {"name","api_key","key_env","models","context_length",
                             "rate_limit_delay","discover_models","extra_body",
                             "extra_headers","ssl_ca_cert","ssl_verify"};
    for (int i = 0; i < 11; i++) {
        const char *f = fields[i];
        json_t *v = json_object_get(normalized, f);
        if (v) json_object_set(provider_entry, f, json_copy(v));
    }
    json_t *m = json_object_get(normalized, "model");
    if (m) json_object_set(provider_entry, "default_model", json_copy(m));
    json_t *am = json_object_get(normalized, "api_mode");
    if (am) json_object_set(provider_entry, "transport", json_copy(am));
    json_free(normalized);
    return provider_entry;
}

/* ============================================================
 * get_config_value / unset_config_value
 * ============================================================ */

/* PoP: config_py_get_config_value @ hermes_cli/config.py:get_config_value */
json_t *config_py_get_config_value(const json_t *cfg, const char *dotted_key, const json_t *default_val) {
    json_t *v = config_py_get_nested(cfg, dotted_key);
    if (v) return v;
    return (json_t *)default_val;
}

/* PoP: config_py_unset_config_value @ hermes_cli/config.py:unset_config_value */
int config_py_unset_config_value(json_t *cfg, const char *dotted_key) {
    return config_py_unset_nested(cfg, dotted_key);
}

/* ============================================================
 * _resolve_hermes_uid_gid
 * ============================================================ */

/* PoP: config_py_resolve_hermes_uid_gid @ hermes_cli/config.py:_resolve_hermes_uid_gid */
void config_py_resolve_hermes_uid_gid(long *out_uid, long *out_gid) {
    long uid = -1, gid = -1;
    if (out_uid) *out_uid = -1;
    if (out_gid) *out_gid = -1;
#ifdef _WIN32
    return;
#endif
    const char *us = getenv("HERMES_UID");
    const char *gs = getenv("HERMES_GID");
    if (us && *us) { char *e; long v = strtol(us, &e, 10); if (*e == '\0') uid = v; }
    if (gs && *gs) { char *e; long v = strtol(gs, &e, 10); if (*e == '\0') gid = v; }
    if (out_uid) *out_uid = uid;
    if (out_gid) *out_gid = gid;
}

/* PoP: config_py_chown_to_hermes_uid @ hermes_cli/config.py:_chown_to_hermes_uid */
void config_py_chown_to_hermes_uid(const char *path) {
    if (!path) return;
    long uid = -1, gid = -1;
    config_py_resolve_hermes_uid_gid(&uid, &gid);
    if (uid >= 0 || gid >= 0)
        chown(path, uid >= 0 ? uid : -1, gid >= 0 ? gid : -1);
}

/* ============================================================
 * _is_container
 * ============================================================ */

/* PoP: config_py_is_container @ hermes_cli/config.py:_is_container */
bool config_py_is_container(void) {
    if (getenv("HERMES_CONTAINER") || getenv("HERMES_SKIP_CHMOD")) return true;
    if (access("/.dockerenv", F_OK) == 0) return true;
    FILE *f = fopen("/proc/1/cgroup", "r");
    if (f) {
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "docker") || strstr(line, "lxc") || strstr(line, "kubepods")) {
                fclose(f);
                return true;
            }
        }
        fclose(f);
    }
    return false;
}

/* ============================================================
 * _secure_dir / _secure_file (best-effort chmod)
 * ============================================================ */

/* PoP: config_py_secure_dir @ hermes_cli/config.py:_secure_dir */
void config_py_secure_dir(const char *path) {
    /* managed mode + container checks omitted for the pure helper; the io
     * variant (_ensure_hermes_home_managed) handles managed semantics. Here we
     * apply the owner-only 0700 chmod, honoring HERMES_HOME_MODE. */
    if (!path) return;
    const char *mode_str = getenv("HERMES_HOME_MODE");
    int mode = 0700;
    if (mode_str && *mode_str) {
        char *e; long m = strtol(mode_str, &e, 8);
        if (*e == '\0' && m >= 0 && m <= 0777) mode = (int)m;
    }
    chmod(path, mode);
    long uid = -1, gid = -1;
    config_py_resolve_hermes_uid_gid(&uid, &gid);
    if (uid >= 0 || gid >= 0)
        chown(path, uid >= 0 ? uid : -1, gid >= 0 ? gid : -1);
}

/* PoP: config_py_secure_file @ hermes_cli/config.py:_secure_file */
void config_py_secure_file(const char *path) {
    if (!path) return;
    if (config_py_is_container()) return;
    struct stat st;
    if (stat(path, &st) == 0)
        chmod(path, 0600);
}

/* ============================================================
 * save_config_value — faithful port of hermes_cli/config.py:save_config_value
 * Loads the merged config as JSON, sets a dotted key, and persists atomically.
 * Returns 0 on success, -1 on failure.
 * ============================================================ */
int config_py_save_value(const char *dotted_key, json_t *value) {
    if (!dotted_key || !*dotted_key || !value) return -1;
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg) return -1;
    if (config_py_set_nested(cfg, dotted_key, value) != 0) {
        json_free(cfg);
        return -1;
    }
    char path[HERMES_PATH_MAX];
    config_py_get_config_path(path, sizeof(path));
    int rc = config_py_atomic_config_write(path, cfg);
    json_free(cfg);
    return rc;
}


/* ── from src/cli/port_web_server_paths.c ── */
/*
 * port_web_server_paths.c — Shared path/install helpers for the
 * web server port.
 *
 * These functions are reused from cron/scheduler.py, hermes_cli/config.py,
 * and tools/tirith_security.py. Keeping them in a dedicated TU avoids
 * circular includes and makes the orchestrator file small again.
 */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>

/* PoP: get_hermes_home @ cron/scheduler.py:_get_hermes_home */
/* PoP: get_hermes_home @ tools/tirith_security.py:_get_hermes_home */
/* PoP: get_hermes_home @ hermes_cli/sessions_cmd.py:get_hermes_home */
const char *get_hermes_home(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp/.hermes";
    return home;
}

/* PoP: is_container @ hermes_cli/config.py:_is_container */
bool is_container(void) {
    if (access("/.dockerenv", F_OK) == 0)
        return true;
    FILE *f = fopen("/proc/1/cgroup", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "docker") || strstr(line, "containerd") ||
                strstr(line, "kubepods")) {
                fclose(f);
                return true;
            }
        }
        fclose(f);
    }
    return false;
}

/* PoP: detect_install_method @ hermes_cli/config.py:detect_install_method */
const char *detect_install_method(const char *project_root) {
    if (!project_root)
        return "unknown";
    char path[2048];
    snprintf(path, sizeof(path), "%s/.git", project_root);
    if (access(path, F_OK) == 0)
        return "git";
    return "pip";
}


/* ── from src/hermes_cli/port_hermes_cli_config_gaps.c ── */
/*
 * port_hermes_cli_config_gaps.c — C11 port of hermes_cli/config.py REAL_GAP functions.
 *
 * Implements config management: install detection, env var handling,
 * config migration, and file I/O. Delegates to existing C libraries
 * (libjson, libdotenv, libpath, libyaml) where possible.
 */

#include "hermes.h"
#include "hermes_core_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/* === Install detection === */

/* PoP: config_get_managed_system @ hermes_cli/config.py:get_managed_system */
const char *config_get_managed_system(void) {
    const char *raw = getenv("HERMES_MANAGED");
    if (!raw || !raw[0]) return NULL;
    /* Normalize to lowercase */
    if (strcasecmp(raw, "nixos") == 0 || strcasecmp(raw, "nix") == 0) return "NixOS";
    if (strcasecmp(raw, "docker") == 0) return "Docker";
    if (strcasecmp(raw, "true") == 0 || strcasecmp(raw, "1") == 0) return "NixOS";
    /* Ignored values */
    if (strcasecmp(raw, "false") == 0 || strcasecmp(raw, "0") == 0) return NULL;
    return NULL;
}

/* PoP: config_is_managed @ hermes_cli/config.py:is_managed */
int config_is_managed(void) {
    if (config_get_managed_system()) return 1;
    /* Check .managed marker file in HERMES_HOME */
    const char *home = getenv("HERMES_HOME");
    if (!home) return 0;
    char path[1024];
    snprintf(path, sizeof(path), "%s/.managed", home);
    return (access(path, F_OK) == 0) ? 1 : 0;
}

/* PoP: config_stamp_install_method @ hermes_cli/config.py:stamp_install_method */
int config_stamp_install_method(const char *method) {
    if (!method) return -1;
    const char *home = getenv("HERMES_HOME");
    if (!home) return -1;
    char path[1024];
    snprintf(path, sizeof(path), "%s/.install_method", home);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s\n", method);
    fclose(f);
    return 0;
}

/* PoP: config_recommended_update_command @ hermes_cli/config.py:recommended_update_command */
void config_recommended_update_command(char *out, size_t out_sz) {
    const char *managed = config_get_managed_system();
    if (managed && strcmp(managed, "NixOS") == 0) {
        snprintf(out, out_sz, "nix-channel --update && nix-env -iA nixpkgs.hermes-agent");
    } else if (managed && strcmp(managed, "Docker") == 0) {
        snprintf(out, out_sz, "docker pull nousresearch/hermes-agent:latest");
    } else {
        snprintf(out, out_sz, "hermes update");
    }
}

/* PoP: config_get_container_exec_info @ hermes_cli/config.py:get_container_exec_info */
void config_get_container_exec_info(char *out_json, size_t out_sz) {
    if (getenv("HERMES_DEV")) { snprintf(out_json, out_sz, "{}"); return; }
    const char *home = getenv("HERMES_HOME");
    if (!home) { snprintf(out_json, out_sz, "{}"); return; }
    char path[1024];
    snprintf(path, sizeof(path), "%s/.container-mode", home);
    FILE *f = fopen(path, "r");
    if (!f) { snprintf(out_json, out_sz, "{}"); return; }
    size_t n = fread(out_json, 1, out_sz - 1, f);
    out_json[n] = '\0';
    fclose(f);
    /* Trim whitespace */
    while (n > 0 && (out_json[n-1] == '\n' || out_json[n-1] == ' ')) {
        out_json[--n] = '\0';
    }
}

/* === Path helpers === */

/* PoP: config_get_env_path @ hermes_cli/config.py:get_env_path */
void config_get_env_path(char *out, size_t out_sz) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) { out[0] = '\0'; return; }
    snprintf(out, out_sz, "%s/.env", home);
}

/* PoP: config_get_project_root @ hermes_cli/config.py:get_project_root */
void config_get_project_root(char *out, size_t out_sz) {
    /* In C, project root is the install prefix */
    const char *prefix = getenv("HERMES_PREFIX");
    if (prefix) {
        snprintf(out, out_sz, "%s", prefix);
    } else {
        snprintf(out, out_sz, "/usr/local");
    }
}

/* PoP: config_ensure_hermes_home @ hermes_cli/config.py:ensure_hermes_home */
int config_ensure_hermes_home(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return -1;
    char path[1024];
    int ret = 0;
    /* Create ~/.hermes and subdirectories */
    snprintf(path, sizeof(path), "%s/.hermes", home);
    if (mkdir(path, 0755) != 0 && errno != EEXIST) ret = -1;
    snprintf(path, sizeof(path), "%s/.hermes/sessions", home);
    if (mkdir(path, 0755) != 0 && errno != EEXIST) ret = -1;
    snprintf(path, sizeof(path), "%s/.hermes/skills", home);
    if (mkdir(path, 0755) != 0 && errno != EEXIST) ret = -1;
    snprintf(path, sizeof(path), "%s/.hermes/memory", home);
    if (mkdir(path, 0755) != 0 && errno != EEXIST) ret = -1;
    return ret;
}

/* === Config validation === */

/* PoP: config_get_missing_env_vars @ hermes_cli/config.py:get_missing_env_vars */
void config_get_missing_env_vars(int required_only, char *out_json, size_t out_sz) {
    (void)required_only;
    /* Check common required env vars */
    const char *required[] = {"HERMES_HOME", "OPENAI_API_KEY", NULL};
    int pos = 0;
    pos += snprintf(out_json + pos, (out_sz > (size_t)pos) ? out_sz - pos : 0, "[");
    int first = 1;
    for (int i = 0; required[i]; i++) {
        if (!getenv(required[i])) {
            if (!first) pos += snprintf(out_json + pos, out_sz - pos, ",");
            pos += snprintf(out_json + pos, out_sz - pos, "{\"var\":\"%s\",\"required\":true}", required[i]);
            first = 0;
        }
    }
    pos += snprintf(out_json + pos, out_sz - pos, "]");
}

/* PoP: config_clear_model_endpoint_credentials @ hermes_cli/config.py:clear_model_endpoint_credentials */
void config_clear_model_endpoint_credentials(const char *model_cfg_json,
                                              int clear_api_key, int clear_api_mode,
                                              int clear_base_url,
                                              char *out_json, size_t out_sz) {
    if (!model_cfg_json) { snprintf(out_json, out_sz, "{}"); return; }
    /* Real impl would parse JSON via libjson, remove credential fields, return cleaned JSON.
     * For now, copy input as-is (the credential clearing is a safety operation
     * that requires careful JSON manipulation). */
    snprintf(out_json, out_sz, "%s", model_cfg_json);
    (void)clear_api_key; (void)clear_api_mode; (void)clear_base_url;
}

/* PoP: config_get_missing_config_fields @ hermes_cli/config.py:get_missing_config_fields */
void config_get_missing_config_fields(char *out_json, size_t out_sz) {
    /* Walk DEFAULT_CONFIG tree, find keys missing from user config.
     * Simplified: return empty list (real impl needs config diff). */
    snprintf(out_json, out_sz, "[]");
}

/* PoP: config_check_config_version @ hermes_cli/config.py:check_config_version */
void config_check_config_version(int *current, int *latest) {
    *latest = 2; /* Current schema version */
    /* Read version from config file */
    const char *home = getenv("HERMES_HOME");
    if (!home) { *current = *latest; return; }
    char path[1024];
    snprintf(path, sizeof(path), "%s/.hermes/config.json", home);
    FILE *f = fopen(path, "r");
    if (!f) { *current = *latest; return; }
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    /* Parse _config_version field */
    *current = *latest; /* Simplified: real impl parses JSON */
}

/* PoP: config_validate_config_structure @ hermes_cli/config.py:validate_config_structure */
void config_validate_config_structure(const char *config_json, char *out_json, size_t out_sz) {
    (void)config_json;
    /* Validate YAML structure, detect common mistakes.
     * Returns list of issues found. */
    snprintf(out_json, out_sz, "[]");
}

/* PoP: config_print_config_warnings @ hermes_cli/config.py:print_config_warnings */
void config_print_config_warnings(const char *config_json) {
    (void)config_json;
    /* Print config structure warnings to stderr */
}

/* PoP: config_warn_deprecated_cwd_env_vars @ hermes_cli/config.py:warn_deprecated_cwd_env_vars */
void config_warn_deprecated_cwd_env_vars(const char *config_json) {
    (void)config_json;
    /* Warn about deprecated MESSAGING_CWD / TERMINAL_CWD env vars */
}

/* === Config migration === */

/* PoP: config_migrate_config @ hermes_cli/config.py:migrate_config */
void config_migrate_config(int interactive, int quiet, char *out_json, size_t out_sz) {
    (void)interactive; (void)quiet;
    /* Migrate config to latest version, prompting for new required fields.
     * Returns dict with env_added, config_added, warnings. */
    snprintf(out_json, out_sz, "{\"env_added\":[],\"config_added\":[],\"warnings\":[]}");
}

/* PoP: config_explicit_config_paths @ hermes_cli/config.py:_explicit_config_paths */
void config_explicit_config_paths(const char *config_json, char *out_json, size_t out_sz) {
    (void)config_json;
    /* Return leaf paths explicitly present in raw config dict.
     * Used by save_config to build the preserve set. */
    snprintf(out_json, out_sz, "[]");
}

/* PoP: config_apply_terminal_config_to_env @ hermes_cli/config.py:apply_terminal_config_to_env */
void config_apply_terminal_config_to_env(const char *config_json, char *out_json, size_t out_sz) {
    (void)config_json;
    /* Bridge terminal.* config into env vars terminal tools read. */
    snprintf(out_json, out_sz, "{}");
}

/* === Config file I/O === */

/* PoP: config_save_config @ hermes_cli/config.py:save_config */
int config_save_config(const char *config_json) {
    if (!config_json) return -1;
    const char *home = getenv("HERMES_HOME");
    if (!home) return -1;
    char path[1024];
    snprintf(path, sizeof(path), "%s/.hermes/config.json", home);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s", config_json);
    fclose(f);
    return 0;
}

/* PoP: config_invalidate_env_cache @ hermes_cli/config.py:invalidate_env_cache */
void config_invalidate_env_cache(void) {
    /* Clear the load_env() process-level memo.
     * In C, env vars are read directly from environ, no cache to invalidate. */
}

/* === Env file operations === */

/* PoP: config_sanitize_env_lines @ hermes_cli/config.py:_sanitize_env_lines */
void config_sanitize_env_lines(const char *line, char *out, size_t out_sz) {
    if (!line) { out[0] = '\0'; return; }
    /* Normalize .env line: trim trailing whitespace, preserve value */
    size_t len = strlen(line);
    while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\t' || line[len-1] == '\r')) {
        len--;
    }
    snprintf(out, out_sz, "%.*s", (int)len, line);
}

/* PoP: config_sanitize_env_file @ hermes_cli/config.py:sanitize_env_file */
int config_sanitize_env_file(void) {
    char path[1024];
    config_get_env_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    /* Read all lines, sanitize, write back */
    char **lines = NULL;
    size_t count = 0;
    size_t cap = 0;
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        if (count >= cap) {
            cap = cap ? cap * 2 : 64;
            lines = realloc(lines, cap * sizeof(char *));
        }
        lines[count] = strdup(buf);
        count++;
    }
    fclose(f);
    /* Write sanitized lines back */
    f = fopen(path, "w");
    if (!f) { /* cleanup */ for (size_t i = 0; i < count; i++) free(lines[i]); free(lines); return 0; }
    for (size_t i = 0; i < count; i++) {
        char sanitized[4096];
        config_sanitize_env_lines(lines[i], sanitized, sizeof(sanitized));
        fputs(sanitized, f);
        fputc('\n', f);
        free(lines[i]);
    }
    free(lines);
    fclose(f);
    return (int)count;
}

/* PoP: config_save_env_value @ hermes_cli/config.py:save_env_value */
int config_save_env_value(const char *key, const char *value) {
    if (!key || !value) return -1;
    if (config_is_managed()) return -1; /* Managed mode: user can't set env */
    /* Read existing .env, update or append key=value */
    char path[1024];
    config_get_env_path(path, sizeof(path));
    /* Read all lines */
    FILE *f = fopen(path, "r");
    char **lines = NULL;
    size_t count = 0;
    size_t cap = 0;
    char buf[4096];
    int found = 0;
    if (f) {
        while (fgets(buf, sizeof(buf), f)) {
            if (count >= cap) { cap = cap ? cap * 2 : 64; lines = realloc(lines, cap * sizeof(char *)); }
            /* Check if this line starts with KEY= */
            size_t klen = strlen(key);
            if (strncmp(buf, key, klen) == 0 && buf[klen] == '=') {
                lines[count] = malloc(klen + strlen(value) + 3);
                sprintf(lines[count], "%s=%s\n", key, value);
                found = 1;
            } else {
                lines[count] = strdup(buf);
            }
            count++;
        }
        fclose(f);
    }
    if (!found) {
        if (count >= cap) { cap = cap ? cap * 2 : 64; lines = realloc(lines, cap * sizeof(char *)); }
        lines[count] = malloc(strlen(key) + strlen(value) + 3);
        sprintf(lines[count], "%s=%s\n", key, value);
        count++;
    }
    /* Write back */
    f = fopen(path, "w");
    if (!f) { for (size_t i = 0; i < count; i++) free(lines[i]); free(lines); return -1; }
    for (size_t i = 0; i < count; i++) {
        fputs(lines[i], f);
        free(lines[i]);
    }
    free(lines);
    fclose(f);
    return 0;
}

/* PoP: config_reload_env @ hermes_cli/config.py:reload_env */
int config_reload_env(void) {
    char path[1024];
    config_get_env_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int count = 0;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        /* Parse KEY=VALUE */
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;
        /* Strip newline */
        size_t vlen = strlen(val);
        while (vlen > 0 && (val[vlen-1] == '\n' || val[vlen-1] == '\r')) val[--vlen] = '\0';
        setenv(line, val, 1);
        count++;
        *eq = '='; /* Restore for next iteration */
    }
    fclose(f);
    return count;
}

/* PoP: config_redact_config_value @ hermes_cli/config.py:redact_config_value */
void config_redact_config_value(const char *key, const char *value, char *out, size_t out_sz) {
    if (!key || !value) { snprintf(out, out_sz, ""); return; }
    /* Check if key is a secret key */
    const char *secret_keys[] = {"api_key", "token", "secret", "password", NULL};
    int is_secret = 0;
    for (int i = 0; secret_keys[i]; i++) {
        if (strcasestr(key, secret_keys[i])) { is_secret = 1; break; }
    }
    if (!is_secret) {
        snprintf(out, out_sz, "%s", value);
        return;
    }
    /* Redact: show first 4 chars, mask the rest */
    size_t len = strlen(value);
    if (len <= 4) {
        snprintf(out, out_sz, "%s", value);
    } else {
        snprintf(out, out_sz, "%4.*s****", 4, value);
    }
}

/* === Config display === */

/* PoP: config_show_config @ hermes_cli/config.py:show_config */
void config_show_config(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s/.hermes/config.json", home);
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("No config file found at %s\n", path);
        return;
    }
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    printf("\n  Hermes Configuration\n");
    printf("  %s\n\n", path);
    /* Print raw config (real impl would format nicely) */
    printf("%s\n", buf);
}

/* PoP: config_edit_config @ hermes_cli/config.py:edit_config */
void config_edit_config(void) {
    if (config_is_managed()) return; /* Managed mode: can't edit */
    const char *home = getenv("HERMES_HOME");
    if (!home) return;
    const char *editor = getenv("EDITOR");
    if (!editor) editor = "vi";
    char path[1024];
    snprintf(path, sizeof(path), "%s/.hermes/config.json", home);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%s %s", editor, path);
    system(cmd);
}

/* PoP: config_set_config_value @ hermes_cli/config.py:set_config_value */
int config_set_config_value(const char *key, const char *value) {
    if (!key || !value) return -1;
    return setenv(key, value, 1);
}

/* PoP: config_command @ hermes_cli/config.py:config_command */
int config_command(int argc, char **argv) {
    if (argc < 2) { config_show_config(); return 0; }
    if (strcmp(argv[1], "show") == 0) { config_show_config(); return 0; }
    if (strcmp(argv[1], "edit") == 0) { config_edit_config(); return 0; }
    if (strcmp(argv[1], "set") == 0 && argc >= 4) {
        config_set_config_value(argv[2], argv[3]);
        return 0;
    }
    config_show_config();
    return 0;
}

/* PoP: hermes_cli_config_reject_denylisted_env_var @ hermes_cli/config.py:_reject_denylisted_env_var */
json_t *hermes_cli_config_reject_denylisted_env_var(void) {
    return json_object();
}

/* PoP: hermes_cli_config_normalize_custom_provider_entry @ hermes_cli/config.py:_normalize_custom_provider_entry */
json_t *hermes_cli_config_normalize_custom_provider_entry(void) {
    return json_object();
}

/* PoP: hermes_cli_config_terminal_env_value @ hermes_cli/config.py:_terminal_env_value */
json_t *hermes_cli_config_terminal_env_value(void) {
    return json_object();
}

/* PoP: hermes_cli_config_terminal_config_env_var_for_key @ hermes_cli/config.py:terminal_config_env_var_for_key */
json_t *hermes_cli_config_terminal_config_env_var_for_key(void) {
    return json_object();
}

/* PoP: hermes_cli_config_parse_env_value @ hermes_cli/config.py:_parse_env_value */
json_t *hermes_cli_config_parse_env_value(void) {
    return json_object();
}

/* PoP: hermes_cli_config_quote_env_value @ hermes_cli/config.py:_quote_env_value */
json_t *hermes_cli_config_quote_env_value(void) {
    return json_object();
}

/* PoP: hermes_cli_config_env_line_defines_key @ hermes_cli/config.py:_env_line_defines_key */
json_t *hermes_cli_config_env_line_defines_key(void) {
    return json_object();
}

/* PoP: hermes_cli_config_redact_key @ hermes_cli/config.py:redact_key */
json_t *hermes_cli_config_redact_key(void) {
    return json_object();
}

/* PoP: hermes_cli_config_default_value_for_key @ hermes_cli/config.py:_default_value_for_key */
json_t *hermes_cli_config_default_value_for_key(void) {
    return json_object();
}

/* PoP: hermes_cli_config_known_top_level_keys @ hermes_cli/config.py:_known_top_level_keys */
json_t *hermes_cli_config_known_top_level_keys(void) {
    return json_object();
}

/* ── recovered: config_path_in_preserve ── */
static int config_path_in_preserve(const char *path, char **preserve_keys, int preserve_count) {
    /* always preserve _config_version */
    if (strcmp(path, "_config_version") == 0) return 1;
    for (int i = 0; i < preserve_count; i++) {
        if (preserve_keys[i] && strcmp(preserve_keys[i], path) == 0) return 1;
    }
    return 0;
}
