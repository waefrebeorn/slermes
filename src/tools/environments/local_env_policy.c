/*
 * local_env_policy.c - C11 port of upstream
 * tools/environments/local_env_policy.py (gate w31_localenvpo_3ff534).
 *
 * PoP: w31_localenvpo_3ff534_snake_field @ tools/environments/local_env_policy.py:_snake_field
 * PoP: w31_localenvpo_3ff534_matches_terminal_first_party_prefix @ tools/environments/local_env_policy.py:_matches_terminal_first_party_prefix
 * PoP: w31_localenvpo_3ff534_is_hermes_internal_secret @ tools/environments/local_env_policy.py:_is_hermes_internal_secret
 * PoP: w31_localenvpo_3ff534_is_provider_env_blocklisted @ tools/environments/local_env_policy.py:_is_provider_env_blocklisted
 * PoP: w31_localenvpo_3ff534_is_profile_gate_env @ tools/environments/local_env_policy.py:is_profile_gate_env
 * PoP: w31_localenvpo_3ff534_carry_gateway_profile_pin @ tools/environments/local_env_policy.py:carry_gateway_profile_pin
 * PoP: w31_localenvpo_3ff534_strip_supervised_process_identity @ tools/environments/local_env_policy.py:strip_supervised_process_identity
 * PoP: w31_localenvpo_3ff534_strip_profile_gate_env @ tools/environments/local_env_policy.py:strip_profile_gate_env
 * PoP: w31_localenvpo_3ff534_config_declared_key_envs @ tools/environments/local_env_policy.py:_config_declared_key_envs
 * PoP: w31_localenvpo_3ff534_config_credential_env_refs @ tools/environments/local_env_policy.py:_config_credential_env_refs
 * PoP: w31_localenvpo_3ff534_constants @ tools/environments/local_env_policy.py:<module constants>
 *
 * Seams (left to the caller, no I/O here): os.environ (BUZZ_MANAGED_AGENT /
 * HERMES_GATEWAY_EXTERNAL_SUPERVISOR are passed in as the env dict),
 * gateway.session_context.get_session_env, the provider/adapter registries and
 * OPTIONAL_ENV_VARS that grow _PROVIDER_ENV_BLOCKLIST, the per-home plugin
 * manifests, the secret-source registry, and the gateway Platform enum behind
 * _platform_gate_env_prefixes (the owner set arrives as `prefixes`).
 *
 * ASCII-faithful note: name.upper()/lower()/strip() are ASCII here (upstream
 * folds Unicode too); every fixture name is ASCII, which is all an env name can
 * be on the platforms this policy guards.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_localenvpo_3ff534.h"

/* ------------------------------------------------------------------ */
/* module literals                                                     */
/* ------------------------------------------------------------------ */

/* _TERMINAL_FIRST_PARTY_ENV_PREFIXES */
static const char *const LV_TERMINAL_FIRST_PARTY_PREFIXES[] = { "BUZZ_" };

/* _SUPERVISED_PROCESS_IDENTITY_VARS */
static const char *const LV_SUPERVISED_IDENTITY_VARS[] = {
    "HERMES_LAUNCHD_LABEL", "HERMES_GATEWAY_EXTERNAL_SUPERVISOR", "NOTIFY_SOCKET",
    "WATCHDOG_USEC", "WATCHDOG_PID",
};
#define LV_SUPERVISED_IDENTITY_N (sizeof LV_SUPERVISED_IDENTITY_VARS / sizeof *LV_SUPERVISED_IDENTITY_VARS)

/* GATEWAY_PROFILE_PIN_ENV */
static const char *const LV_GATEWAY_PROFILE_PIN_ENV = "HERMES_GATEWAY_PROFILE_PIN";

/* _PROFILE_GATE_ENV_MARKERS */
static const char *const LV_PROFILE_GATE_MARKERS[] = {
    "_ALLOWED_", "_ALLOW_ALL_", "_ALLOW_FROM", "_ALLOW_BOTS", "_ALLOW_PUBLIC_",
    "_IGNORED_CHANNELS", "_NO_THREAD_CHANNELS", "_FREE_RESPONSE_CHANNELS",
    "_BACKFILL_CHANNELS", "_GROUP_ALLOWED",
};
#define LV_PROFILE_GATE_MARKERS_N (sizeof LV_PROFILE_GATE_MARKERS / sizeof *LV_PROFILE_GATE_MARKERS)

