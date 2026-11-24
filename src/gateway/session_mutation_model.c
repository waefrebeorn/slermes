/*
 * session_mutation_model.c — C home for upstream Hermes
 * gateway/session_mutation_model.py.
 *
 * PoP: w18_sessionmut_44a137_selected_provider_key_env @ gateway/session_mutation_model.py:_selected_provider_key_env
 * PoP: w18_sessionmut_44a137_is_model_reset @ gateway/session_mutation_model.py:is_model_reset
 */
#include "noc_w18_sessionmut_44a137.h"

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Verbatim copy of hermes_cli.model_switch.MODEL_SWITCH_ERROR_TEXT[reset_with_flags]. */
static const char *RESET_WITH_FLAGS_DETAIL =
    "/model reset takes no flags — it clears this session's model override so the "
    "configured default applies again.";

static int is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\v' || c == '\f';
}

/* Python str.strip(): trim ASCII whitespace, write NUL at the new length. */
static char *strip_dup(const char *s) {
    size_t n, a, b;
    char *out;
    if (!s) return NULL;
    a = 0; n = strlen(s); b = n;
    while (a < b && is_ws((unsigned char)s[a])) ++a;
    while (b > a && is_ws((unsigned char)s[b - 1])) --b;
    if (a == b) return NULL;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *strip_lower_dup(const char *s) {
    char *out = strip_dup(s);
    size_t i;
    if (!out) return NULL;
    for (i = 0; out[i]; ++i) out[i] = (char)tolower((unsigned char)out[i]);
    return out;
}

struct json_t *w18_sessionmut_44a137_selected_provider_key_env(
    const struct json_t *config, const char *provider) {
    json_t *providers = config ? json_obj_get(config, "providers") : NULL;
    json_t *entry;
    const char *v;
    char *stripped;
    json_t *out;

    if (!providers || providers->type != JSON_OBJECT || !provider)
        return json_null();
    entry = json_obj_get(providers, provider);
    if (!entry || entry->type != JSON_OBJECT) return json_null();

    v = json_get_str(entry, "key_env", NULL);
    if (!v || v[0] == '\0') v = json_get_str(entry, "api_key_env", NULL);
    stripped = strip_dup(v);
    if (!stripped) return json_null();
    out = json_string(stripped);
    free(stripped);
    return out;
}

struct json_t *w18_sessionmut_44a137_is_model_reset(const struct json_t *payload) {
    json_t *modelv;
    char *model;
    size_t i;

    if (!payload || payload->type != JSON_OBJECT) return json_bool(0);
    modelv = json_obj_get(payload, "model");
    if (!modelv || modelv->type != JSON_STRING) return json_bool(0);
    model = strip_lower_dup(modelv->str_val);
    if (!model || strcmp(model, "reset") != 0) {
        free(model);
        return json_bool(0);
    }
    free(model);

    /* set(payload) - {'model', 'confirm'} -> AuthorityRefusal('invalid_params', ...) */
    for (i = 0; i < json_object_size(payload); ++i) {
        const char *k = json_object_get_key_at(payload, i);
        if (k && strcmp(k, "model") && strcmp(k, "confirm")) {
            json_t *out = json_object();
            json_set(out, "refused", json_string("invalid_params"));
            json_set(out, "field", json_string("payload"));
            json_set(out, "detail", json_string(RESET_WITH_FLAGS_DETAIL));
            return out;
        }
    }
    return json_bool(1);
}
