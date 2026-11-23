/*
 * provider_route_hooks.c — C port of upstream Hermes agent/provider_route_hooks.py.
 *
 * PoP: w13_providerro_daa14d_freeze @ agent/provider_route_hooks.py:_freeze
 * PoP: w13_providerro_daa14d_forces_max @ agent/provider_route_hooks.py:model_forces_max_completion_tokens
 * PoP: w13_providerro_daa14d_route @ agent/provider_route_hooks.py:route_profiles
 * PoP: w13_providerro_daa14d_ask @ agent/provider_route_hooks.py:ask_profile
 * PoP: w13_providerro_daa14d_hook_failed @ agent/provider_route_hooks.py:hook_failed
 * PoP: w13_providerro_daa14d_model_api_mode @ agent/provider_route_hooks.py:model_api_mode
 * PoP: w13_providerro_daa14d_route_answer @ agent/provider_route_hooks.py:route_answer
 *
 * Profile identity/seams: profiles are fixture objects {name, overrides, answers,
 * raises, api_mode, model_mode_answer, model_mode_raises}. The provider registry
 * (get_provider_profile / profile_for_host) is driven by `providers` and `hosts`
 * maps, registry_generation is folded into each case's state, and the sticky
 * failure key (id, type, name, hook) is collapsed to (name, hook) -- fixtures use
 * one distinct object per name.
 */
#include "noc_w13_providerro_daa14d.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME_MAX 128

/* ---------- generic helpers ---------- */
static int jtruthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default: return 0;
    }
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}

static int arr_has_str(const json_t *arr, const char *s) {
    if (!json_is_array(arr) || !s) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *e = json_get(arr, i);
        if (e && e->type == JSON_STRING && !strcmp(e->str_val, s)) return 1;
    }
    return 0;
}

static const json_t *find_profile(const json_t *profiles, const char *name) {
    if (!json_is_array(profiles) || !name || !name[0]) return NULL;
    for (size_t i = 0; i < json_len(profiles); i++) {
        json_t *p = json_get(profiles, i);
        if (json_is_object(p) && !strcmp(jstr(json_obj_get(p, "name")), name)) return p;
    }
    return NULL;
}

/* profile lookup by a map (provider/base_url -> profile name) */
static const json_t *lookup_profile(const json_t *profiles, const json_t *map, const char *key) {
    if (!key || !key[0] || !json_is_object(map)) return NULL;
    json_t *nm = json_obj_get(map, key);
    if (!nm || nm->type != JSON_STRING) return NULL;
    return find_profile(profiles, nm->str_val);
}

static int profile_overrides(const json_t *profile, const char *hook) {
    return profile ? arr_has_str(json_obj_get(profile, "overrides"), hook) : 0;
}

static const char *profile_name(const json_t *profile) {
    return profile ? jstr(json_obj_get(profile, "name")) : "";
}

/* ---------- _freeze ---------- */
static json_t *freeze_rec(const json_t *v) {
    if (!v) return json_null();
    if (v->type == JSON_OBJECT) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_object_size(v); i++) {
            const char *k = json_object_get_key_at(v, i);
            if (!k) continue;
            json_t *pair = json_array();
            json_append(pair, json_string(k));
            json_append(pair, freeze_rec(json_object_get_at(v, i)));
            json_append(out, pair);
        }
        /* Python sorts tuples (key, frozen): sort by key, then serialized value. */
        for (size_t i = 1; i < json_len(out); i++) {
            json_t *cur = json_get(out, i);
            size_t j = i;
            while (j > 0) {
                json_t *prev = json_get(out, j - 1);
                const char *kc = jstr(json_get(cur, 0)), *kp = jstr(json_get(prev, 0));
                int cmp = strcmp(kc, kp);
                if (cmp == 0) {
                    char *a = json_dumps(json_get(cur, 1), 0), *b = json_dumps(json_get(prev, 1), 0);
                    cmp = strcmp(a ? a : "", b ? b : "");
                    free(a); free(b);
                }
                if (cmp >= 0) break;
                out->c.items[j - 1] = cur;
                out->c.items[j] = prev;
                j--;
            }
        }
        return out;
    }
    if (v->type == JSON_ARRAY) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(v); i++) json_append(out, freeze_rec(json_get(v, i)));
        return out;
    }
    return json_copy(v);
}

