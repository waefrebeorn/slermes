/*
 * runtime_provider_missing_key.c -- real C11 port of
 * hermes_cli/runtime_provider_missing_key.py (gate w21_runtimepro_eb8ed1): a
 * provider key the active profile's secret scope does not have, said before
 * any request.
 *
 * PoP: w21_runtimepro_eb8ed1_declared_key_env @ hermes_cli/runtime_provider_missing_key.py:_declared_key_env
 * PoP: w21_runtimepro_eb8ed1_declared_custom_key @ hermes_cli/runtime_provider_missing_key.py:_declared_custom_key
 * PoP: w21_runtimepro_eb8ed1_where_to_set_key @ hermes_cli/runtime_provider_missing_key.py:where_to_set_key
 * PoP: w21_runtimepro_eb8ed1_named_profile_key_hint @ hermes_cli/runtime_provider_missing_key.py:named_profile_key_hint
 * PoP: w21_runtimepro_eb8ed1_raise_if_declared_key_missing @ hermes_cli/runtime_provider_missing_key.py:raise_if_declared_key_missing
 *
 * The entry matching `_declared_custom_key` leans on
 * `agent.custom_entry_pool.named_custom_entry` / `custom_entry_for_endpoint` is
 * ported here whole, together with the credential-pool helpers they use
 * (`_normalize_custom_pool_name`, `_requested_custom_name_aliases`,
 * `_custom_entry_name_aliases`, `_norm_url`): all pure name/URL comparisons
 * over the compatible custom-provider view. Only the config load that produces
 * that view, the secret scope and the profile/process-environment reads are
 * seams.
 */
#include "hermes_json.h"
#include "noc_w21_runtimepro_eb8ed1.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const RPK8E_NO_KEY_PLACEHOLDER = "no-key-required";
static const char *const RPK8E_CUSTOM_POOL_PREFIX = "custom:";

static char *rpk8e_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python str.strip()/str.lower() for the ASCII shapes config carries. */
static char *rpk8e_strip(const char *s) {
    size_t b = 0, e;
    char *out;
    if (!s) return rpk8e_dup("");
    e = strlen(s);
    while (b < e && isspace((unsigned char)s[b])) b++;
    while (e > b && isspace((unsigned char)s[e - 1])) e--;
    out = (char *)malloc(e - b + 1);
    if (!out) return NULL;
    memcpy(out, s + b, e - b);
    out[e - b] = '\0';
    return out;
}

static void rpk8e_lower(char *s) {
    for (; s && *s; s++) s[0] = (char)tolower((unsigned char)*s);
}

/* `_normalize_custom_pool_name`: strip, lower, spaces to hyphens. */
static char *rpk8e_pool_name(const char *name) {
    char *out = rpk8e_strip(name);
    if (!out) return NULL;
    rpk8e_lower(out);
    for (char *p = out; *p; p++) if (*p == ' ') *p = '-';
    return out;
}

/* `_norm_url(url)`: str(url or "").strip().rstrip("/"). */
static char *rpk8e_norm_url(const json_t *v) {
    char *out;
    size_t n;
    if (!v) return rpk8e_dup("");
    if (v->type == JSON_STRING) {
        out = rpk8e_strip(v->str_val);
    } else if (v->type == JSON_NUMBER) {
        char buf[40];
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        out = rpk8e_dup(buf);
    } else if (v->type == JSON_BOOL) {
        out = rpk8e_dup(v->bool_val ? "True" : "False");
    } else {
        return rpk8e_dup("");
    }
    if (!out) return NULL;
    n = strlen(out);
    while (n > 0 && out[n - 1] == '/') out[--n] = '\0';
    return out;
}

/* The same normalisation for a C string. */
static char *rpk8e_norm_url_text(const char *text) {
    char *out = rpk8e_strip(text);
    size_t n;
    if (!out) return NULL;
    n = strlen(out);
    while (n > 0 && out[n - 1] == '/') out[--n] = '\0';
    return out;
}

