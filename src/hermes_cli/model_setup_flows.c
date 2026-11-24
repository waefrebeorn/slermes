/*
 * model_setup_flows.c - C home for upstream Hermes hermes_cli/model_setup_flows.py.
 * PoP: w16_modelsetup_5e0304_nous_login_args @ hermes_cli/model_setup_flows.py:_nous_login_args
 * PoP: w16_modelsetup_5e0304_select_zai_endpoint @ hermes_cli/model_setup_flows.py:_select_zai_endpoint
 *
 * Faithful ports of two pure helpers: the Nous login-argument normaliser and
 * the Z.AI endpoint picker's pure selection logic. Interactive prompting and
 * I/O are injected by the caller (fixture) rather than performed here.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_modelsetup_5e0304.h"

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default:          return 0;
    }
}

/* Trim ASCII whitespace from both ends; returns the ORIGINAL allocation start. */
static char *strip_inplace(char *s) {
    if (!s) return s;
    size_t n = strlen(s), lead = 0;
    while (lead < n && isspace((unsigned char)s[lead])) lead++;
    if (lead) memmove(s, s + lead, n - lead + 1);
    n -= lead;
    while (n && isspace((unsigned char)s[n - 1])) n--;
    s[n] = '\0';
    return s;
}

/* rstrip only '/' characters; returns the ORIGINAL allocation start. */
static char *rstrip_slash(char *s) {
    if (!s) return s;
    size_t n = strlen(s);
    while (n && s[n - 1] == '/') n--;
    s[n] = '\0';
    return s;
}

struct json_t *w16_modelsetup_5e0304_nous_login_args(const struct json_t *args) {
    json_t *res = json_object();
    const char *str_keys[] = {"portal_url", "inference_url", "client_id", "scope", "ca_bundle"};
    for (size_t i = 0; i < sizeof(str_keys) / sizeof(str_keys[0]); i++) {
        json_t *v = json_obj_get(args, str_keys[i]);
        json_set(res, str_keys[i], v && v->type != JSON_NULL ? json_copy(v) : json_null());
    }
    json_set(res, "no_browser", json_bool(truthy(json_obj_get(args, "no_browser"))));
    json_set(res, "insecure", json_bool(truthy(json_obj_get(args, "insecure"))));
    json_t *timeout = json_obj_get(args, "timeout");
    if (timeout && timeout->type != JSON_NULL && truthy(timeout))
        json_set(res, "timeout", json_copy(timeout));
    else
        json_set(res, "timeout", json_number(15.0));
    return res;
}

/* (label, url) pairs from hermes_cli.auth.ZAI_ENDPOINTS, in declared order. */
static const char *const ZAI_LABELS[4] = {
    "Global", "China", "Global (Coding Plan)", "China (Coding Plan)"
};
static const char *const ZAI_URLS[4] = {
    "https://api.z.ai/api/paas/v4",
    "https://open.bigmodel.cn/api/paas/v4",
    "https://api.z.ai/api/coding/paas/v4",
    "https://open.bigmodel.cn/api/coding/paas/v4"
};
#define ZAI_COUNT 4

struct json_t *w16_modelsetup_5e0304_select_zai_endpoint(const char *current_base,
                                                         const struct json_t *selected,
                                                         const char *override) {
    char *norm = strip_inplace(dupstr(current_base ? current_base : ""));
    rstrip_slash(norm);
    int default_idx = 0;
    int matched = 0;
    for (int i = 0; i < ZAI_COUNT; i++) {
        char *u = rstrip_slash(dupstr(ZAI_URLS[i]));
        if (strcmp(norm, u) == 0) { default_idx = i; matched = 1; }
        free(u);
        if (matched) break;
    }
    if (!matched && norm[0]) default_idx = ZAI_COUNT;
    free(norm);
    (void)default_idx;  /* the picker's default is caller-injected via `selected` */

    int sel;
    if (!selected || selected->type == JSON_NULL) {
        return json_string(current_base ? current_base : "");
    }
    sel = (int)selected->num_val;

    if (sel != ZAI_COUNT) {
        if (sel < 0 || sel >= ZAI_COUNT)
            return json_string(current_base ? current_base : "");
        char *u = rstrip_slash(dupstr(ZAI_URLS[sel]));
        json_t *res = json_string(u);
        free(u);
        return res;
    }
    const char *ov = override ? override : "";
    if (!ov[0]) return json_string(current_base ? current_base : "");
    if (strncmp(ov, "http://", 7) != 0 && strncmp(ov, "https://", 8) != 0)
        return json_string(current_base ? current_base : "");
    char *o = rstrip_slash(dupstr(ov));
    json_t *res = json_string(o);
    free(o);
    return res;
}
