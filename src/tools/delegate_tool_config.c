/*
 * delegate_tool_config.c - C home for upstream Hermes tools/delegate_tool_config.py.
 * PoP: w14_delegateto_5b49f0_parse_timeout @ tools/delegate_tool_config.py:_parse_timeout
 * PoP: w14_delegateto_5b49f0_normalized_runtime_url @ tools/delegate_tool_config.py:_normalized_runtime_url
 * PoP: w14_delegateto_5b49f0_merge_request_overrides @ tools/delegate_tool_config.py:_merge_request_overrides
 * PoP: w14_delegateto_5b49f0_credential_bundle @ tools/delegate_tool_config.py:_credential_bundle
 * PoP: w14_delegateto_5b49f0_inherit_parent_capabilities @ tools/delegate_tool_config.py:_inherit_parent_capabilities
 * PoP: w14_delegateto_5b49f0_gateway_delegation_inline @ tools/delegate_tool_config.py:gateway_delegation_inline
 *
 * Pure delegation knobs/bundle helpers. Config loading, logging and provider
 * resolution are injected as JSON inputs, so nothing here touches the process
 * environment or the filesystem.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_delegateto_5b49f0.h"

static char *dupstr(const char *s) { size_t n = strlen(s) + 1; char *p = (char *)malloc(n); if (p) memcpy(p, s, n); return p; }
static char *fmti(long long v) { char b[48]; snprintf(b, sizeof(b), "%lld", v); return dupstr(b); }
static char *fmtg(double v) { char b[64]; snprintf(b, sizeof(b), "%g", v); return dupstr(b); }

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return 0;
}

/* Python str() for the scalars used by these helpers. Integral doubles render
 * without '.0' so JSON int inputs stringify like Python ints. */
static char *py_str(const json_t *v) {
    if (!truthy(v)) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) return fmti((long long)v->num_val);
        return fmtg(v->num_val);
    }
    char *s = json_dumps(v, 0); return s ? s : dupstr("");
}