/* _EXTRA_GATE_ENV_PREFIXES */
static const char *const LV_EXTRA_GATE_PREFIXES[] = { "GATEWAY", "QQ" };
#define LV_EXTRA_GATE_PREFIXES_N (sizeof LV_EXTRA_GATE_PREFIXES / sizeof *LV_EXTRA_GATE_PREFIXES)

/* BRIDGED_ENV_MARKER_PREFIX (gateway.bridged_env) */
static const char *const LV_BRIDGED_MARKER_PREFIX = "HERMES_BRIDGED_";

/* _STATIC_PROVIDER_ENV_BLOCKLIST - the one blocklist the module owns as data;
 * every other member is derived at import time and arrives as `extra`. */
static const char *const LV_STATIC_PROVIDER_BLOCKLIST[] = {
    "OPENAI_BASE_URL", "OPENAI_API_KEY", "OPENAI_API_BASE", "OPENAI_ORG_ID",
    "OPENAI_ORGANIZATION", "OPENROUTER_API_KEY", "ANTHROPIC_BASE_URL",
    "ANTHROPIC_API_KEY", "ANTHROPIC_TOKEN", "LLM_MODEL", "GOOGLE_API_KEY",
    "VERTEX_CREDENTIALS_PATH", "GOOGLE_APPLICATION_CREDENTIALS", "DEEPSEEK_API_KEY",
    "MISTRAL_API_KEY", "GROQ_API_KEY", "TOGETHER_API_KEY", "PERPLEXITY_API_KEY",
    "COHERE_API_KEY", "FIREWORKS_API_KEY", "XAI_API_KEY", "HELICONE_API_KEY",
    "PARALLEL_API_KEY", "FIRECRAWL_API_KEY", "FIRECRAWL_API_URL",
    "TELEGRAM_HOME_CHANNEL", "TELEGRAM_HOME_CHANNEL_NAME", "DISCORD_HOME_CHANNEL",
    "DISCORD_HOME_CHANNEL_NAME", "DISCORD_REQUIRE_MENTION",
    "DISCORD_FREE_RESPONSE_CHANNELS", "DISCORD_AUTO_THREAD", "SLACK_HOME_CHANNEL",
    "SLACK_HOME_CHANNEL_NAME", "SLACK_ALLOWED_USERS", "WHATSAPP_ENABLED",
    "WHATSAPP_MODE", "WHATSAPP_ALLOWED_USERS", "SIGNAL_HTTP_URL", "SIGNAL_ACCOUNT",
    "SIGNAL_ALLOWED_USERS", "SIGNAL_GROUP_ALLOWED_USERS", "SIGNAL_HOME_CHANNEL",
    "SIGNAL_HOME_CHANNEL_NAME", "SIGNAL_IGNORE_STORIES",
    "EMAIL_ADDRESS", "EMAIL_PASSWORD", "EMAIL_IMAP_HOST", "EMAIL_SMTP_HOST",
    "EMAIL_HOME_ADDRESS", "EMAIL_HOME_ADDRESS_NAME", "HERMES_DASHBOARD_SESSION_TOKEN",
    "HERMES_TUI_SIDECAR_URL", "HERMES_TUI_GATEWAY_URL",
    "GATEWAY_ALLOWED_USERS", "GH_TOKEN", "GITHUB_APP_ID", "GITHUB_APP_PRIVATE_KEY_PATH",
    "GITHUB_APP_INSTALLATION_ID", "MODAL_TOKEN_ID", "MODAL_TOKEN_SECRET",
    "DAYTONA_API_KEY", "GATEWAY_RELAY_ID", "GATEWAY_RELAY_SECRET",
    "GATEWAY_RELAY_DELIVERY_KEY", "VERCEL_OIDC_TOKEN", "VERCEL_TOKEN",
    "VERCEL_PROJECT_ID", "VERCEL_TEAM_ID",
    "NOUS_API_KEY", "QWEN_API_KEY",
    "HERMES_ANON_API_SECRET", "HERMES_DASHBOARD_BASIC_AUTH_PASSWORD",
    "HERMES_DASHBOARD_BASIC_AUTH_SECRET", "HERMES_DASHBOARD_DRAIN_SECRET",
    "HERMES_DASHBOARD_OIDC_CLIENT_SECRET", "HERMES_MEET_REALTIME_KEY",
};
#define LV_STATIC_PROVIDER_BLOCKLIST_N \
    (sizeof LV_STATIC_PROVIDER_BLOCKLIST / sizeof *LV_STATIC_PROVIDER_BLOCKLIST)

