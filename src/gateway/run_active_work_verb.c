/*
 * run_active_work_verb.c — C port of upstream Hermes gateway/run_active_work_verb.py.
 *
 * PoP: w16_runactive_34c07d_profile_units @ gateway/run_active_work_verb.py:_profile_units
 *
 * The loop/threading handler is I/O; the gated slice is _profile_units' pure
 * namespace filtering + unit assembly, with cross-module helper results injected
 * as fixture seams (managed units, side/cron counts, pid).
 */
#include "noc_w16_runactive_34c07d.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}
static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof(b), "%.17g", v->num_val);
        return dupstr(b);
    }
    return dupstr("");
}
/* gateway/session.py::_session_key_namespace — pure, ported verbatim. */
static char *session_key_namespace(const json_t *profile) {
    if (!py_truthy(profile)) return dupstr("agent:main");
    char *ps = py_str(profile);
    char *out = NULL;
    if (!strcmp(ps, "default")) out = dupstr("agent:main");
    else if (!strcmp(ps, "main")) out = dupstr("agent:main~");
    else {
        size_t n = strlen(ps) + 16;
        out = (char *)malloc(n);
        if (out) snprintf(out, n, "agent:%s", ps);
    }
    free(ps);
    return out;
}
static const char *jkind(const json_t *u) {
    const json_t *k = json_obj_get(u, "kind");
    return (json_is_string(k) && k->str_val) ? k->str_val : NULL;
}

json_t *w16_runactive_34c07d_profile_units(const json_t *units, const json_t *profile,
                                           const json_t *home, const json_t *authority,
                                           const json_t *pid) {
    json_t *out = json_array();
    char *ns = session_key_namespace(profile);
    size_t plen = strlen(ns);
    char *prefix = (char *)malloc(plen + 2);
    if (prefix) { memcpy(prefix, ns, plen); prefix[plen] = ':'; prefix[plen+1] = '\0'; }
    free(ns);

    /* mine = [u for u in units if u.get("kind")=="chat" and str(u.get("session") or "").startswith(prefix)] */
    if (json_is_array(units)) {
        for (size_t i = 0; i < json_len(units); i++) {
            const json_t *u = json_get(units, i);
            if (!json_is_object(u)) continue;
            const char *kind = jkind(u);
            if (!kind || strcmp(kind, "chat")) continue;
            const json_t *sess = json_obj_get(u, "session");
            char *s = py_truthy(sess) ? py_str(sess) : dupstr("");
            int keep = s && strncmp(s, prefix, plen + 1) == 0;
            free(s);
            if (keep) json_append(out, json_copy(u));
        }
    }
    free(prefix);

    /* authority is None when home is None; injected helper results otherwise */
    int have = json_is_object(authority) && !json_is_null(home);
    long long crons = 0, sides = 0;
    if (have) {
        const json_t *c = json_obj_get(authority, "cron_count");
        const json_t *sd = json_obj_get(authority, "side_count");
        if (json_is_number(c)) crons = (long long)c->num_val;
        if (json_is_number(sd)) sides = (long long)sd->num_val;
        const json_t *mu = json_obj_get(authority, "managed_units");
        if (json_is_array(mu))
            for (size_t i = 0; i < json_len(mu); i++)
                json_append(out, json_copy(json_get(mu, i)));
    }
    for (long long i = 0; i < crons; i++) {
        json_t *u = json_object();
        json_set(u, "kind", json_string("cron"));
        json_set(u, "job_id", json_string("?"));
        json_set(u, "pid", json_copy(pid));
        json_append(out, u);
    }
    for (long long i = 0; i < sides; i++) {
        json_t *u = json_object();
        json_set(u, "kind", json_string("side_agent"));
        json_set(u, "pid", json_copy(pid));
        json_append(out, u);
    }
    return out;
}
