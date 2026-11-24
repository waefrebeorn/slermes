/*
 * client.c - real C11 port of upstream Hermes plugins/model-providers/gemini/client.py
 *   pure routing helpers, plus the two adapter predicates this module routes on
 *   (agent/gemini_native_adapter.py: bare_gemini_model_id / gemini_uses_thinking_level).
 * PoP: w19_client_ead47a_bare_model_id @ agent/gemini_native_adapter.py:bare_gemini_model_id
 * PoP: w19_client_ead47a_uses_thinking_level @ agent/gemini_native_adapter.py:gemini_uses_thinking_level
 * PoP: w19_client_ead47a_configured_api @ plugins/model-providers/gemini/client.py:_configured_api
 * PoP: w19_client_ead47a_uses_interactions @ plugins/model-providers/gemini/client.py:uses_interactions
 * PoP: w19_client_ead47a_continues @ plugins/model-providers/gemini/client.py:GeminiClient._continues
 */
#include "hermes_json.h"
#include "noc_w19_client_ead47a.h"

#define API_INTERACTIONS      "interactions"
#define API_GENERATE_CONTENT  "generate_content"
#define API_AUTO              "auto"
#define INTERACTIONS_HOST     "generativelanguage.googleapis.com"

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *d = (char *)malloc(n + 1);
    if (!d) return NULL;
    if (n) memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

static int py_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str.strip() -> malloc'd copy. */
static char *py_strip(const char *s) {
    if (!s) return dupstr("");
    const char *b = s;
    while (*b && py_space(*b)) b++;
    const char *e = b + strlen(b);
    while (e > b && py_space(e[-1])) e--;
    char *out = (char *)malloc((size_t)(e - b) + 1);
    if (!out) return NULL;
    memcpy(out, b, (size_t)(e - b));
    out[e - b] = '\0';
    return out;
}

