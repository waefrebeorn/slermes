/*
 * session_client_env.c — C home for upstream Hermes agent/session_client_env.py.
 *
 * Faithful C11 ports of the client_env allowlist / normalisation / retry rules.
 * The frozen secret-substring policy (tools.code_execution_env._SECRET_SUBSTRINGS)
 * is injected through the args object as "secret" so the C side never imports
 * live interpreter state.
 *
 * PoP: w10_sessioncli_766251_configured_names @ agent/session_client_env.py:configured_names
 * PoP: w10_sessioncli_766251_client_env_rules @ agent/session_client_env.py:client_env_rules
 * PoP: w10_sessioncli_766251_travels @ agent/session_client_env.py:_travels
 * PoP: w10_sessioncli_766251_normalise @ agent/session_client_env.py:normalise_client_env
 * PoP: w10_sessioncli_766251_accept @ agent/session_client_env.py:accept_client_env
 * PoP: w10_sessioncli_766251_retry_matches @ agent/session_client_env.py:retry_matches
 */
#include "noc_w10_sessioncli_766251.h"

#include <string.h>

#define CONFIG_KEY "client_env_passthrough"
#define MAX_NAMES 256
#define MAX_BYTES (256 * 1024)

static const char *BEHAVIOUR_ENV[] = {
    "HERMES_EPHEMERAL_SYSTEM_PROMPT", "HERMES_PREFILL_MESSAGES_FILE",
    "HERMES_INFERENCE_PROVIDER", "HERMES_ENABLE_PROJECT_PLUGINS",
};
static const char *HOST_BEHAVIOUR[] = {
    "HERMES_PREFILL_MESSAGES_FILE", "HERMES_ENABLE_PROJECT_PLUGINS",
};
static const char *TOOL_ENV[] = {
    "PATH", "VIRTUAL_ENV", "GOPATH", "GOROOT", "CARGO_HOME", "RUSTUP_HOME", "JAVA_HOME",
};
static const char *TOOL_PREFIXES[] = { "CONDA_", "NODE_", "NVM_", "PYENV_" };
#define N_BEH (sizeof(BEHAVIOUR_ENV) / sizeof(BEHAVIOUR_ENV[0]))
#define N_HOST (sizeof(HOST_BEHAVIOUR) / sizeof(HOST_BEHAVIOUR[0]))
#define N_TOOL (sizeof(TOOL_ENV) / sizeof(TOOL_ENV[0]))
#define N_PREFIX (sizeof(TOOL_PREFIXES) / sizeof(TOOL_PREFIXES[0]))

static char *dupn(const char *s, size_t n) {
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n); r[n] = '\0'; return r;
}
static char *py_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r' || s[a] == '\f' || s[a] == '\v')) a++;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\n' || s[b-1] == '\r' || s[b-1] == '\f' || s[b-1] == '\v')) b--;
    return dupn(s + a, b - a);
}
static char *py_upper(const char *s) {
    size_t n = strlen(s);
    char *r = dupn(s, n);
    for (size_t i = 0; i < n; i++) if (r[i] >= 'a' && r[i] <= 'z') r[i] = (char)(r[i] - 'a' + 'A');
    return r;
}
static int str_in_arr(const char *s, const char *const *arr, size_t n) {
    for (size_t i = 0; i < n; i++) if (!strcmp(s, arr[i])) return 1;
    return 0;
}
static int starts_any(const char *s, const char *const *arr, size_t n) {
    for (size_t i = 0; i < n; i++) if (!strncmp(s, arr[i], strlen(arr[i]))) return 1;
    return 0;
}

typedef struct { char **v; size_t n, cap; } StrSet;
static void set_add(StrSet *s, char *name) {
    if (!name) return;
    for (size_t i = 0; i < s->n; i++) if (!strcmp(s->v[i], name)) { free(name); return; }
    if (s->n == s->cap) { s->cap = s->cap ? s->cap * 2 : 8; s->v = (char **)realloc(s->v, s->cap * sizeof(char *)); }
    s->v[s->n++] = name;
}
static void set_free(StrSet *s) { for (size_t i = 0; i < s->n; i++) free(s->v[i]); free(s->v); }
static int cmp_str(const void *a, const void *b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }
static json_t *set_sorted_json(const StrSet *s) {
    json_t *arr = json_array();
    char **cp = (char **)malloc((s->n ? s->n : 1) * sizeof(char *));
    for (size_t i = 0; i < s->n; i++) cp[i] = s->v[i];
    if (s->n > 1) qsort(cp, s->n, sizeof(char *), cmp_str);
    for (size_t i = 0; i < s->n; i++) json_append(arr, json_string(cp[i]));
    free(cp);
    return arr;
}

