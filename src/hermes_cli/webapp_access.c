/*
 * webapp_access.c — faithful C11 port of hermes_cli/webapp_access.py.
 * Gate w11_webappacce_9bd4bc.
 *
 * PoP: w11_webappacce_9bd4bc_sign_in_client @ hermes_cli/webapp_access.py:sign_in_client
 * PoP: w11_webappacce_9bd4bc_mode @ hermes_cli/webapp_access.py:_mode
 * PoP: w11_webappacce_9bd4bc_serves_config @ hermes_cli/webapp_access.py:serves_config
 * PoP: w11_webappacce_9bd4bc_write_config @ hermes_cli/webapp_access.py:_write_config
 */
#include "hermes_json.h"
#include "noc_w11_webappacce_9bd4bc.h"
#include <stdlib.h>
#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

json_t *w11_webappacce_9bd4bc_sign_in_client(const json_t *record) {
    json_t *own = json_obj_get(record, "own_client");
    if (!own || own->type != JSON_BOOL || !own->bool_val) return NULL; /* is not True */
    json_t *portal = json_obj_get(record, "portal");
    if (!portal || portal->type != JSON_STRING || !portal->str_val || !portal->str_val[0]) return NULL;
    json_t *cid = json_obj_get(record, "client_id");
    if (!cid || cid->type != JSON_STRING || !cid->str_val || strncmp(cid->str_val, "agent:", 6)) return NULL;
    json_t *out = json_array();
    json_append(out, json_string(cid->str_val));
    json_append(out, json_string(portal->str_val));
    return out;
}

const char *w11_webappacce_9bd4bc_mode(const struct json_t *settings, int wildcard) {
    if (!py_truthy(json_obj_get(settings, "enabled"))) return NULL;
    if (py_truthy(json_obj_get(settings, "public_url"))) return "public";
    return wildcard ? "lan" : "local";
}

/* urlsplit(url).port: 1 + out when a valid in-range port is present, else 0
 * (no port, or ValueError which serves_config catches as False). */
static int url_port(const char *url, int *out) {
    if (!url) return 0;
    const char *p = strstr(url, "://");
    const char *netloc = p ? p + 3 : url;
    const char *end = netloc;
    while (*end && *end != '/' && *end != '?' && *end != '#') end++;
    if (end == netloc) return 0;
    const char *at = NULL;
    for (const char *q = netloc; q < end; q++) if (*q == '@') at = q;
    const char *host = at ? at + 1 : netloc;
    const char *colon = NULL;
    if (*host == '[') {
        const char *rb = host;
        while (rb < end && *rb != ']') rb++;
        if (rb + 1 < end && *(rb + 1) == ':') colon = rb + 1;
    } else {
        for (const char *q = host; q < end; q++) if (*q == ':') colon = q;
    }
    if (!colon || colon + 1 >= end) return 0;
    const char *ds = colon + 1;
    for (const char *q = ds; q < end; q++) if (*q < '0' || *q > '9') return 0;
    long v = strtol(ds, NULL, 10);
    if (v > 65535) return 0;
    *out = (int)v;
    return 1;
}

int w11_webappacce_9bd4bc_serves_config(const struct json_t *status) {
    if (!py_truthy(json_obj_get(status, "enabled"))) return 0;
    json_t *gw = json_obj_get(status, "gateway");
    if (!gw || gw->type != JSON_OBJECT) return 0;
    json_t *state = json_obj_get(gw, "state");
    if (!state || state->type != JSON_STRING || !state->str_val || strcmp(state->str_val, "serving")) return 0;
    if (!py_truthy(json_obj_get(gw, "url"))) return 0;
    json_t *sp = json_obj_get(status, "port");
    if (!sp || sp->type != JSON_NUMBER) return 0;
    int p;
    if (!url_port(json_get_str(gw, "url", ""), &p)) return 0;
    return p == (int)sp->num_val;
}

struct json_t *w11_webappacce_9bd4bc_write_config(const struct json_t *cfg, int enabled,
                                                  const char *host, const char *public_url,
                                                  const struct json_t *port) {
    json_t *newcfg = (cfg && cfg->type == JSON_OBJECT) ? json_copy(cfg) : json_object();
    json_t *wa = json_obj_get(newcfg, "webapp");
    json_t *webapp = (wa && wa->type == JSON_OBJECT) ? json_copy(wa) : json_object();
    json_set(webapp, "enabled", json_bool(enabled ? true : false));
    if (enabled) json_set(webapp, "host", json_string(host ? host : ""));
    if (py_truthy(port)) json_set(webapp, "port", json_copy(port));
    if (enabled && public_url && public_url[0]) {
        json_set(webapp, "public_url", json_string(public_url));
    } else if (enabled) {
        json_obj_del(webapp, "public_url");
    }
    json_set(newcfg, "webapp", webapp);
    return newcfg;
}