/* _CONFIG_KEY_ENV_FIELD_RE: (?:^|_)(?:key|token|secret|password)_env$ */
static const char *const LV_KEY_ENV_SUFFIXES[] = { "key_env", "token_env", "secret_env", "password_env" };
#define LV_KEY_ENV_SUFFIXES_N (sizeof LV_KEY_ENV_SUFFIXES / sizeof *LV_KEY_ENV_SUFFIXES)

/* _CONFIG_CREDENTIAL_FIELD_RE: (?:^|_)(?:key|token|secret|password)$ */
static const char *const LV_CREDENTIAL_SUFFIXES[] = { "key", "token", "secret", "password" };
#define LV_CREDENTIAL_SUFFIXES_N (sizeof LV_CREDENTIAL_SUFFIXES / sizeof *LV_CREDENTIAL_SUFFIXES)

/* ------------------------------------------------------------------ */
/* string helpers (Python spellings)                                   */
/* ------------------------------------------------------------------ */

static char *lv_dup(const char *s)
{
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s) + 1;
    p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int lv_is_space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python .strip() over ASCII whitespace; returns a fresh string. */
static char *lv_strip_dup(const char *s)
{
    const char *b = s;
    size_t n;
    char *p;
    if (!s) return lv_dup("");
    while (*b && lv_is_space((unsigned char)*b)) b++;
    n = strlen(b);
    while (n && lv_is_space((unsigned char)b[n - 1])) n--;
    p = malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, b, n);
    p[n] = '\0';
    return p;
}

static char *lv_upper_dup(const char *s)
{
    char *p;
    size_t i, n;
    if (!s) return lv_dup("");
    n = strlen(s);
    p = malloc(n + 1);
    if (!p) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        p[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : (char)c;
    }
    p[n] = '\0';
    return p;
}

static char *lv_lower_dup(const char *s)
{
    char *p;
    size_t i, n;
    if (!s) return lv_dup("");
    n = strlen(s);
    p = malloc(n + 1);
    if (!p) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        p[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : (char)c;
    }
    p[n] = '\0';
    return p;
}

static int lv_starts(const char *s, const char *prefix)
{
    size_t n;
    if (!s || !prefix) return 0;
    n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

static int lv_ends(const char *s, const char *suffix)
{
    size_t a, b;
    if (!s || !suffix) return 0;
    a = strlen(s);
    b = strlen(suffix);
    if (b > a) return 0;
    return memcmp(s + (a - b), suffix, b) == 0;
}

static int lv_contains(const char *s, const char *needle)
{
    return (s && needle && strstr(s, needle) != NULL) ? 1 : 0;
}

/* Python truthiness. */
static int lv_truthy(const json_t *v)
{
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python str(value) for the shapes an env dict holds.  Containers fall back to
 * their JSON text (upstream would render a Python repr); env values are strings. */
static char *lv_py_str(const json_t *v)
{
    char buf[64];
    if (!v || v->type == JSON_NULL) return lv_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return lv_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return lv_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1e15 && d < 1e15)
            snprintf(buf, sizeof buf, "%lld", (long long)d);
        else
            snprintf(buf, sizeof buf, "%g", d);
        return lv_dup(buf);
    }
    default: {
        char *s = json_dumps(v, 0);
        return s ? s : lv_dup("");
    }
    }
}

/* ------------------------------------------------------------------ */
/* case-insensitive tail match against a field-suffix table            */
/* ------------------------------------------------------------------ */

static int lv_tail_suffix(const char *snaked, const char *const *suffixes, size_t n)
{
    char *low = lv_lower_dup(snaked);
    size_t i;
    int hit = 0;
    if (!low) return 0;
    for (i = 0; i < n && !hit; i++) {
        size_t off, sl;
        if (!lv_ends(low, suffixes[i])) continue;
        sl = strlen(suffixes[i]);
        off = strlen(low) - sl;
        if (off == 0 || low[off - 1] == '_') hit = 1;
    }
    free(low);
    return hit;
}