struct json_t *w13_providerro_daa14d_freeze(const json_t *value) {
    return freeze_rec(value);
}

/* ---------- model_forces_max_completion_tokens ---------- */
int w13_providerro_daa14d_forces_max(const char *model) {
    if (!model) model = "";
    const char *p = model;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    /* lowercase + take segment after last '/' */
    char seg[512];
    size_t n = 0;
    const char *last = p;
    for (const char *q = p; q < e; q++) if (*q == '/') last = q + 1;
    for (const char *q = last; q < e && n + 1 < sizeof(seg); q++)
        seg[n++] = (char)tolower((unsigned char)*q);
    seg[n] = '\0';
    const char *prefixes[] = {"gpt-4o", "gpt-4.1", "gpt-5", "o1", "o3", "o4"};
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++)
        if (strncmp(seg, prefixes[i], strlen(prefixes[i])) == 0) return 1;
    return 0;
}

/* ---------- route_profiles ---------- */
static void route_names(const char *hook, const char *provider, const char *base_url,
                        const json_t *profiles, const json_t *providers, const json_t *hosts,
                        json_t *order) {
    const json_t *named = lookup_profile(profiles, providers, provider);
    if (named && profile_overrides(named, hook)) json_append(order, json_string(profile_name(named)));
    const json_t *hosted = lookup_profile(profiles, hosts, base_url);
    if (hosted && strcmp(profile_name(hosted), profile_name(named)) != 0 &&
        profile_overrides(hosted, hook))
        json_append(order, json_string(profile_name(hosted)));
}

struct json_t *w13_providerro_daa14d_route(const char *hook, const char *provider,
                                           const char *base_url, const json_t *profiles,
                                           const json_t *providers, const json_t *hosts) {
    json_t *order = json_array();
    route_names(hook, provider ? provider : "", base_url ? base_url : "",
                profiles, providers, hosts, order);
    return order;
}

/* ---------- sticky failures ---------- */
struct fail { char name[NAME_MAX]; char hook[NAME_MAX]; };

static int fail_has(const struct fail *f, size_t n, const char *name, const char *hook) {
    for (size_t i = 0; i < n; i++)
        if (!strcmp(f[i].name, name) && !strcmp(f[i].hook, hook)) return 1;
    return 0;
}

static struct fail *fail_load(const json_t *scope, size_t *n) {
    struct fail *f = NULL;
    size_t cap = 0;
    *n = 0;
    const json_t *arr = json_is_object(scope) ? json_obj_get(scope, "_provider_hook_failures") : NULL;
    if (json_is_array(arr)) {
        for (size_t i = 0; i < json_len(arr); i++) {
            json_t *e = json_get(arr, i);
            if (!json_is_array(e) || json_len(e) < 2) continue;
            if (*n == cap) { cap = cap ? cap * 2 : 8; f = (struct fail *)realloc(f, cap * sizeof(*f)); }
            snprintf(f[*n].name, NAME_MAX, "%s", jstr(json_get(e, 0)));
            snprintf(f[*n].hook, NAME_MAX, "%s", jstr(json_get(e, 1)));
            (*n)++;
        }
    }
    return f;
}

static json_t *fail_dump(struct fail *f, size_t n) {
    json_t *out = json_array();
    /* sorted by (name, hook) strings */
    for (size_t i = 1; i < n; i++) {
        struct fail cur = f[i];
        size_t j = i;
        while (j > 0) {
            int c = strcmp(cur.name, f[j-1].name);
            if (c == 0) c = strcmp(cur.hook, f[j-1].hook);
            if (c >= 0) break;
            f[j] = f[j-1]; j--;
        }
        f[j] = cur;
    }
    for (size_t i = 0; i < n; i++) {
        json_t *pair = json_array();
        json_append(pair, json_string(f[i].name));
        json_append(pair, json_string(f[i].hook));
        json_append(out, pair);
    }
    return out;
}

