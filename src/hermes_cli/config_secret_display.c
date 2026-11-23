/*
 * config_secret_display.c — real C11 port of upstream
 * hermes_cli/config_secret_display.py.
 *
 * PoP: secdisplay_maskable_secret_text @ hermes_cli/config_secret_display.py:_maskable_secret_text
 * PoP: secdisplay_redact_config_value @ hermes_cli/config_secret_display.py:redact_config_value
 *
 * The helpers it delegates to (agent.redact.mask_secret and
 * hermes_cli.config._is_secret_config_key / _ENV_PLACEHOLDER_RE) are
 * reimplemented locally so this object links with json.o alone.
 */
#include "noc_secdisplay.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_n(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
static char *sd_strdup(const char *s) { return dup_n(s ? s : "", s ? strlen(s) : 0); }

static char *num_py_str(double v) {
    char buf[64];
    if (v == 0.0) return dup_n("0", 1);
    if (isfinite(v) && floor(v) == v && v >= -9.22e18 && v <= 9.22e18) {
        snprintf(buf, sizeof(buf), "%lld", (long long)v);
        return dup_n(buf, strlen(buf));
    }
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, sizeof(buf), "%.*g", prec, v);
        if (strtod(buf, NULL) == v) break;
    }
    return dup_n(buf, strlen(buf));
}

/* PoP: secdisplay_maskable_secret_text @ hermes_cli/config_secret_display.py:_maskable_secret_text */
char *secdisplay_maskable_secret_text(const json_t *value) {
    if (!value) return sd_strdup("");
    switch (value->type) {
    case JSON_BOOL:   return sd_strdup("");           /* isinstance(value, bool) first */
    case JSON_NUMBER: return num_py_str(value->num_val);
    case JSON_STRING: return sd_strdup(value->str_val);
    default:          return sd_strdup("");           /* None / list / dict */
    }
}

/* --- copied shapes from hermes_cli/config.py --- */
static const char *const ENV_CONFIG_KEYS[] = {
    "ANTHROPIC_API_KEY", "API_SERVER_KEY", "BROWSERBASE_API_KEY",
    "BROWSERBASE_PROJECT_ID", "BROWSER_USE_API_KEY", "CONNECTOR_GATEWAY_URL",
    "DISCORD_BOT_TOKEN", "EXA_API_KEY", "FAL_KEY", "FIRECRAWL_API_KEY",
    "FIRECRAWL_API_URL", "FIRECRAWL_GATEWAY_URL", "GITHUB_TOKEN",
    "OPENAI_API_KEY", "OPENROUTER_API_KEY", "PARALLEL_API_KEY",
    "PERPLEXITY_API_KEY", "SLACK_APP_TOKEN", "SLACK_BOT_TOKEN",
    "SUDO_PASSWORD", "TAVILY_API_KEY", "TELEGRAM_BOT_TOKEN",
    "TERMINAL_SSH_HOST", "TERMINAL_SSH_KEY", "TERMINAL_SSH_USER",
    "TOOL_GATEWAY_DOMAIN", "TOOL_GATEWAY_SCHEME", "TOOL_GATEWAY_URL",
    "TOOL_GATEWAY_USER_TOKEN", "VOICE_TOOLS_OPENAI_KEY", NULL
};
static const char *const SECRET_CONFIG_KEYS[] = {
    "api_key", "apikey", "key", "token", "access_token", "refresh_token",
    "id_token", "secret", "client_secret", "password", "passwd",
    "authorization", "private_key", "bearer", "jwt", NULL
};
static const char *const SECRET_SUFFIXES[] = {
    "_api_key", "_token", "_secret", "_password", "_key", "_access_key", NULL
};
static const char *const NON_SECRET_SUFFIXES[] = {
    "_url", "_host", "_user", "_id", "_domain", "_scheme", NULL
};

static bool endswith_any(const char *s, const char *const *suffs) {
    size_t n;
    if (!s) return false;
    n = strlen(s);
    for (int i = 0; suffs[i]; i++) {
        size_t m = strlen(suffs[i]);
        if (n >= m && !strcmp(s + n - m, suffs[i])) return true;
    }
    return false;
}
static bool in_list_ci(const char *s, const char *const *list) {
    char *up;
    bool found = false;
    if (!s) return false;
    up = (char *)malloc(strlen(s) + 1);
    if (!up) return false;
    for (size_t i = 0; s[i]; i++) up[i] = (char)toupper((unsigned char)s[i]);
    up[strlen(s)] = '\0';
    for (int i = 0; list[i]; i++) {
        if (!strcmp(up, list[i])) { found = true; break; }
    }
    free(up);
    return found;
}