/* `str(value or "")` for the fields these functions read. */
static char *rpk8e_str_or_empty(const json_t *v) {
    char buf[40];
    if (!v || v->type == JSON_NULL) return rpk8e_dup("");
    switch (v->type) {
    case JSON_STRING: return rpk8e_dup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return rpk8e_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return rpk8e_dup(buf);
    default: return rpk8e_dup("");
    }
}

/* Python truthiness for a decoded JSON value; NULL (absent) reads as None. */
static int rpk8e_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

char *w21_runtimepro_eb8ed1_declared_key_env(const json_t *entry) {
    json_t *v;
    char *out;
    if (!entry || entry->type != JSON_OBJECT) return rpk8e_dup("");
    v = json_obj_get(entry, "key_env");
    if (!rpk8e_truthy(v)) v = json_obj_get(entry, "api_key_env");
    if (!rpk8e_truthy(v)) return rpk8e_dup("");
    {
        char *raw = rpk8e_str_or_empty(v);
        out = rpk8e_strip(raw);
        free(raw);
    }
    return out;
}

/* --- the credential-pool name/URL matching, ported ----------------------- */

/* `_requested_custom_name_aliases(provider_name)`: the normalized name, plus its
 * suffix when it is spelled `custom:<name>`. */
static int rpk8e_requested_aliases(const char *provider_name, char *out[2]) {
    char *normalized;
    int n = 0;
    out[0] = out[1] = NULL;
    if (!provider_name) return 0;
    normalized = rpk8e_pool_name(provider_name);
    if (!normalized) return 0;
    if (normalized[0]) out[n++] = normalized;
    else { free(normalized); return 0; }
    if (!strncmp(normalized, RPK8E_CUSTOM_POOL_PREFIX, strlen(RPK8E_CUSTOM_POOL_PREFIX))) {
        char *suffix = rpk8e_pool_name(normalized + strlen(RPK8E_CUSTOM_POOL_PREFIX));
        if (suffix && suffix[0]) out[n++] = suffix;
        else free(suffix);
    }
    return n;
}

/* `_custom_entry_name_aliases(norm_name, entry)`: the normalized name and the
 * entry's normalized `provider_key`. */
static int rpk8e_entry_aliases(const char *norm_name, const json_t *entry, char *out[2]) {
    json_t *key = entry && entry->type == JSON_OBJECT ? json_obj_get(entry, "provider_key") : NULL;
    char *raw = rpk8e_str_or_empty(key);
    char *pk = rpk8e_pool_name(raw);
    int n = 0;
    free(raw);
    out[0] = out[1] = NULL;
    if (norm_name && norm_name[0]) out[n++] = rpk8e_dup(norm_name);
    if (pk && pk[0]) out[n++] = pk;
    else free(pk);
    return n;
}

/* `_iter_custom_providers(config)`: (normalized_name, entry) for dict entries
 * that carry a string name, in view order. */
static const json_t *rpk8e_entry_name(const json_t *entry, char **name_out) {
    json_t *name;
    *name_out = NULL;
    if (!entry || entry->type != JSON_OBJECT) return NULL;
    name = json_obj_get(entry, "name");
    if (!name || name->type != JSON_STRING) return NULL;
    *name_out = rpk8e_pool_name(name->str_val);
    return entry;
}

/* `named_custom_entry(provider_name, config)`: index into entries, or -1. */
static int rpk8e_named_entry(const char *provider_name, const json_t *entries) {
    char *aliases[2] = {NULL, NULL};
    int na, found = -1;
    size_t i, n;
    na = rpk8e_requested_aliases(provider_name, aliases);
    if (!na) return -1;
    n = entries && entries->type == JSON_ARRAY ? entries->c.count : 0;
    for (i = 0; i < n && found < 0; i++) {
        char *norm = NULL;
        char *entry_aliases[2] = {NULL, NULL};
        int neb, k, m;
        if (!rpk8e_entry_name(entries->c.items[i], &norm)) continue;
        neb = rpk8e_entry_aliases(norm, entries->c.items[i], entry_aliases);
        for (k = 0; k < na && found < 0; k++) {
            for (m = 0; m < neb; m++) {
                if (!strcmp(aliases[k], entry_aliases[m])) { found = (int)i; break; }
            }
        }
        for (m = 0; m < neb; m++) free(entry_aliases[m]);
        free(norm);
    }
    for (na = 0; na < 2; na++) free(aliases[na]);
    return found;
}