/* ------------------------------------------------------------------ */
/* ordered, de-duplicated name set                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    char **items;
    size_t n, cap;
} lv_names;

static int lv_names_push_owned(lv_names *set, char *owned)
{
    size_t i;
    if (!owned) return 0;
    for (i = 0; i < set->n; i++) {
        if (strcmp(set->items[i], owned) == 0) { free(owned); return 1; }
    }
    if (set->n == set->cap) {
        size_t cap = set->cap ? set->cap * 2 : 8;
        char **grown = realloc(set->items, cap * sizeof *grown);
        if (!grown) { free(owned); return 0; }
        set->items = grown;
        set->cap = cap;
    }
    set->items[set->n++] = owned;
    return 1;
}

static void lv_names_free(lv_names *set)
{
    size_t i;
    for (i = 0; i < set->n; i++) free(set->items[i]);
    free(set->items);
    set->items = NULL;
    set->n = set->cap = 0;
}

static int lv_name_cmp(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* sorted array: Python compares the set as a set, the wire form needs an order. */
static json_t *lv_names_to_json(lv_names *set)
{
    json_t *arr = json_array();
    size_t i;
    if (set->n > 1) qsort(set->items, set->n, sizeof *set->items, lv_name_cmp);
    for (i = 0; i < set->n; i++) json_append(arr, json_string(set->items[i]));
    return arr;
}

/* membership of a JSON array of strings (null / non-array -> empty) */
static int lv_in_array(const json_t *arr, const char *s)
{
    size_t i, n;
    if (!s || !arr || arr->type != JSON_ARRAY) return 0;
    n = json_len(arr);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(arr, i);
        if (e && e->type == JSON_STRING && e->str_val && strcmp(e->str_val, s) == 0) return 1;
    }
    return 0;
}

static int lv_in_static_blocklist(const char *s)
{
    size_t i;
    if (!s) return 0;
    for (i = 0; i < LV_STATIC_PROVIDER_BLOCKLIST_N; i++) {
        if (strcmp(LV_STATIC_PROVIDER_BLOCKLIST[i], s) == 0) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* _snake_field                                                        */
/* ------------------------------------------------------------------ */
/* re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", str(key)) */
json_t *w31_localenvpo_3ff534_snake_field(const json_t *key)
{
    char *raw = lv_py_str(key);
    size_t n, i, o = 0;
    char *out;
    if (!raw) return json_string("");
    n = strlen(raw);
    out = malloc(n * 2 + 1);
    if (!out) { free(raw); return json_string(""); }
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)raw[i];
        if (i > 0) {
            unsigned char p = (unsigned char)raw[i - 1];
            int prev = (p >= 'a' && p <= 'z') || (p >= '0' && p <= '9');
            if (prev && c >= 'A' && c <= 'Z') out[o++] = '_';
        }
        out[o++] = (char)c;
    }
    out[o] = '\0';
    free(raw);
    {
        json_t *r = json_string(out);
        free(out);
        return r;
    }
}

/* ------------------------------------------------------------------ */
/* _matches_terminal_first_party_prefix                                */
/* ------------------------------------------------------------------ */
json_t *w31_localenvpo_3ff534_matches_terminal_first_party_prefix(const json_t *name)
{
    char *raw = lv_py_str(name);
    char *up = lv_upper_dup(raw ? raw : "");
    int hit = lv_starts(up, LV_TERMINAL_FIRST_PARTY_PREFIXES[0]);
    free(raw);
    free(up);
    return json_bool(hit ? true : false);
}

/* ------------------------------------------------------------------ */
/* _is_hermes_internal_secret                                          */
/* ------------------------------------------------------------------ */
json_t *w31_localenvpo_3ff534_is_hermes_internal_secret(const json_t *key)
{
    char *raw = lv_py_str(key);
    char *up = lv_upper_dup(raw ? raw : "");
    int hit = 0;

    if (lv_starts(up, LV_BRIDGED_MARKER_PREFIX)) {
        hit = 1;
    } else if (lv_starts(up, "AUXILIARY_")
               && (lv_ends(up, "_API_KEY") || lv_ends(up, "_BASE_URL"))) {
        hit = 1;
    } else if (strcmp(up, "HERMES_MODEL_API_KEY") == 0
               || strcmp(up, "HERMES_DELEGATION_API_KEY") == 0) {
        hit = 1;
    } else if ((lv_starts(up, "HERMES_CUSTOM_") || lv_starts(up, "HERMES_PROVIDER_"))
               && lv_ends(up, "_API_KEY")) {
        hit = 1;
    } else if (lv_starts(up, "GATEWAY_RELAY_")
               && (lv_ends(up, "_SECRET") || lv_ends(up, "_KEY") || lv_ends(up, "_TOKEN"))) {
        hit = 1;
    }
    free(raw);
    free(up);
    return json_bool(hit ? true : false);
}

