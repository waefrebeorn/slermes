/*
 * client_lifecycle.c — C port of upstream Hermes agent/client_lifecycle.py.
 *
 * PoP: w8_clientlifec_20bd72_valid_credential_pair @ agent/client_lifecycle.py:_valid_credential_pair
 * PoP: w8_clientlifec_20bd72_reset_slot @ agent/client_lifecycle.py:_reset_slot
 * PoP: w8_clientlifec_20bd72_api_kwargs_have_image_parts @ agent/client_lifecycle.py:ClientLifecycleMixin._api_kwargs_have_image_parts
 * PoP: w8_clientlifec_20bd72_is_openai_client_closed @ agent/client_lifecycle.py:ClientLifecycleMixin._is_openai_client_closed
 * PoP: w8_clientlifec_20bd72_should_adopt_env_credentials @ agent/client_lifecycle.py:ClientLifecycleMixin._should_adopt_env_credentials
 * PoP: w8_clientlifec_20bd72_anthropic_log_context @ agent/client_lifecycle.py:ClientLifecycleMixin._anthropic_log_context
 *
 * Collaborators (getattr sources, Mock detection) are injected from fixtures.
 */
#include "noc_w8_clientlifec_20bd72.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_nonblank_string(const json_t *v) {
    if (!v || v->type != JSON_STRING || !v->str_val) return 0;
    for (const char *p = v->str_val; *p; p++)
        if (!isspace((unsigned char)*p)) return 1;
    return 0;
}
static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: return v->c.count > 0 ? 1 : 0;
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

int w8_clientlifec_20bd72_valid_credential_pair(const struct json_t *api_key,
                                                const struct json_t *base_url) {
    return is_nonblank_string(api_key) && is_nonblank_string(base_url) ? 1 : 0;
}

json_t *w8_clientlifec_20bd72_reset_slot(const struct json_t *cache, int in_use) {
    json_t *out = json_object();
    if (cache && cache->type == JSON_OBJECT) {
        for (size_t i = 0; i < cache->c.count; i++) {
            const char *k = cache->c.keys[i];
            if (!k) continue;
            json_set(out, k, json_copy(cache->c.items[i]));
        }
    }
    json_set(out, "client", json_null());
    json_set(out, "key", json_null());
    json_set(out, "poisoned", json_bool(false));
    json_set(out, "in_use", json_bool(in_use ? true : false));
    return out;
}

static int contains_image(const json_t *value) {
    if (!value) return 0;
    if (value->type == JSON_OBJECT) {
        const json_t *t = json_obj_get(value, "type");
        if (t && t->type == JSON_STRING && t->str_val &&
            (!strcmp(t->str_val, "image_url") || !strcmp(t->str_val, "input_image")))
            return 1;
        for (size_t i = 0; i < value->c.count; i++)
            if (contains_image(value->c.items[i])) return 1;
        return 0;
    }
    if (value->type == JSON_ARRAY) {
        for (size_t i = 0; i < value->c.count; i++)
            if (contains_image(value->c.items[i])) return 1;
    }
    return 0;
}

int w8_clientlifec_20bd72_api_kwargs_have_image_parts(const struct json_t *api_kwargs) {
    if (!api_kwargs || api_kwargs->type != JSON_OBJECT) return 0;
    static const char *FIELDS[] = {"messages", "input"};
    for (size_t f = 0; f < 2; f++) {
        json_t *lst = json_obj_get(api_kwargs, FIELDS[f]);
        if (!lst || lst->type != JSON_ARRAY) continue;
        for (size_t i = 0; i < lst->c.count; i++)
            if (contains_image(lst->c.items[i])) return 1;
    }
    return 0;
}

int w8_clientlifec_20bd72_is_openai_client_closed(const struct json_t *client) {
    if (!client || client->type != JSON_OBJECT) return 0;
    if (truthy(json_obj_get(client, "mock"))) return 0;
    const json_t *ic = json_obj_get(client, "is_closed");
    if (ic && ic->type == JSON_BOOL && ic->bool_val) return 1;
    if (ic && ic->type == JSON_OBJECT) {
        const json_t *call = json_obj_get(ic, "$call");
        if (call && call->type == JSON_BOOL && call->bool_val) return 1;
    }
    const json_t *hc = json_obj_get(client, "_client");
    if (hc && hc->type == JSON_OBJECT) {
        const json_t *hic = json_obj_get(hc, "is_closed");
        if (hic && hic->type == JSON_BOOL && hic->bool_val) return 1;
    }
    return 0;
}

