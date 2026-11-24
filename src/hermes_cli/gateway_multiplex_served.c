/*
 * gateway_multiplex_served.c — C home for upstream Hermes hermes_cli/gateway_multiplex_served.py.
 * Gate w26_gatewaymul_37308a.
 *
 * PoP: w26_gatewaymul_37308a_served_profiles @ hermes_cli/gateway_multiplex_served.py:recorded_served_profiles
 * PoP: w26_gatewaymul_37308a_secondaries @ hermes_cli/gateway_multiplex_served.py:multiplexer_served_secondaries
 * PoP: w26_gatewaymul_37308a_unserved_platforms @ hermes_cli/gateway_multiplex_served.py:served_profile_unserved_platforms
 * PoP: w26_gatewaymul_37308a_ingress_urls @ hermes_cli/gateway_multiplex_served.py:served_profile_ingress_urls
 * PoP: w26_gatewaymul_37308a_format_lines @ hermes_cli/gateway_multiplex_served.py:format_ingress_url_lines
 *
 * The live-PID proof (pid file + lock + process table) and the
 * ``gateway_state.json`` reader are pure I/O seams; the caller supplies both as
 * values (``live`` / ``runtime``). Everything here is then a pure function of
 * the record: one answer for every CLI and dashboard surface about what the
 * running multiplexer really serves.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_gatewaymul_37308a.h"

/* gateway.config.SERVED_PROFILE_MIRROR_PLATFORMS (sorted for iteration). */
static const char *const GM_MIRROR_PLATFORMS[W26_GATEWAYMUL_37308A_MIRROR_PLATFORMS] = {
    "api_server", "relay", "webhook",
};

/* gateway.config.SHARED_LISTENER_MIRROR_PLATFORMS: the two that bind a port. */
static const char *const GM_SHARED_LISTENERS[W26_GATEWAYMUL_37308A_SHARED_LISTENERS] = {
    "api_server", "webhook",
};