/* `custom_entry_for_endpoint(base_url, provider_name, config)`. */
static int rpk8e_entry_for_endpoint(const char *base_url, const char *provider_name, const json_t *entries) {
    char *url;
    int named;
    size_t i, n;
    if (!base_url || !base_url[0]) return -1;
    url = rpk8e_norm_url_text(base_url);
    named = rpk8e_named_entry(provider_name, entries);
    if (named >= 0) {
        char *entry_url = rpk8e_norm_url(json_obj_get(entries->c.items[named], "base_url"));
        int result;
        if (entry_url && entry_url[0] && strcmp(entry_url, url)) result = -1;
        else result = named;
        free(entry_url);
        free(url);
        return result;
    }
    n = entries && entries->type == JSON_ARRAY ? entries->c.count : 0;
    for (i = 0; i < n; i++) {
        char *entry_url = rpk8e_norm_url(json_obj_get(entries->c.items[i], "base_url"));
        int hit = url && url[0] && entry_url && !strcmp(entry_url, url);
        free(entry_url);
        if (hit) { free(url); return (int)i; }
    }
    free(url);
    return -1;
}

json_t *w21_runtimepro_eb8ed1_declared_custom_key(const char *requested_provider,
                                                   const json_t *runtime,
                                                   const json_t *model_cfg,
                                                   const json_t *entries) {
    char *base_url = NULL, *name = NULL, *label = NULL, *key_env = NULL;
    json_t *out = json_object();
    int match = -1;
    json_t *rt_url = runtime && runtime->type == JSON_OBJECT ? json_obj_get(runtime, "base_url") : NULL;
    char *raw_url = rpk8e_str_or_empty(rt_url);
    json_t *source = runtime && runtime->type == JSON_OBJECT ? json_obj_get(runtime, "source") : NULL;
    char *source_text = rpk8e_str_or_empty(source);
    int from_named = !strncmp(source_text, "custom_provider:", strlen("custom_provider:"));

    base_url = raw_url;                       /* ownership moves here */
    free(source_text);
    if (requested_provider && requested_provider[0] &&
        strcmp(requested_provider, "auto") && strcmp(requested_provider, "custom")) {
        name = rpk8e_dup(requested_provider);
    }
    if (name && from_named) match = rpk8e_named_entry(name, entries);
    else match = rpk8e_entry_for_endpoint(base_url, name, entries);

    if (match >= 0) {
        json_t *entry = entries->c.items[match];
        key_env = w21_runtimepro_eb8ed1_declared_key_env(entry);
        if (key_env && key_env[0]) {
            json_t *entry_name = json_obj_get(entry, "name");
            char *shown = rpk8e_truthy(entry_name) ? rpk8e_str_or_empty(entry_name) : NULL;
            size_t need;
            if (!shown) {
                char *norm = NULL;
                rpk8e_entry_name(entry, &norm);
                shown = norm ? norm : rpk8e_dup("");
            }
            need = strlen(shown) + 32;
            label = (char *)malloc(need);
            if (label) snprintf(label, need, "custom provider '%s'", shown);
            free(shown);
            json_set(out, "label", json_string(label ? label : ""));
            json_set(out, "key_env", json_string(key_env));
            free(label); free(key_env); free(name); free(base_url);
            return out;
        }
        free(key_env);
        key_env = NULL;
    }
    free(name);
    {
        char *cfg_url = model_cfg && model_cfg->type == JSON_OBJECT
                            ? rpk8e_norm_url(json_obj_get(model_cfg, "base_url")) : rpk8e_dup("");
        char *rt_norm = rpk8e_dup(base_url ? base_url : "");
        size_t n = strlen(rt_norm);
        while (n > 0 && isspace((unsigned char)rt_norm[n - 1])) rt_norm[--n] = '\0';
        {   /* base_url.strip().rstrip("/") */
            size_t m = 0;
            while (rt_norm[m] && isspace((unsigned char)rt_norm[m])) m++;
            if (m) memmove(rt_norm, rt_norm + m, strlen(rt_norm + m) + 1);
        }
        n = strlen(rt_norm);
        while (n > 0 && rt_norm[n - 1] == '/') rt_norm[--n] = '\0';
        if (cfg_url && cfg_url[0] && !strcmp(cfg_url, rt_norm)) {
            char *model_key = w21_runtimepro_eb8ed1_declared_key_env(model_cfg);
            if (model_key && model_key[0]) {
                json_set(out, "label", json_string("the custom endpoint in config.yaml (model.key_env)"));
                json_set(out, "key_env", json_string(model_key));
                free(model_key); free(cfg_url); free(rt_norm); free(base_url);
                return out;
            }
            free(model_key);
        }
        free(cfg_url);
        free(rt_norm);
    }
    json_set(out, "label", json_string(""));
    json_set(out, "key_env", json_string(""));
    free(base_url);
    return out;
}