/* (value or "").strip().rstrip("/") */
static char *normalized_base(const json_t *v) {
    const char *s = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    while (n > 0 && s[n - 1] == '/') n--;
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}
static int json_eq_str(const json_t *a, const json_t *b) {
    if ((!a || a->type == JSON_NULL) && (!b || b->type == JSON_NULL)) return 1;
    if (!a || !b) return 0;
    if (a->type != JSON_STRING || b->type != JSON_STRING) return 0;
    return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
}

int w8_clientlifec_20bd72_should_adopt_env_credentials(const struct json_t *args) {
    const json_t *api_key = json_obj_get(args, "api_key");
    const json_t *base_url = json_obj_get(args, "base_url");
    const json_t *default_base = json_obj_get(args, "default_base");
    const json_t *state = json_obj_get(args, "state");

    const json_t *prev = state ? json_obj_get(state, "_env_creds_seen") : NULL;
    char *current_base = normalized_base(state ? json_obj_get(state, "base_url") : NULL);
    /* unchanged = base_url == current_base and api_key == self.api_key */
    int same_base = 0, same_key = 0;
    {
        const char *bs = (base_url && base_url->type == JSON_STRING && base_url->str_val) ? base_url->str_val : "";
        same_base = base_url && base_url->type == JSON_STRING &&
                    strcmp(bs, current_base ? current_base : "") == 0;
        const json_t *self_key = state ? json_obj_get(state, "api_key") : NULL;
        same_key = json_eq_str(api_key, self_key);
    }
    int unchanged_flag = same_base && same_key;

    int result = 0;
    if (!prev || prev->type == JSON_NULL) {
        const char *db = (default_base && default_base->type == JSON_STRING && default_base->str_val) ? default_base->str_val : "";
        int pool_rotated = 0;
        if (state) {
            const json_t *pk = json_obj_get(state, "api_key");
            int key_differs = !json_eq_str(api_key, pk);
            const json_t *pool = json_obj_get(state, "_credential_pool");
            int pool_present = pool && pool->type != JSON_NULL;
            int entry = truthy(json_obj_get(state, "_credential_pool_entry_id"));
            pool_rotated = key_differs && pool_present && entry ? 1 : 0;
        }
        result = (current_base && strcmp(current_base, db) == 0) && !unchanged_flag && !pool_rotated ? 1 : 0;
    } else if (prev->type == JSON_ARRAY && prev->c.count >= 2) {
        const char *bs = (base_url && base_url->type == JSON_STRING && base_url->str_val) ? base_url->str_val : "";
        const char *ks = (api_key && api_key->type == JSON_STRING && api_key->str_val) ? api_key->str_val : "";
        const json_t *p0 = prev->c.items[0], *p1 = prev->c.items[1];
        int differs = !(json_eq_str(base_url, p0) && json_eq_str(api_key, p1));
        const char *db = (default_base && default_base->type == JSON_STRING && default_base->str_val) ? default_base->str_val : "";
        const char *p0s = (p0 && p0->type == JSON_STRING && p0->str_val) ? p0->str_val : "";
        int in_set = (strcmp(current_base ? current_base : "", db) == 0) ||
                     (strcmp(current_base ? current_base : "", p0s) == 0);
        (void)bs; (void)ks;
        result = differs && in_set && !unchanged_flag ? 1 : 0;
    }
    free(current_base);
    return result;
}

static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return NULL;
    if (v->type == JSON_STRING) return v->str_val ? (char *)v->str_val : NULL;
    if (v->type == JSON_BOOL) return v->bool_val ? "True" : "False";
    if (v->type == JSON_NUMBER) {
        static char buf[64];
        snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        return buf;
    }
    return NULL;
}

json_t *w8_clientlifec_20bd72_anthropic_log_context(const struct json_t *agent) {
    const char *p = agent ? py_str(json_obj_get(agent, "provider")) : NULL;
    const char *m = agent ? py_str(json_obj_get(agent, "model")) : NULL;
    char buf[2048];
    snprintf(buf, sizeof(buf), "provider=%s model=%s", p ? p : "None", m ? m : "None");
    return json_string(buf);
}
