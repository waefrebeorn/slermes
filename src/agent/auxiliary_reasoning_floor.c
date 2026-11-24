/*
 * auxiliary_reasoning_floor.c — C11 port of upstream Hermes
 * agent/auxiliary_reasoning_floor.py (gate w26_auxiliaryr_2d56c8).
 *
 * PoP: w26_auxiliaryr_2d56c8_route_key @ agent/auxiliary_reasoning_floor.py:_route_key
 * PoP: w26_auxiliaryr_2d56c8_is_disabled @ agent/auxiliary_reasoning_floor.py:_is_disabled
 * PoP: w26_auxiliaryr_2d56c8_floor_config @ agent/auxiliary_reasoning_floor.py:floor_reasoning_config
 * PoP: w26_auxiliaryr_2d56c8_with_floor @ agent/auxiliary_reasoning_floor.py:with_reasoning_floor
 * PoP: w26_auxiliaryr_2d56c8_known_floor @ agent/auxiliary_reasoning_floor.py:known_reasoning_floor
 * PoP: w26_auxiliaryr_2d56c8_constants @ agent/auxiliary_reasoning_floor.py:<module constants>
 *
 * The process memo (_FLOORED_ROUTES), the /v1/models catalog and the logger are
 * seams: the caller passes the memo/catalog answers in as values, and this file
 * owns only what those values decide — the route key, the disable test, the
 * lift-to-floor rewrite and the effort vocabulary.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_auxiliaryr_2d56c8.h"

/* _DISABLED_EFFORTS, sorted: Python compares through a set, the order is ours. */
static const char *const AUX_DISABLED_EFFORTS[] = {"0", "disabled", "false", "none", "off"};
#define AUX_DISABLED_EFFORT_COUNT (sizeof(AUX_DISABLED_EFFORTS) / sizeof(AUX_DISABLED_EFFORTS[0]))

static char *aux_strdup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Python ``str(x).strip().lower()`` for the shapes a flag can hold. */
static void aux_str_lower_strip(const json_t *v, char *buf, size_t cap) {
    size_t n = 0, start = 0, end;
    if (!v || v->type == JSON_NULL) {
        snprintf(buf, cap, "none");
        return;
    }
    if (v->type == JSON_BOOL) {
        snprintf(buf, cap, "%s", v->bool_val ? "true" : "false");
        return;
    }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1e16 && d < 1e16) snprintf(buf, cap, "%.0f", d);
        else snprintf(buf, cap, "%.17g", d);
        return;
    }
    if (v->type != JSON_STRING) { /* containers are never a disabled effort */
        snprintf(buf, cap, "%s", v->type == JSON_ARRAY ? "[]" : "{}");
        return;
    }
    while (v->str_val && v->str_val[n] && n + 1 < cap) {
        buf[n] = (char)tolower((unsigned char)v->str_val[n]);
        n++;
    }
    buf[n] = '\0';
    end = n;
    while (start < end && isspace((unsigned char)buf[start])) start++;
    while (end > start && isspace((unsigned char)buf[end - 1])) end--;
    if (start > 0) memmove(buf, buf + start, end - start);
    buf[end - start] = '\0';
}

static int aux_str_in_disabled(const json_t *v) {
    char buf[256];
    size_t i;
    /* An ABSENT key is Python's default "" (never a disabled effort); an
     * explicit null is str(None) == "none", which is one. */
    if (!v) return 0;
    aux_str_lower_strip(v, buf, sizeof(buf));
    for (i = 0; i < AUX_DISABLED_EFFORT_COUNT; i++)
        if (!strcmp(buf, AUX_DISABLED_EFFORTS[i])) return 1;
    return 0;
}

/* isinstance(x, dict) and x.get("enabled") is False — identity, not truthiness. */
static int aux_is_disabled(const json_t *cfg) {
    const json_t *enabled;
    if (!cfg || cfg->type != JSON_OBJECT) return 0;
    enabled = json_obj_get(cfg, "enabled");
    return enabled && enabled->type == JSON_BOOL && !enabled->bool_val;
}

static json_t *aux_floor_dict(void) {
    json_t *o = json_object();
    json_set(o, "enabled", json_bool(true));
    json_set(o, "effort", json_string(W26_AUXILIARYR_2D56C8_FLOOR_EFFORT));
    return o;
}

/* str(provider or "").strip().lower() */
static json_t *aux_py_strip_lower(const json_t *v) {
    char *buf, *p;
    size_t n = 0, start = 0, end;
    if (!v || v->type == JSON_NULL) return json_string("");
    if (v->type == JSON_BOOL) return json_string(v->bool_val ? "true" : "false");
    if (v->type == JSON_NUMBER) {
        char tmp[64];
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1e16 && d < 1e16) snprintf(tmp, sizeof(tmp), "%.0f", d);
        else snprintf(tmp, sizeof(tmp), "%.17g", d);
        return json_string(tmp);
    }
    if (v->type != JSON_STRING) return json_string(v->type == JSON_ARRAY ? "[]" : "{}");
    buf = aux_strdup(v->str_val ? v->str_val : "");
    if (!buf) return json_string("");
    n = strlen(buf);
    end = n;
    while (start < end && isspace((unsigned char)buf[start])) start++;
    while (end > start && isspace((unsigned char)buf[end - 1])) end--;
    buf[end] = '\0';
    p = buf + start;
    for (n = 0; p[n]; n++) p[n] = (char)tolower((unsigned char)p[n]);
    {
        json_t *out = json_string(p);
        free(buf);
        return out;
    }
}