char *w21_runtimepro_eb8ed1_where_to_set_key(const char *env_var, const char *profile,
                                             const char *display_home, const char *cli_selector,
                                             int serves_routed, int env_has_var) {
    const char *var = env_var ? env_var : "";
    const char *who = (profile && profile[0]) ? profile : "default";
    const char *home = display_home ? display_home : "";
    const char *sel = cli_selector ? cli_selector : "";
    char *hint;
    size_t cap = strlen(var) * 2 + strlen(who) + strlen(home) + strlen(sel) + 256;
    hint = (char *)malloc(cap);
    if (!hint) return NULL;
    snprintf(hint, cap,
             "Add it to profile '%s': run `hermes %sconfig set %s <your key>` (stored in %s/.env).",
             who, sel, var, home);
    if (serves_routed && env_has_var) {
        char *with_note = (char *)malloc(cap + strlen(var) + strlen(who) + 160);
        if (!with_note) return hint;
        snprintf(with_note, cap + strlen(var) + strlen(who) + 160,
                 "%s is set in this server's environment, but profile '%s' reads keys only from its "
                 "own .env. %s",
                 var, who, hint);
        free(hint);
        return with_note;
    }
    return hint;
}

char *w21_runtimepro_eb8ed1_named_profile_key_hint(const json_t *env_vars, const char *profile,
                                                   const char *display_home, const char *cli_selector,
                                                   int serves_routed, int env_has_var) {
    char *first = NULL;
    size_t i, n;
    n = env_vars && env_vars->type == JSON_ARRAY ? env_vars->c.count : 0;
    for (i = 0; i < n && !first; i++) {
        char *raw = rpk8e_str_or_empty(env_vars->c.items[i]);
        char *stripped = rpk8e_strip(raw);
        free(raw);
        if (stripped && stripped[0]) first = stripped;
        else free(stripped);
    }
    if (!first) return rpk8e_dup("");
    if (!profile || !profile[0] || !strcmp(profile, "default")) {
        free(first);
        return rpk8e_dup("");
    }
    {
        char *where = w21_runtimepro_eb8ed1_where_to_set_key(first, profile, display_home,
                                                            cli_selector, serves_routed, env_has_var);
        char *out = (char *)malloc(strlen(where ? where : "") + 2);
        if (out) snprintf(out, strlen(where ? where : "") + 2, " %s", where ? where : "");
        free(where);
        free(first);
        return out;
    }
}

/* `env` fields, defaulted as the live defaults read. */
static const char *rpk8e_env_str(const json_t *env, const char *key, const char *def) {
    if (!env || env->type != JSON_OBJECT) return def;
    return json_get_str(env, key, def);
}

static int rpk8e_env_var_set(const json_t *env, const char *name) {
    json_t *vars = env && env->type == JSON_OBJECT ? json_obj_get(env, "vars") : NULL;
    if (!vars || vars->type != JSON_OBJECT || !name) return 0;
    return rpk8e_truthy(json_obj_get(vars, name));
}