/* _is_env_config_key: dotted keys are never env keys. */
static bool is_env_config_key(const char *key) {
    size_t n;
    char *up;
    bool res;
    if (!key || strchr(key, '.')) return false;
    n = strlen(key);
    up = (char *)malloc(n + 1);
    if (!up) return false;
    for (size_t i = 0; i < n; i++) up[i] = (char)toupper((unsigned char)key[i]);
    up[n] = '\0';
    res = in_list_ci(key, ENV_CONFIG_KEYS)
       || endswith_any(up, (const char *const[]){"_API_KEY", "_TOKEN", "_SECRET", NULL})
       || !strncmp(up, "TERMINAL_SSH", 12);
    free(up);
    return res;
}

/* _is_secret_config_key: leaf = last dot segment, lowercased, '-'->'_'. */
static bool is_secret_config_key(const char *key) {
    const char *dot, *leaf0;
    char *leaf;
    size_t n, i;
    bool env, res;
    if (!key) return false;
    dot = strrchr(key, '.');
    leaf0 = dot ? dot + 1 : key;
    n = strlen(leaf0);
    leaf = (char *)malloc(n + 1);
    if (!leaf) return false;
    for (i = 0; i < n; i++) leaf[i] = (char)tolower((unsigned char)leaf0[i]);
    leaf[n] = '\0';
    for (i = 0; leaf[i]; i++) if (leaf[i] == '-') leaf[i] = '_';
    env = is_env_config_key(key);
    if (env) {
        res = !endswith_any(leaf, NON_SECRET_SUFFIXES);
    } else {
        res = false;
        for (int j = 0; SECRET_CONFIG_KEYS[j]; j++)
            if (!strcmp(leaf, SECRET_CONFIG_KEYS[j])) { res = true; break; }
        if (!res) res = endswith_any(leaf, SECRET_SUFFIXES);
    }
    free(leaf);
    return res;
}

/* _ENV_PLACEHOLDER_RE.match: ^\$\{[A-Za-z_][A-Za-z0-9_]*\}$ */
static bool env_placeholder(const char *t) {
    const char *p;
    if (!t || t[0] != '$' || t[1] != '{') return false;
    p = t + 2;
    if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_'))
        return false;
    p++;
    while (isalnum((unsigned char)*p) || *p == '_') p++;
    return *p == '}' && *(p + 1) == '\0';
}

/* agent.redact.mask_secret(value, head=4, tail=4, floor=12). */
static char *mask_secret(const char *value) {
    size_t n, i, kept = 0;
    char *clean, *out;
    if (!value || !*value) return sd_strdup("");
    n = strlen(value);
    clean = (char *)malloc(n + 1);
    if (!clean) return sd_strdup("");
    /* _DISPLAY_CONTROL_RE: C0/C1 controls, byte-oriented for ASCII/C1. */
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)value[i];
        if (c <= 0x1f || (c >= 0x7f && c <= 0x9f)) continue;
        clean[kept++] = value[i];
    }
    clean[kept] = '\0';
    if (kept == 0) { free(clean); return sd_strdup(""); }
    if (kept < 12) { free(clean); return sd_strdup("***"); }
    out = (char *)malloc(4 + 3 + 4 + 1);
    if (!out) { free(clean); return sd_strdup(""); }
    memcpy(out, clean, 4);
    memcpy(out + 4, "...", 3);
    memcpy(out + 7, clean + kept - 4, 4);
    out[11] = '\0';
    free(clean);
    return out;
}

/* PoP: secdisplay_redact_config_value @ hermes_cli/config_secret_display.py:redact_config_value */
json_t *secdisplay_redact_config_value(const json_t *value, int depth) {
    json_t *out;
    if (depth > 20) return value ? json_copy(value) : json_null();
    if (!value) return json_null();
    if (value->type == JSON_OBJECT) {
        out = json_object();
        if (!out) return NULL;
        for (size_t i = 0; i < json_object_size(value); i++) {
            const char *k = json_object_get_key_at(value, i);
            json_t *v = json_object_get_at(value, i);
            char *text = NULL;
            if (k && is_secret_config_key(k))
                text = secdisplay_maskable_secret_text(v);
            if (text && *text && !env_placeholder(text)) {
                char *masked = mask_secret(text);
                json_set(out, k, json_string(masked ? masked : ""));
                free(masked);
            } else {
                json_set(out, k, secdisplay_redact_config_value(v, depth + 1));
            }
            free(text);
        }
        return out;
    }
    if (value->type == JSON_ARRAY) {
        out = json_array();
        if (!out) return NULL;
        for (size_t i = 0; i < json_len(value); i++)
            json_append(out, secdisplay_redact_config_value(json_get(value, i), depth + 1));
        return out;
    }
    return json_copy(value);  /* scalars pass through unchanged */
}