/* urlparse(base_url or "").netloc */
static json_t *aux_netloc(const json_t *base_url) {
    const char *u;
    size_t len, i, net_start = 0, net_end;
    int have_scheme = 0;
    if (!base_url || base_url->type != JSON_STRING || !base_url->str_val) return json_string("");
    u = base_url->str_val;
    len = strlen(u);
    /* scheme: ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) ":" */
    if (len && isalpha((unsigned char)u[0])) {
        for (i = 1; i < len; i++) {
            char c = u[i];
            if (c == ':') { have_scheme = 1; break; }
            if (!(isalnum((unsigned char)c) || c == '+' || c == '-' || c == '.')) break;
        }
    }
    if (have_scheme) {
        if (i + 2 < len && u[i + 1] == '/' && u[i + 2] == '/') net_start = i + 3;
        else return json_string("");
    } else if (len >= 2 && u[0] == '/' && u[1] == '/') {
        net_start = 2;
    } else {
        return json_string("");
    }
    net_end = net_start;
    while (net_end < len && u[net_end] != '/' && u[net_end] != '?' && u[net_end] != '#') net_end++;
    {
        size_t n = 0;
        char *buf = (char *)malloc(net_end - net_start + 1);
        json_t *out;
        if (!buf) return json_string("");
        for (i = net_start; i < net_end; i++) buf[n++] = (char)tolower((unsigned char)u[i]);
        buf[n] = '\0';
        out = json_string(buf);
        free(buf);
        return out;
    }
}

json_t *w26_auxiliaryr_2d56c8_route_key(const json_t *provider, const json_t *base_url) {
    json_t *net = aux_netloc(base_url);
    json_t *out;
    if (net && net->type == JSON_STRING && net->str_val && net->str_val[0]) {
        out = json_string(net->str_val);
        json_free(net);
        return out;
    }
    json_free(net);
    return aux_py_strip_lower(provider);
}

json_t *w26_auxiliaryr_2d56c8_is_disabled(const json_t *reasoning_config) {
    return json_bool(aux_is_disabled(reasoning_config) ? true : false);
}

json_t *w26_auxiliaryr_2d56c8_floor_config(const json_t *reasoning_config) {
    if (aux_is_disabled(reasoning_config)) return aux_floor_dict();
    return reasoning_config ? json_copy(reasoning_config) : json_null();
}

json_t *w26_auxiliaryr_2d56c8_with_floor(const json_t *kwargs) {
    json_t *retry, *extra, *reasoning;
    int changed = 0;

    if (!kwargs || kwargs->type != JSON_OBJECT) return json_null();
    retry = json_copy(kwargs);
    if (!retry) return json_null();

    if (aux_str_in_disabled(json_obj_get(retry, "reasoning_effort"))) {
        json_set(retry, "reasoning_effort", json_string(W26_AUXILIARYR_2D56C8_FLOOR_EFFORT));
        changed = 1;
    }
    if (aux_is_disabled(json_obj_get(retry, "_reasoning_config"))) {
        json_set(retry, "_reasoning_config", aux_floor_dict());
        changed = 1;
    }
    extra = json_obj_get(retry, "extra_body");
    if (extra && extra->type == JSON_OBJECT) {
        json_t *new_extra;
        reasoning = json_obj_get(extra, "reasoning");
        if (aux_is_disabled(reasoning) ||
            (reasoning && reasoning->type == JSON_OBJECT &&
             aux_str_in_disabled(json_obj_get(reasoning, "effort")))) {
            new_extra = json_copy(extra);
            if (new_extra) {
                json_set(new_extra, "reasoning", aux_floor_dict());
                json_set(retry, "extra_body", new_extra);
                changed = 1;
            }
        }
    }
    if (!changed) {
        json_free(retry);
        return json_null();
    }
    return retry;
}

json_t *w26_auxiliaryr_2d56c8_known_floor(const json_t *reasoning_config, const json_t *provider,
                                          const json_t *base_url, const json_t *model,
                                          int known, int catalog) {
    (void)provider;
    (void)base_url;
    (void)model;
    if (!aux_is_disabled(reasoning_config)) return reasoning_config ? json_copy(reasoning_config) : json_null();
    if (!known && !catalog) return reasoning_config ? json_copy(reasoning_config) : json_null();
    return aux_floor_dict();
}

json_t *w26_auxiliaryr_2d56c8_constants(void) {
    json_t *o = json_object();
    json_t *efforts = json_array();
    size_t i;
    for (i = 0; i < AUX_DISABLED_EFFORT_COUNT; i++) json_append(efforts, json_string(AUX_DISABLED_EFFORTS[i]));
    json_set(o, "floor_effort", json_string(W26_AUXILIARYR_2D56C8_FLOOR_EFFORT));
    json_set(o, "disabled_efforts", efforts);
    return o;
}