/* core of ask_profile: `answers`/`raises` belong to the profile. */
static json_t *ask_core(const char *pname, const char *hook, const json_t *fallback,
                        struct fail **pf, size_t *pn, int ephemeral,
                        const json_t *answers, const json_t *raises, json_t **ret) {
    if (fail_has(*pf, *pn, pname, hook)) {
        *ret = fallback ? json_copy(fallback) : json_null();
        return NULL;
    }
    if (arr_has_str(raises, hook)) {
        if (!ephemeral) {
            size_t n = *pn;
            *pf = (struct fail *)realloc(*pf, (n + 1) * sizeof(struct fail));
            snprintf((*pf)[n].name, NAME_MAX, "%s", pname);
            snprintf((*pf)[n].hook, NAME_MAX, "%s", hook);
            *pn = n + 1;
        }
        *ret = fallback ? json_copy(fallback) : json_null();
        return NULL;
    }
    json_t *a = json_obj_get(answers, hook);
    *ret = json_copy(a ? a : json_null());
    return NULL;
}

struct json_t *w13_providerro_daa14d_ask(const json_t *profile, const char *hook,
                                         const json_t *fallback, const json_t *scope,
                                         const json_t *answers, const json_t *raises) {
    const char *pname = profile_name(profile);
    size_t n = 0;
    struct fail *f = fail_load(scope, &n);
    int ephemeral = (scope == NULL || scope->type == JSON_NULL);
    json_t *ret = NULL;
    ask_core(pname, hook ? hook : "", fallback, &f, &n, ephemeral, answers, raises, &ret);
    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "failures", fail_dump(f, n));
    free(f);
    return out;
}

int w13_providerro_daa14d_hook_failed(const json_t *profile, const char *hook,
                                      const json_t *scope) {
    size_t n = 0;
    struct fail *f = fail_load(scope, &n);
    int r = fail_has(f, n, profile_name(profile), hook ? hook : "");
    free(f);
    return r;
}

/* ---------- model_api_mode ---------- */
struct json_t *w13_providerro_daa14d_model_api_mode(const json_t *profile, const char *model) {
    (void)model;
    if (!profile || profile->type != JSON_OBJECT) return json_string("chat_completions");
    int raising = json_get_bool(profile, "model_mode_raises", false) ? 1 : 0;
    if (!raising) {
        json_t *a = json_obj_get(profile, "model_mode_answer");
        return a ? json_copy(a) : json_null();
    }
    json_t *am = json_obj_get(profile, "api_mode");
    return (am && am->type == JSON_STRING) ? json_string(am->str_val)
                                           : json_string("chat_completions");
}

/* ---------- route_answer ---------- */
struct json_t *w13_providerro_daa14d_route_answer(const char *hook, const char *provider,
                                                  const char *base_url, const json_t *profiles,
                                                  const json_t *providers, const json_t *hosts,
                                                  const json_t *scope) {
    json_t *order = json_array();
    route_names(hook, provider ? provider : "", base_url ? base_url : "",
                profiles, providers, hosts, order);
    size_t n = 0;
    struct fail *f = fail_load(scope, &n);
    int ephemeral = (scope == NULL || scope->type == JSON_NULL);
    json_t *answer = NULL;
    for (size_t i = 0; i < json_len(order); i++) {
        const char *pname = jstr(json_get(order, i));
        const json_t *prof = find_profile(profiles, pname);
        json_t *ret = NULL;
        ask_core(pname, hook ? hook : "", json_null(), &f, &n, ephemeral,
                 prof ? json_obj_get(prof, "answers") : NULL,
                 prof ? json_obj_get(prof, "raises") : NULL, &ret);
        if (jtruthy(ret)) { answer = ret; ret = NULL; break; }
        json_free(ret);
    }
    json_free(order);
    json_t *out = json_object();
    json_set(out, "ret", answer ? answer : json_null());
    json_set(out, "failures", fail_dump(f, n));
    free(f);
    return out;
}