json_t *w21_runtimepro_eb8ed1_raise_if_declared_key_missing(const char *requested_provider,
                                                            const json_t *runtime,
                                                            const json_t *model_cfg,
                                                            const json_t *entries,
                                                            const json_t *secrets,
                                                            const json_t *explicit_api_key,
                                                            const json_t *env) {
    json_t *out;
    char *requested;
    {
        json_t *provider = runtime && runtime->type == JSON_OBJECT ? json_obj_get(runtime, "provider") : NULL;
        json_t *api_key = runtime && runtime->type == JSON_OBJECT ? json_obj_get(runtime, "api_key") : NULL;
        json_t *api_mode;
        if (!provider || provider->type != JSON_STRING || strcmp(provider->str_val ? provider->str_val : "", "custom"))
            return NULL;   /* not a custom runtime: nothing to say */
        if (!api_key || api_key->type != JSON_STRING || strcmp(api_key->str_val ? api_key->str_val : "",
                                                              RPK8E_NO_KEY_PLACEHOLDER))
            return NULL;
        api_mode = json_obj_get(runtime, "api_mode");
        if (rpk8e_truthy(explicit_api_key)) {
            char *text = rpk8e_str_or_empty(explicit_api_key);
            char *stripped = rpk8e_strip(text);
            int has = stripped && stripped[0];
            free(stripped);
            free(text);
            if (has) return NULL;
        }
        if (api_mode && api_mode->type == JSON_STRING) {
            const char *mode = api_mode->str_val ? api_mode->str_val : "";
            if (!strcmp(mode, "codex_app_server") || !strcmp(mode, "bedrock_converse")) return NULL;
        }
    }
    if (!env || env->type != JSON_OBJECT || !rpk8e_truthy(json_obj_get(env, "serves_routed"))) return NULL;

    requested = rpk8e_strip(requested_provider ? requested_provider : "");
    if (!requested) { free(requested); requested = rpk8e_dup(""); }
    rpk8e_lower(requested);
    {
        json_t *declared = w21_runtimepro_eb8ed1_declared_custom_key(requested, runtime, model_cfg, entries);
        const char *label = json_get_str(declared, "label", "");
        const char *key_env = json_get_str(declared, "key_env", "");
        if (!key_env || !key_env[0]) { json_free(declared); free(requested); return NULL; }
        if (secrets && secrets->type == JSON_OBJECT) {
            json_t *secret = json_obj_get(secrets, key_env);
            char *text = rpk8e_str_or_empty(secret);
            char *stripped = rpk8e_strip(text);
            int has = stripped && stripped[0];
            free(stripped);
            free(text);
            if (has) { json_free(declared); free(requested); return NULL; }
        }
        {
            char *where = w21_runtimepro_eb8ed1_where_to_set_key(
                key_env, rpk8e_env_str(env, "profile", "default"), rpk8e_env_str(env, "display_home", ""),
                rpk8e_env_str(env, "cli_selector", ""),
                rpk8e_truthy(json_obj_get(env, "serves_routed")), rpk8e_env_var_set(env, key_env));
            size_t cap = strlen(label) + strlen(key_env) + strlen(where ? where : "") + 256;
            char *message = (char *)malloc(cap);
            out = json_object();
            json_set(out, "status", json_string("error"));
            json_set(out, "type", json_string("AuthError"));
            if (message) {
                snprintf(message, cap,
                         "No API key for %s: it reads %s, which is not set for this profile, so the request "
                         "was not sent. %s",
                         label, key_env, where ? where : "");
                json_set(out, "message", json_string(message));
                free(message);
            } else {
                json_set(out, "message", json_string(""));
            }
            json_set(out, "provider", json_string(requested_provider && requested_provider[0] ? requested_provider : "custom"));
            json_set(out, "code", json_string("missing_api_key"));
            free(where);
        }
        json_free(declared);
    }
    free(requested);
    return out;
}