/* ------------------------------------------------------------------ */
/* _is_provider_env_blocklisted                                        */
/* ------------------------------------------------------------------ */
json_t *w31_localenvpo_3ff534_is_provider_env_blocklisted(const json_t *name,
                                                           const json_t *extra,
                                                           const json_t *registered)
{
    char *raw = lv_py_str(name);
    char *up = lv_upper_dup(raw ? raw : "");
    int hit = 0;

    /* `name in _HERMES_PROVIDER_ENV_BLOCKLIST or upper in _HERMES_PROVIDER_ENV_BLOCKLIST` */
    if (lv_in_static_blocklist(raw) || lv_in_static_blocklist(up)
        || lv_in_array(extra, raw) || lv_in_array(extra, up)) {
        hit = 1;
    } else if (lv_in_array(registered, up)) {
        /* the per-profile sets are already upper-folded; the lookup is not */
        hit = 1;
    }
    free(raw);
    free(up);
    return json_bool(hit ? true : false);
}

/* ------------------------------------------------------------------ */
/* is_profile_gate_env                                                 */
/* ------------------------------------------------------------------ */
static int lv_profile_gate_str(const char *raw, const json_t *prefixes)
{
    char *up = lv_upper_dup(raw ? raw : "");
    size_t i, n;
    int marker = 0, hit = 0;

    if (lv_starts(up, "HERMES_") || lv_starts(up, "_")) {
        free(up);
        return 0;
    }
    for (i = 0; i < LV_PROFILE_GATE_MARKERS_N && !marker; i++) {
        if (lv_contains(up, LV_PROFILE_GATE_MARKERS[i])) marker = 1;
    }
    if (marker) {
        if (prefixes && prefixes->type == JSON_ARRAY) {
            n = json_len(prefixes);
            for (i = 0; i < n && !hit; i++) {
                const json_t *p = json_get(prefixes, i);
                if (!p || p->type != JSON_STRING || !p->str_val) continue;
                if (lv_starts(up, p->str_val) && up[strlen(p->str_val)] == '_') hit = 1;
            }
        } else {
            for (i = 0; i < LV_EXTRA_GATE_PREFIXES_N && !hit; i++) {
                size_t pl = strlen(LV_EXTRA_GATE_PREFIXES[i]);
                if (lv_starts(up, LV_EXTRA_GATE_PREFIXES[i]) && up[pl] == '_') hit = 1;
            }
        }
    }
    free(up);
    return hit;
}

json_t *w31_localenvpo_3ff534_is_profile_gate_env(const json_t *name,
                                                  const json_t *prefixes)
{
    char *raw = lv_py_str(name);
    int hit = lv_profile_gate_str(raw, prefixes);
    free(raw);
    return json_bool(hit ? true : false);
}

/* ------------------------------------------------------------------ */
/* carry_gateway_profile_pin / strip_supervised_process_identity       */
/* ------------------------------------------------------------------ */
static int lv_gateway_pin_marker(const json_t *env)
{
    const json_t *child, *sup;
    char *raw, *stripped, *low;
    int pin = 0;

    if (!env || env->type != JSON_OBJECT) return 0;
    child = json_obj_get(env, "HERMES_SUPERVISED_CHILD");
    if (lv_truthy(child)) return 1;
    sup = json_obj_get(env, "HERMES_GATEWAY_EXTERNAL_SUPERVISOR");
    raw = lv_py_str(sup);
    stripped = lv_strip_dup(raw ? raw : "");
    low = lv_lower_dup(stripped ? stripped : "");
    free(raw);
    free(stripped);
    if (low && (strcmp(low, "1") == 0 || strcmp(low, "true") == 0
                || strcmp(low, "yes") == 0 || strcmp(low, "on") == 0)) {
        pin = 1;
    }
    free(low);
    return pin;
}

json_t *w31_localenvpo_3ff534_carry_gateway_profile_pin(const json_t *env)
{
    json_t *out = (env && env->type == JSON_OBJECT) ? json_copy(env) : json_object();
    if (out && lv_gateway_pin_marker(env)) {
        json_set(out, LV_GATEWAY_PROFILE_PIN_ENV, json_string("1"));
    }
    return out;
}