/* Python truthiness of a JSON value. */
static int gm_truthy(const json_t *v) {
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

static int gm_is_object(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int gm_is_array(const json_t *v) { return v && v->type == JSON_ARRAY; }

/* Python str() over the element shapes a runtime record holds: strings keep
 * their text, booleans become True/False, integral numbers drop the ".0". */
static char *gm_str_dup(const json_t *v) {
    char buf[64];
    if (!v) return strdup("None");
    switch (v->type) {
    case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return strdup(buf);
    case JSON_NULL:   return strdup("None");
    case JSON_ARRAY:
    case JSON_OBJECT: return strdup("");
    }
    return strdup("");
}

static int gm_str_eq(const json_t *v, const char *s) {
    return v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, s) == 0;
}

/* isinstance(served, list) -> [str(p) for p in served], else None. */
json_t *w26_gatewaymul_37308a_served_profiles(const json_t *runtime, int live) {
    const json_t *served;
    json_t *out;
    size_t i;
    if (!live) return json_null();
    if (!gm_is_object(runtime)) return json_null();
    served = json_obj_get(runtime, "served_profiles");
    if (!gm_is_array(served)) return json_null();
    out = json_array();
    for (i = 0; i < json_len(served); i++) {
        char *s = gm_str_dup(json_get(served, i));
        json_append(out, json_string(s ? s : ""));
        free(s);
    }
    return out;
}

/* [p for p in served if p and p != "default"] */
json_t *w26_gatewaymul_37308a_secondaries(const json_t *runtime, int live) {
    json_t *served = w26_gatewaymul_37308a_served_profiles(runtime, live);
    json_t *out = json_array();
    size_t i;
    if (!gm_is_array(served)) { json_free(served); return out; }
    for (i = 0; i < json_len(served); i++) {
        const json_t *p = json_get(served, i);
        if (!gm_truthy(p)) continue;
        if (gm_str_eq(p, "default")) continue;
        json_append(out, json_copy(p));
    }
    json_free(served);
    return out;
}

/* {key[len(prefix):]: str(entry["error_message"] or "not served under multiplex")} */
json_t *w26_gatewaymul_37308a_unserved_platforms(const json_t *runtime, int live,
                                                const char *profile) {
    const json_t *platforms;
    json_t *out = json_object();
    char prefix[512];
    size_t plen, i;
    if (!profile || !profile[0] || !live) return out;
    if (!gm_is_object(runtime)) return out;
    platforms = json_obj_get(runtime, "platforms");
    if (!gm_is_object(platforms)) return out;
    snprintf(prefix, sizeof(prefix), "%s:", profile);
    plen = strlen(prefix);
    for (i = 0; i < json_object_size(platforms); i++) {
        const char *key = json_object_get_key_at(platforms, i);
        const json_t *entry = json_object_get_at(platforms, i);
        const json_t *code, *msg;
        char *name, *text;
        if (!key || strncmp(key, prefix, plen) != 0) continue;
        if (!gm_is_object(entry)) continue;
        code = json_obj_get(entry, "error_code");
        if (!gm_str_eq(code, "multiplex_shared_ingress") && !gm_str_eq(code, "whatsapp_unpaired"))
            continue;
        msg = json_obj_get(entry, "error_message");
        text = gm_truthy(msg) ? gm_str_dup(msg) : strdup("not served under multiplex");
        name = strdup(key + plen);
        if (name && text) json_set(out, name, json_string(text));
        free(name);
        free(text);
    }
    return out;
}

/* shared_listener_mirror_platforms(runtime, profile) -> gateway.status_served */
static json_t *gm_mirror_platforms(const json_t *runtime, const char *profile) {
    const json_t *platforms;
    json_t *mirrored = json_object();
    size_t i;
    if (!profile || !profile[0] || strcmp(profile, "default") == 0) return mirrored;
    if (!gm_is_object(runtime)) return mirrored;
    platforms = json_obj_get(runtime, "platforms");
    if (!gm_is_object(platforms)) return mirrored;
    for (i = 0; i < W26_GATEWAYMUL_37308A_MIRROR_PLATFORMS; i++) {
        const char *name = GM_MIRROR_PLATFORMS[i];
        const json_t *entry = json_obj_get(platforms, name);
        const json_t *state, *base;
        json_t *copy;
        int j, shared = 0;
        char url[2048];
        const char *suffix = "";
        if (!gm_is_object(entry)) continue;
        for (j = 0; j < W26_GATEWAYMUL_37308A_SHARED_LISTENERS; j++)
            if (strcmp(GM_SHARED_LISTENERS[j], name) == 0) shared = 1;
        if (shared) {
            state = json_obj_get(entry, "state");
            if (!gm_str_eq(state, "connected") && !gm_str_eq(state, "connecting")
                && !gm_str_eq(state, "retrying"))
                continue;
        }
        if (strcmp(name, "api_server") == 0) suffix = "/v1";
        else if (strcmp(name, "webhook") == 0) suffix = "/webhooks/<route>";
        base = json_obj_get(entry, "listener_base");
        copy = json_copy(entry);
        json_obj_del(copy, "listener_base");
        if (base && base->type == JSON_STRING && base->str_val && base->str_val[0]) {
            snprintf(url, sizeof(url), "%s/p/%s%s", base->str_val, profile, suffix);
            json_set(copy, "ingress_url", json_string(url));
        } else {
            json_set(copy, "ingress_url", json_null());
        }
        json_set(copy, "mirrored_from", json_string("default"));
        json_set(mirrored, name, copy);
    }
    return mirrored;
}

/* entry.get("ingress_url") for a mirror entry, as a string copy or NULL. */
static char *gm_entry_url(const json_t *entry) {
    const json_t *url;
    if (!gm_is_object(entry)) return NULL;
    url = json_obj_get(entry, "ingress_url");
    if (!gm_truthy(url)) return NULL;
    return gm_str_dup(url);
}

json_t *w26_gatewaymul_37308a_ingress_urls(const json_t *runtime, int live,
                                          const char *profile) {
    const json_t *platforms, *served_raw;
    json_t *urls = json_object();
    size_t i, j;
    if (!live) return urls;
    if (!gm_is_object(runtime)) return urls;
    platforms = json_obj_get(runtime, "platforms");
    if (!gm_is_object(platforms)) return urls;

    for (i = 0; i < json_object_size(platforms); i++) {
        const char *key = json_object_get_key_at(platforms, i);
        const json_t *entry = json_object_get_at(platforms, i);
        const json_t *url, *state;
        const char *colon;
        char *name, *platform, *text;
        json_t *bucket;
        if (!key || !strchr(key, ':') || !gm_is_object(entry)) continue;
        url = json_obj_get(entry, "ingress_url");
        if (!gm_truthy(url)) continue;
        state = json_obj_get(entry, "state");
        if (gm_str_eq(state, "fatal") || gm_str_eq(state, "disconnected")
            || gm_str_eq(state, "stopped"))
            continue;
        colon = strchr(key, ':');
        name = (char *)malloc((size_t)(colon - key) + 1);
        if (!name) continue;
        memcpy(name, key, (size_t)(colon - key));
        name[colon - key] = '\0';
        platform = strdup(colon + 1);
        if (profile && profile[0] && strcmp(name, profile) != 0) {
            free(name);
            free(platform);
            continue;
        }
        bucket = json_obj_get(urls, name);
        if (!gm_is_object(bucket)) {
            bucket = json_object();
            json_set(urls, name, bucket);
        }
        text = gm_str_dup(url);
        if (platform && text) json_set(bucket, platform, json_string(text));
        free(text);
        free(platform);
        free(name);
    }

    /* api_server/webhook are the default's adapters mirrored at /p/<profile>/. */
    served_raw = json_obj_get(runtime, "served_profiles");
    for (i = 0; gm_is_array(served_raw) && i < json_len(served_raw); i++) {
        const json_t *p = json_get(served_raw, i);
        char *name;
        json_t *mirrors;
        if (!gm_truthy(p) || gm_str_eq(p, "default")) continue;
        name = gm_str_dup(p);
        if (!name) continue;
        if (profile && profile[0] && strcmp(name, profile) != 0) { free(name); continue; }
        mirrors = gm_mirror_platforms(runtime, name);
        for (j = 0; j < json_object_size(mirrors); j++) {
            const char *platform = json_object_get_key_at(mirrors, j);
            const json_t *entry = json_object_get_at(mirrors, j);
            char *url = gm_entry_url(entry);
            json_t *bucket;
            if (!url) continue;
            bucket = json_obj_get(urls, name);
            if (!gm_is_object(bucket)) {
                bucket = json_object();
                json_set(urls, name, bucket);
            }
            json_set(bucket, platform, json_string(url));
            free(url);
        }
        json_free(mirrors);
        free(name);
    }
    return urls;
}

/* One "<indent><platform>: <url>" line per platform, sorted. */
json_t *w26_gatewaymul_37308a_format_lines(const json_t *urls, const char *indent) {
    json_t *out = json_array();
    size_t n, i, k;
    const char **keys;
    if (!indent) indent = "  ";
    if (!gm_is_object(urls)) return out;
    n = json_object_size(urls);
    if (n == 0) return out;
    keys = (const char **)calloc(n, sizeof(const char *));
    if (!keys) return out;
    for (i = 0; i < n; i++) keys[i] = json_object_get_key_at(urls, i);
    /* insertion sort: Python sorted() is stable and these tables are tiny. */
    for (i = 1; i < n; i++) {
        const char *cur = keys[i];
        k = i;
        while (k > 0 && strcmp(keys[k - 1], cur) > 0) { keys[k] = keys[k - 1]; k--; }
        keys[k] = cur;
    }
    for (i = 0; i < n; i++) {
        const json_t *v = json_obj_get(urls, keys[i]);
        char *url = gm_str_dup(v);
        size_t need = strlen(indent) + strlen(keys[i]) + 2 + (url ? strlen(url) : 0) + 1;
        char *line = (char *)malloc(need);
        if (line) {
            snprintf(line, need, "%s%s: %s", indent, keys[i], url ? url : "None");
            json_append(out, json_string(line));
            free(line);
        }
        free(url);
    }
    free(keys);
    return out;
}

json_t *w26_gatewaymul_37308a_constants(void) {
    json_t *o = json_object();
    json_t *mirror = json_array(), *shared = json_array(), *paths = json_object();
    size_t i;
    for (i = 0; i < W26_GATEWAYMUL_37308A_MIRROR_PLATFORMS; i++)
        json_append(mirror, json_string(GM_MIRROR_PLATFORMS[i]));
    for (i = 0; i < W26_GATEWAYMUL_37308A_SHARED_LISTENERS; i++)
        json_append(shared, json_string(GM_SHARED_LISTENERS[i]));
    json_set(paths, "api_server", json_string("/v1"));
    json_set(paths, "webhook", json_string("/webhooks/<route>"));
    json_set(o, "served_profile_mirror_platforms", mirror);
    json_set(o, "shared_listener_mirror_platforms", shared);
    json_set(o, "listener_mirror_paths", paths);
    return o;
}