static void lower_inplace(char *s) {
    for (; s && *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

/* Python truthiness of a JSON value (`x or fallback`). */
static int jobj_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

/* bare_gemini_model_id -> malloc'd string (caller frees). */
static char *bare_alloc(const char *model) {
    char *name = py_strip(model);
    if (!name) return NULL;
    char *low = dupstr(name);
    if (!low) return name;          /* no scratch copy: the stripped name is the answer */
    lower_inplace(low);
    static const char *const PREFIXES[] = { "google/", "gemini/", NULL };
    for (int i = 0; PREFIXES[i]; i++) {
        size_t plen = strlen(PREFIXES[i]);
        if (!strncmp(low, PREFIXES[i], plen)) {
            char *rest = py_strip(name + plen);
            if (rest && rest[0]) {
                free(name);
                free(low);
                return rest;
            }
            /* `name[plen:].strip() or name` - an empty remainder answers the stripped name. */
            free(rest);
            free(low);
            return name;
        }
    }
    free(low);
    return name;
}

/* re.match(r"gemini-(\d+)(?:\.(\d+))?", bare.lower()) -> (major, minor); (0,0) when it fails. */
static void gemini_version(const char *model, int *major, int *minor) {
    *major = 0;
    *minor = 0;
    char *b = bare_alloc(model);
    if (!b) return;
    lower_inplace(b);
    const char *p = b;
    if (strncmp(p, "gemini-", 7) != 0) { free(b); return; }
    p += 7;
    if (*p < '0' || *p > '9') { free(b); return; }
    long maj = 0;
    while (*p >= '0' && *p <= '9') { maj = maj * 10 + (*p - '0'); p++; }
    long mn = 0;
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') { mn = mn * 10 + (*p - '0'); p++; }
    }
    *major = (int)maj;
    *minor = (int)mn;
    free(b);
}

static char *ring_dup(const char *s) {
    static char *slots[4];
    static unsigned idx;
    unsigned i = idx++ % 4u;
    free(slots[i]);
    slots[i] = dupstr(s ? s : "");
    return slots[i];
}

const char *w19_client_ead47a_bare_model_id(const char *model) {
    char *b = bare_alloc(model);
    const char *out = ring_dup(b ? b : "");
    free(b);
    return out;
}

int w19_client_ead47a_uses_thinking_level(const char *model) {
    char *b = bare_alloc(model);
    if (!b) return 0;
    lower_inplace(b);
    size_t n = strlen(b);
    int latest = (n >= 7 && !strcmp(b + n - 7, "-latest"));
    int gemini = !strncmp(b, "gemini-", 7);
    free(b);
    int major = 0, minor = 0;
    gemini_version(model, &major, &minor);
    if (gemini && latest) return 1;
    return major >= 3 ? 1 : 0;
}

static const char *intern_choice(const char *s) {
    if (!strcmp(s, API_INTERACTIONS)) return API_INTERACTIONS;
    if (!strcmp(s, API_GENERATE_CONTENT)) return API_GENERATE_CONTENT;
    if (!strcmp(s, API_AUTO)) return API_AUTO;
    return NULL;
}

/* source.get("gemini_api") coerced the way the live helper does: a string, stripped and
 * lowercased, that must name one of the three settings. */
static const char *choice_of(json_t *source) {
    if (!source || source->type != JSON_OBJECT) return NULL;
    json_t *v = json_obj_get(source, "gemini_api");
    if (!v || v->type != JSON_STRING) return NULL;
    char *t = py_strip(v->str_val);
    if (!t) return NULL;
    lower_inplace(t);
    const char *c = intern_choice(t);
    free(t);
    return c;
}

const char *w19_client_ead47a_configured_api(const char *model, const char *bare,
                                             int config_ok, json_t *config) {
    if (!config_ok) return API_AUTO;                       /* unreadable config keeps the default */
    if (!config || config->type != JSON_OBJECT) return API_AUTO;
    json_t *providers = json_obj_get(config, "providers");
    json_t *provider = NULL;
    if (providers && providers->type == JSON_OBJECT)
        provider = json_obj_get(providers, "gemini");
    if (!provider || provider->type != JSON_OBJECT) return API_AUTO;

    json_t *models = json_obj_get(provider, "models");
    json_t *per_model = NULL;
    if (models && models->type == JSON_OBJECT) {
        json_t *by_model = json_obj_get(models, model ? model : "");
        if (jobj_truthy(by_model)) per_model = by_model;
        else if (bare) per_model = json_obj_get(models, bare);
    }
    /* The first valid setting wins, an explicit per-model "auto" included. */
    const char *c = choice_of(per_model);
    if (!c) c = choice_of(provider);
    return c ? c : API_AUTO;
}

static int contains_ci(const char *hay, const char *needle) {
    if (!hay || !needle || !*needle) return 0;
    size_t n = strlen(needle);
    for (const char *p = hay; *p; p++)
        if (!strncmp(p, needle, n)) return 1;
    return 0;
}

int w19_client_ead47a_uses_interactions(const char *model, const char *base_url,
                                        int config_ok, json_t *config) {
    char *base = py_strip(base_url ? base_url : "");
    if (base) lower_inplace(base);
    int host = base && contains_ci(base, INTERACTIONS_HOST);
    free(base);
    if (!host) return 0;
    char *bare = bare_alloc(model);
    char *stripped = py_strip(model ? model : "");
    const char *choice = w19_client_ead47a_configured_api(stripped, bare ? bare : "",
                                                          config_ok, config);
    int thinking = w19_client_ead47a_uses_thinking_level(bare ? bare : "");
    free(bare);
    free(stripped);
    if (!strcmp(choice, API_INTERACTIONS)) return 1;
    if (!strcmp(choice, API_AUTO) && thinking) return 1;
    return 0;
}

int w19_client_ead47a_continues(json_t *interaction) {
    if (!interaction || interaction->type != JSON_OBJECT) return 0;
    json_t *status = json_obj_get(interaction, "status");
    if (!status || status->type != JSON_STRING || strcmp(status->str_val, "requires_continuation"))
        return 0;
    if (!jobj_truthy(json_obj_get(interaction, "id"))) return 0;
    json_t *tok = json_obj_get(interaction, "continuation_token");
    if (!tok || tok->type != JSON_STRING) return 0;
    return tok->str_val && tok->str_val[0] ? 1 : 0;
}