/* frozenset(name.strip() for name in raw if isinstance(name, str) and name.strip()) */
static void configured_names_into(StrSet *out, json_t *config)
{
    json_t *terminal = json_is_object(config) ? json_obj_get(config, "terminal") : NULL;
    json_t *raw = json_is_object(terminal) ? json_obj_get(terminal, CONFIG_KEY) : NULL;
    if (!json_is_array(raw)) return;
    for (size_t i = 0; i < json_len(raw); i++) {
        json_t *v = json_get(raw, i);
        if (!json_is_string(v)) continue;
        char *nm = py_strip(json_string_value(v));
        if (nm && *nm) set_add(out, nm); else free(nm);
    }
}

/* PoP: w10_sessioncli_766251_configured_names @ agent/session_client_env.py:configured_names */
json_t *w10_sessioncli_766251_configured_names(json_t *a)
{
    StrSet names = {0};
    configured_names_into(&names, a ? json_obj_get(a, "config") : NULL);
    json_t *out = set_sorted_json(&names);
    set_free(&names);
    return out;
}

static json_t *build_rules(json_t *config, int local, json_t *secret)
{
    json_t *rules = json_object();
    json_t *beh = json_array();
    for (size_t i = 0; i < N_BEH; i++)
        if (local || !str_in_arr(BEHAVIOUR_ENV[i], HOST_BEHAVIOUR, N_HOST))
            json_append(beh, json_string(BEHAVIOUR_ENV[i]));
    json_set(rules, "behaviour", beh);

    StrSet names = {0};
    if (local) {
        for (size_t i = 0; i < N_TOOL; i++) set_add(&names, dupn(TOOL_ENV[i], strlen(TOOL_ENV[i])));
        configured_names_into(&names, config);
    }
    json_set(rules, "names", set_sorted_json(&names));
    set_free(&names);

    json_t *prefixes = json_array();
    if (local) for (size_t i = 0; i < N_PREFIX; i++) json_append(prefixes, json_string(TOOL_PREFIXES[i]));
    json_set(rules, "prefixes", prefixes);

    json_t *sec = json_array();
    if (json_is_array(secret))
        for (size_t i = 0; i < json_len(secret); i++) {
            json_t *v = json_get(secret, i);
            if (json_is_string(v)) json_append(sec, json_copy(v));
        }
    json_set(rules, "secret", sec);
    return rules;
}

/* PoP: w10_sessioncli_766251_client_env_rules @ agent/session_client_env.py:client_env_rules */
json_t *w10_sessioncli_766251_client_env_rules(json_t *a)
{
    int local = a ? (json_get_bool(a, "local", true) ? 1 : 0) : 1;
    return build_rules(a ? json_obj_get(a, "config") : NULL, local,
                       a ? json_obj_get(a, "secret") : NULL);
}

static int travels(const char *name, json_t *secret, int fallback)
{
    int hidden = 0;
    if (json_is_array(secret)) {
        char *up = py_upper(name);
        for (size_t i = 0; i < json_len(secret); i++) {
            json_t *v = json_get(secret, i);
            if (json_is_string(v) && json_string_value(v) && strstr(up, json_string_value(v))) { hidden = 1; break; }
        }
        free(up);
    } else {
        hidden = fallback;
    }
    return !hidden && strncmp(name, "HERMES_", 7) != 0;
}

/* PoP: w10_sessioncli_766251_travels @ agent/session_client_env.py:_travels */
json_t *w10_sessioncli_766251_travels(json_t *a)
{
    const char *name = json_get_str(a, "name", "");
    int r = travels(name, a ? json_obj_get(a, "secret") : NULL,
                    a && json_get_bool(a, "is_secret", false));
    return json_bool(r);
}