json_t *w31_localenvpo_3ff534_strip_supervised_process_identity(const json_t *env)
{
    json_t *out = w31_localenvpo_3ff534_carry_gateway_profile_pin(env);
    size_t i;
    if (!out || out->type != JSON_OBJECT) return out ? out : json_object();
    for (i = 0; i < LV_SUPERVISED_IDENTITY_N; i++) {
        json_obj_del(out, LV_SUPERVISED_IDENTITY_VARS[i]);
    }
    return out;
}

/* ------------------------------------------------------------------ */
/* strip_profile_gate_env                                              */
/* ------------------------------------------------------------------ */
json_t *w31_localenvpo_3ff534_strip_profile_gate_env(const json_t *env,
                                                     const json_t *prefixes)
{
    json_t *out = (env && env->type == JSON_OBJECT) ? json_copy(env) : json_object();
    size_t i = 0;
    if (!out || out->type != JSON_OBJECT) return out ? out : json_object();
    while (i < json_object_size(out)) {
        const char *k = json_object_get_key_at(out, i);
        if (k && lv_profile_gate_str(k, prefixes)) {
            char *doomed = lv_dup(k);
            if (doomed) {
                json_obj_del(out, doomed);
                free(doomed);
            }
            continue;  /* json_obj_del shifts the tail left: same index next */
        }
        i++;
    }
    return out;
}

/* ------------------------------------------------------------------ */
/* _config_declared_key_envs                                           */
/* ------------------------------------------------------------------ */
static void lv_walk_declared_key_envs(const json_t *node, lv_names *out)
{
    size_t i, n;
    if (!node) return;
    if (node->type == JSON_ARRAY) {
        n = json_len(node);
        for (i = 0; i < n; i++) lv_walk_declared_key_envs(json_get(node, i), out);
        return;
    }
    if (node->type != JSON_OBJECT) return;
    n = json_object_size(node);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(node, i);
        const json_t *v = json_object_get_at(node, i);
        if (k && v && v->type == JSON_STRING) {
            json_t *snake = w31_localenvpo_3ff534_snake_field(json_string(k));
            int field = snake->str_val
                        && lv_tail_suffix(snake->str_val, LV_KEY_ENV_SUFFIXES, LV_KEY_ENV_SUFFIXES_N);
            json_free(snake);
            if (field && v->str_val) {
                char *name = lv_strip_dup(v->str_val);
                if (name && name[0]) lv_names_push_owned(out, name);
                else free(name);
            }
        }
        lv_walk_declared_key_envs(v, out);
    }
}

json_t *w31_localenvpo_3ff534_config_declared_key_envs(const json_t *node)
{
    lv_names set = { NULL, 0, 0 };
    json_t *arr;
    lv_walk_declared_key_envs(node, &set);
    arr = lv_names_to_json(&set);
    lv_names_free(&set);
    return arr;
}

/* ------------------------------------------------------------------ */
/* _config_credential_env_refs                                         */
/* ------------------------------------------------------------------ */
/* _is_non_env_secret_ref: ":" in ref and re.match(r"^[a-z][a-z0-9_-]*:", ref) */
static int lv_non_env_secret_ref(const char *ref)
{
    const char *p;
    if (!ref || !strchr(ref, ':')) return 0;
    if (!(ref[0] >= 'a' && ref[0] <= 'z')) return 0;
    for (p = ref + 1; *p; p++) {
        if (*p == ':') return 1;
        if ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')
            || *p == '_' || *p == '-') continue;
        return 0;
    }
    return 0;
}

/* _env_ref_var_name over one ${...} body. */
static char *lv_env_ref_var_name(const char *body)
{
    char *t = lv_strip_dup(body ? body : "");
    char *name = NULL;
    if (!t) return NULL;
    if (lv_starts(t, "env:")) {
        char *rest = lv_strip_dup(t + 4);
        if (rest && rest[0]) name = rest;
        else free(rest);
    } else if (lv_non_env_secret_ref(t)) {
        name = NULL;
    } else {
        name = lv_dup(t);
    }
    free(t);
    return name;
}