static int is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
/* .strip().lower() in place on a malloc'd buffer (no internal collapse) */
static void lower_strip(char *s) {
    char *a = s, *b = s + strlen(s);
    while (a < b && is_ws((unsigned char)a[0])) a++;
    while (b > a && is_ws((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    memmove(s, a, n);
    s[n] = 0;
    for (size_t i = 0; i < n; i++) s[i] = (char)tolower((unsigned char)s[i]);
}

struct json_t *w14_delegateto_5b49f0_parse_timeout(const struct json_t *raw) {
    double d;
    if (!raw || raw->type == JSON_NULL) return json_null();
    if (raw->type == JSON_NUMBER) d = raw->num_val;
    else if (raw->type == JSON_STRING) {
        char *end = NULL;
        d = strtod(raw->str_val ? raw->str_val : "", &end);
        if (end == raw->str_val) return json_null();
    } else if (raw->type == JSON_BOOL) d = raw->bool_val ? 1.0 : 0.0;
    else return json_null();
    if (!(d > 0.0)) return json_null();
    if (d < 30.0) d = 30.0;
    return json_number(d);
}

struct json_t *w14_delegateto_5b49f0_normalized_runtime_url(const struct json_t *value) {
    char *s = py_str(value);
    char *a = s, *b = s + strlen(s);
    while (a < b && is_ws((unsigned char)a[0])) a++;
    while (b > a && is_ws((unsigned char)b[-1])) b--;
    while (b > a && b[-1] == '/') b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    memcpy(out, a, n); out[n] = 0;
    json_t *r = json_string(out);
    free(s); free(out);
    return r;
}

struct json_t *w14_delegateto_5b49f0_merge_request_overrides(const struct json_t *runtime_overrides,
                                                             const struct json_t *explicit_overrides) {
    int rok = runtime_overrides && runtime_overrides->type == JSON_OBJECT;
    int eok = explicit_overrides && explicit_overrides->type == JSON_OBJECT;
    int rempty = !rok || json_object_size(runtime_overrides) == 0;
    int eempty = !eok || json_object_size(explicit_overrides) == 0;
    if (rempty && eempty) return json_null();
    json_t *merged = rok ? json_copy(runtime_overrides) : json_object();
    json_t *exp = eok ? json_copy(explicit_overrides) : NULL;
    json_t *runtime_extra = json_obj_get(merged, "extra_body");
    json_t *explicit_extra = NULL;
    if (exp) {
        for (size_t i = 0; i < json_object_size(exp); i++) {
            const char *k = json_object_get_key_at(exp, i);
            if (k && !strcmp(k, "extra_body")) {
                explicit_extra = json_copy(json_object_get_at(exp, i));
                json_obj_del(exp, k);
                break;
            }
        }
        for (size_t i = 0; i < json_object_size(exp); i++) {
            const char *k = json_object_get_key_at(exp, i);
            if (k) json_set(merged, k, json_copy(json_object_get_at(exp, i)));
        }
    }
    int re_dict = runtime_extra && runtime_extra->type == JSON_OBJECT;
    int ee_dict = explicit_extra && explicit_extra->type == JSON_OBJECT;
    if (re_dict && ee_dict) {
        for (size_t i = 0; i < json_object_size(explicit_extra); i++) {
            const char *k = json_object_get_key_at(explicit_extra, i);
            if (k) json_set(runtime_extra, k, json_copy(json_object_get_at(explicit_extra, i)));
        }
        /* runtime_extra is already the node stored at "extra_body" in merged;
         * re-setting it would free the very node we just edited (json_set frees
         * the replaced value), so leave the binding in place. */
        json_free(explicit_extra);
    } else if (explicit_extra) {
        json_set(merged, "extra_body", explicit_extra);
    }
    json_free(exp);
    if (json_object_size(merged) == 0) { json_free(merged); return json_null(); }
    return merged;
}

struct json_t *w14_delegateto_5b49f0_credential_bundle(const struct json_t *model, const struct json_t *provider,
                                                       const struct json_t *base_url, const struct json_t *api_key,
                                                       const struct json_t *api_mode, const struct json_t *request_overrides,
                                                       const struct json_t *extra) {
    json_t *r = json_object();
    const char *keys[6] = {"model", "provider", "base_url", "api_key", "api_mode", "request_overrides"};
    const json_t *vals[6] = {model, provider, base_url, api_key, api_mode, request_overrides};
    for (int i = 0; i < 6; i++) json_set(r, keys[i], vals[i] ? json_copy(vals[i]) : json_null());
    if (extra && extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(extra); i++) {
            const char *k = json_object_get_key_at(extra, i);
            if (k) json_set(r, k, json_copy(json_object_get_at(extra, i)));
        }
    }
    return r;
}

struct json_t *w14_delegateto_5b49f0_inherit_parent_capabilities(const struct json_t *parent_agent,
                                                                 const struct json_t *override_provider,
                                                                 const struct json_t *override_base_url) {
    if (truthy(override_provider) || truthy(override_base_url)) return json_null();
    json_t *caps = json_obj_get(parent_agent, "capabilities");
    if (!caps || caps->type != JSON_OBJECT) return json_null();
    json_t *r = json_object();
    for (size_t i = 0; i < json_object_size(caps); i++) {
        const char *k = json_object_get_key_at(caps, i);
        json_t *v = json_object_get_at(caps, i);
        if (k && v && v->type == JSON_BOOL) json_set(r, k, json_copy(v));
    }
    return r;
}

static int non_messaging(const char *s) {
    static const char *set[] = {"", "api_server", "cli", "codex", "desktop", "gateway", "kanban",
                                "local", "msgraph_webhook", "tool", "tui", "webhook", NULL};
    for (int i = 0; set[i]; i++) if (!strcmp(s, set[i])) return 1;
    return 0;
}

int w14_delegateto_5b49f0_gateway_delegation_inline(const struct json_t *user_config, const char *platform_key,
                                                     int interim_enabled) {
    json_t *section = (user_config && user_config->type == JSON_OBJECT)
        ? json_obj_get(user_config, "delegation") : NULL;
    char *mode = dupstr("auto");
    if (section && section->type == JSON_OBJECT) {
        json_t *m = json_obj_get(section, "gateway_mode");
        if (truthy(m)) { free(mode); mode = py_str(m); lower_strip(mode); }
    }
    if (!strcmp(mode, "inline")) { free(mode); return 1; }
    if (!strcmp(mode, "background") || !strcmp(mode, "async")) { free(mode); return 0; }
    free(mode);
    char pk[512];
    snprintf(pk, sizeof(pk), "%s", platform_key ? platform_key : "");
    lower_strip(pk);
    return !interim_enabled && !non_messaging(pk);
}

