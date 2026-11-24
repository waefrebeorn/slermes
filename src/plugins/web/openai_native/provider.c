/*
 * provider.c -- real C11 port of the OpenAI-native web search provider in
 * upstream Hermes plugins/web/openai_native/provider.py.
 *
 * PoP: w20_provider_8409e9_has_credentials @ plugins/web/openai_native/provider.py:has_codex_credentials
 * PoP: w20_provider_8409e9_search_unsupported @ plugins/web/openai_native/provider.py:OpenAINativeWebSearchProvider.search
 * PoP: w20_provider_8409e9_setup_schema @ plugins/web/openai_native/provider.py:OpenAINativeWebSearchProvider.get_setup_schema
 * PoP: w20_provider_8409e9_identity @ plugins/web/openai_native/provider.py:OpenAINativeWebSearchProvider
 *
 * The auth store arrives as the auth.json body: the probe is deliberately a
 * fast disk read upstream, so the port takes the bytes the read would return.
 */
#include "hermes_json.h"
#include "noc_w20_provider_8409e9.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ON_NAME    "openai-native"
#define ON_DISPLAY "OpenAI Native Web Search (Codex Responses)"

static const char *const ON_UNSUPPORTED_MSG =
    "openai-native declares OpenAI's server-side web_search tool; it cannot run as a "
    "client-side search and requires the Codex Responses transport (provider "
    "openai-codex). For client-side search use firecrawl (default) or another backend.";

static const char *const ON_TAG =
    "Search runs on the provider side (needs the Codex Responses transport + an "
    "openai-codex login); search only, extraction still uses another backend";

static int on_truthy(const json_t *v) {
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

static const json_t *on_dget(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}

static int on_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\v' || c == '\f';
}

/* `str(value or "").strip()` -- truthy when anything survives the strip. */
static int on_token_live(const json_t *value) {
    char buf[64];
    const char *text = NULL;
    size_t start = 0, end;
    if (!on_truthy(value)) return 0;
    switch (value->type) {
    case JSON_STRING:
        text = value->str_val ? value->str_val : "";
        break;
    case JSON_BOOL:
        text = value->bool_val ? "True" : "False";
        break;
    case JSON_NUMBER:
        if (value->num_val == (double)(long long)value->num_val)
            snprintf(buf, sizeof buf, "%lld", (long long)value->num_val);
        else
            snprintf(buf, sizeof buf, "%g", value->num_val);
        text = buf;
        break;
    default:
        return 1;   /* an object/list str() is non-empty and survives strip() */
    }
    end = strlen(text);
    while (start < end && on_space(text[start])) start++;
    while (end > start && on_space(text[end - 1])) end--;
    return end > start;
}

int w20_provider_8409e9_has_credentials(const char *auth_text, int file_exists) {
    const char *text = auth_text ? auth_text : "";
    json_t *store;
    const json_t *tokens, *entries;
    size_t i;
    int found = 0;

    if (!file_exists) return 0;
    /* Path.read_text(encoding="utf-8-sig"): a leading BOM is not part of the body. */
    if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF)
        text += 3;
    store = json_parse(text, NULL);
    if (!store) return 0;             /* the upstream try/except returns False */
    tokens = on_dget(on_dget(on_dget(store, "providers"), "openai-codex"), "tokens");
    if (on_token_live(on_dget(tokens, "access_token"))) {
        json_free(store);
        return 1;
    }
    entries = on_dget(on_dget(store, "credential_pool"), "openai-codex");
    if (entries && entries->type == JSON_ARRAY) {
        for (i = 0; i < entries->c.count; i++) {
            json_t *entry = entries->c.items[i];
            if (entry && entry->type == JSON_OBJECT &&
                on_token_live(json_obj_get(entry, "access_token"))) {
                found = 1;
                break;
            }
        }
    }
    json_free(store);
    return found;
}

json_t *w20_provider_8409e9_search_unsupported(void) {
    json_t *o = json_object();
    json_set(o, "success", json_bool(false));
    json_set(o, "error", json_string(ON_UNSUPPORTED_MSG));
    return o;
}

json_t *w20_provider_8409e9_setup_schema(void) {
    json_t *o = json_object();
    json_set(o, "name", json_string(ON_DISPLAY));
    json_set(o, "badge", json_string("native"));
    json_set(o, "tag", json_string(ON_TAG));
    json_set(o, "env_vars", json_array());
    return o;
}

json_t *w20_provider_8409e9_identity(void) {
    json_t *o = json_object();
    json_set(o, "name", json_string(ON_NAME));
    json_set(o, "display_name", json_string(ON_DISPLAY));
    json_set(o, "extract", json_bool(false));
    return o;
}