/* _ENV_REF_RE = \$\{([^}]+)\} - non-overlapping finditer. */
static void lv_refs_in_value(const char *value, lv_names *out)
{
    const char *p = value;
    if (!p) return;
    while ((p = strstr(p, "${")) != NULL) {
        const char *body = p + 2;
        const char *close = strchr(body, '}');
        if (!close || close == body) { p += 2; continue; }
        {
            size_t l = (size_t)(close - body);
            char *raw = malloc(l + 1);
            if (raw) {
                memcpy(raw, body, l);
                raw[l] = '\0';
                {
                    char *name = lv_env_ref_var_name(raw);
                    if (name && name[0]) lv_names_push_owned(out, name);
                    else free(name);
                }
                free(raw);
            }
        }
        p = close + 1;
    }
}

static void lv_walk_credential_refs(const json_t *item, lv_names *out)
{
    size_t i, n;
    if (!item) return;
    if (item->type == JSON_ARRAY) {
        n = json_len(item);
        for (i = 0; i < n; i++) lv_walk_credential_refs(json_get(item, i), out);
        return;
    }
    if (item->type != JSON_OBJECT) return;
    n = json_object_size(item);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(item, i);
        const json_t *v = json_object_get_at(item, i);
        if (k && v && v->type == JSON_STRING) {
            json_t *snake = w31_localenvpo_3ff534_snake_field(json_string(k));
            int field = snake->str_val
                        && lv_tail_suffix(snake->str_val, LV_CREDENTIAL_SUFFIXES, LV_CREDENTIAL_SUFFIXES_N);
            json_free(snake);
            if (field) {
                lv_refs_in_value(v->str_val, out);
                continue;
            }
        }
        lv_walk_credential_refs(v, out);
    }
}

json_t *w31_localenvpo_3ff534_config_credential_env_refs(const json_t *node)
{
    lv_names set = { NULL, 0, 0 };
    json_t *arr;
    size_t i, n;

    if (!node || node->type != JSON_OBJECT) return json_array();
    n = json_object_size(node);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(node, i);
        const json_t *v = json_object_get_at(node, i);
        if (k && strcmp(k, "mcp_servers") == 0) continue;
        lv_walk_credential_refs(v, &set);
        (void)k;
    }
    /* model.api is read as the main model's key; elsewhere `api` is a base-URL alias. */
    {
        const json_t *model = json_obj_get(node, "model");
        if (model && model->type == JSON_OBJECT) {
            const json_t *api = json_obj_get(model, "api");
            if (api && api->type == JSON_STRING) lv_refs_in_value(api->str_val, &set);
        }
    }
    arr = lv_names_to_json(&set);
    lv_names_free(&set);
    return arr;
}

/* ------------------------------------------------------------------ */
/* constants                                                           */
/* ------------------------------------------------------------------ */
json_t *w31_localenvpo_3ff534_constants(void)
{
    json_t *o = json_object();
    json_t *arr;
    size_t i;

    json_set(o, "gateway_profile_pin_env", json_string(LV_GATEWAY_PROFILE_PIN_ENV));
    arr = json_array();
    for (i = 0; i < LV_SUPERVISED_IDENTITY_N; i++)
        json_append(arr, json_string(LV_SUPERVISED_IDENTITY_VARS[i]));
    json_set(o, "supervised_process_identity_vars", arr);
    arr = json_array();
    for (i = 0; i < sizeof LV_TERMINAL_FIRST_PARTY_PREFIXES / sizeof *LV_TERMINAL_FIRST_PARTY_PREFIXES; i++)
        json_append(arr, json_string(LV_TERMINAL_FIRST_PARTY_PREFIXES[i]));
    json_set(o, "terminal_first_party_env_prefixes", arr);
    arr = json_array();
    for (i = 0; i < LV_PROFILE_GATE_MARKERS_N; i++)
        json_append(arr, json_string(LV_PROFILE_GATE_MARKERS[i]));
    json_set(o, "profile_gate_env_markers", arr);
    arr = json_array();
    for (i = 0; i < LV_EXTRA_GATE_PREFIXES_N; i++)
        json_append(arr, json_string(LV_EXTRA_GATE_PREFIXES[i]));
    json_set(o, "extra_gate_env_prefixes", arr);
    json_set(o, "bridged_env_marker_prefix", json_string(LV_BRIDGED_MARKER_PREFIX));
    json_set(o, "static_provider_env_blocklist_size",
             json_number((double)LV_STATIC_PROVIDER_BLOCKLIST_N));
    return o;
}