static int json_str_arr_has(json_t *arr, const char *s) {
    if (!json_is_array(arr)) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (json_is_string(v) && !strcmp(json_string_value(v), s)) return 1;
    }
    return 0;
}
static int json_prefix_has(json_t *arr, const char *s) {
    if (!json_is_array(arr)) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (json_is_string(v) && !strncmp(s, json_string_value(v), strlen(json_string_value(v)))) return 1;
    }
    return 0;
}
static int kept(const char *name, json_t *rules, int fallback)
{
    if (json_str_arr_has(json_obj_get(rules, "behaviour"), name)) return 1;
    if (!travels(name, json_obj_get(rules, "secret"), fallback)) return 0;
    return json_str_arr_has(json_obj_get(rules, "names"), name)
        || json_prefix_has(json_obj_get(rules, "prefixes"), name);
}

/* normalise_client_env core: NULL means ValueError (invalid map). */
static json_t *normalise_map(json_t *value, json_t *rules, int fallback)
{
    if (!json_is_object(value) || json_object_size(value) > MAX_NAMES) return NULL;
    size_t bytes = 0;
    for (size_t i = 0; i < json_object_size(value); i++) {
        json_t *v = json_object_get_at(value, i);
        if (!json_is_string(v)) return NULL;
        bytes += strlen(json_object_get_key_at(value, i)) + strlen(json_string_value(v));
    }
    if (bytes > MAX_BYTES) return NULL;
    json_t *out = json_object();
    for (size_t i = 0; i < json_object_size(value); i++) {
        const char *k = json_object_get_key_at(value, i);
        if (!json_is_null(rules) && rules && !kept(k, rules, fallback)) continue;
        json_set(out, k, json_copy(json_object_get_at(value, i)));
    }
    return out;
}
static json_t *normalise_result(json_t *env)
{
    json_t *out = json_object();
    json_set(out, "error", json_bool(env == NULL));
    json_set(out, "env", env ? env : json_null());
    return out;
}

/* PoP: w10_sessioncli_766251_normalise @ agent/session_client_env.py:normalise_client_env */
json_t *w10_sessioncli_766251_normalise(json_t *a)
{
    json_t *env = normalise_map(a ? json_obj_get(a, "value") : NULL,
                                a ? json_obj_get(a, "rules") : NULL, 0);
    return normalise_result(env);
}

/* PoP: w10_sessioncli_766251_accept @ agent/session_client_env.py:accept_client_env */
json_t *w10_sessioncli_766251_accept(json_t *a)
{
    int local = a ? (json_get_bool(a, "local", true) ? 1 : 0) : 1;
    json_t *rules = build_rules(a ? json_obj_get(a, "config") : NULL, local,
                                a ? json_obj_get(a, "secret") : NULL);
    json_t *env = normalise_map(a ? json_obj_get(a, "value") : NULL, rules, 0);
    json_free(rules);
    return normalise_result(env);
}

static int obj_str_equal(json_t *x, json_t *y)
{
    if (!json_is_object(x) || !json_is_object(y)) return 0;
    if (json_object_size(x) != json_object_size(y)) return 0;
    for (size_t i = 0; i < json_object_size(x); i++) {
        const char *k = json_object_get_key_at(x, i);
        json_t *vx = json_object_get_at(x, i);
        json_t *vy = json_obj_get(y, k);
        if (!json_is_string(vx) || !json_is_string(vy)) return 0;
        if (strcmp(json_string_value(vx), json_string_value(vy))) return 0;
    }
    return 1;
}

/* PoP: w10_sessioncli_766251_retry_matches @ agent/session_client_env.py:retry_matches */
json_t *w10_sessioncli_766251_retry_matches(json_t *a)
{
    json_t *stored = a ? json_obj_get(a, "stored_env") : NULL;
    int has_stored = json_is_object(stored);
    int has_request = a && json_get_bool(a, "has_client_env", false);
    if (!has_stored || !has_request)
        return json_bool(!has_stored && !has_request);
    json_t *rules = a ? json_obj_get(a, "rules") : NULL;
    json_t *sent = normalise_map(a ? json_obj_get(a, "request_env") : NULL, rules, 0);
    if (!sent) return json_bool(false);
    int eq = obj_str_equal(sent, stored);
    json_free(sent);
    return json_bool(eq);
}
