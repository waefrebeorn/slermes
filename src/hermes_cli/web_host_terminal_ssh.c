/*
 * web_host_terminal_ssh.c — C home for upstream Hermes hermes_cli/web_host_terminal_ssh.py.
 * STATUS: PORTED (gate w25_webhostter_aa608d) — differential oracle green.
 * PoP: w25_webhostter_aa608d_profile @ hermes_cli/web_host_terminal_ssh.py:_profile
 * PoP: w25_webhostter_aa608d_entry @ hermes_cli/web_host_terminal_ssh.py:_entry
 * PoP: w25_webhostter_aa608d_dial_fields_changed @ hermes_cli/gateway_connections_normalize.py:connection_dial_fields_changed
 * PoP: w25_webhostter_aa608d_matches @ hermes_cli/web_host_terminal_ssh.py:SshOwner.matches
 * PoP: w25_webhostter_aa608d_constants @ hermes_cli/gateway_connections_normalize.py:<dial fields>
 *
 * The serving Webapp owns the PTY and its retention; remote ``serve --isolated``
 * stays headless.  The shell rides the same SSH master as native Desktop's
 * terminal, so a live terminal must keep proving it is still bound to the saved
 * connection it was admitted for.  The PTY, the tunnel, the registry file, the
 * websocket and the reaper are seams; ported here is what those values decide.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_webhostter_aa608d.h"

/* kind is checked on its own; these are the dial fields compared one by one. */
static const char *const HS_DIAL_FIELDS[W25_WEBHOSTTER_AA608D_DIAL_FIELDS] = {
    "url", "authMode", "org", "host", "user", "port", "keyPath",
    "remoteHermesPath", "remoteProfile",
};

static int hs_truthy(const json_t *v) {
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

static int hs_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python '==' over JSON shapes: absent == null == None, True == 1, 1 == 1.0. */
static int hs_equal(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    size_t i, n;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (hs_number(a, &x) && hs_number(b, &y)) return x == y;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!hs_equal(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !hs_equal(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

static int hs_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

json_t *w25_webhostter_aa608d_profile(const json_t *query_params) {
    const json_t *v = query_params && query_params->type == JSON_OBJECT
                          ? json_obj_get(query_params, "profile") : NULL;
    const char *s;
    size_t start, end;
    char *buf;
    json_t *out;

    if (!hs_truthy(v)) return json_string(W25_WEBHOSTTER_AA608D_DEFAULT_PROFILE);
    if (v->type != JSON_STRING) return json_string(W25_WEBHOSTTER_AA608D_DEFAULT_PROFILE);
    s = v->str_val ? v->str_val : "";
    start = 0;
    end = strlen(s);
    while (s[start] && hs_is_space(s[start])) start++;
    while (end > start && hs_is_space(s[end - 1])) end--;
    buf = (char *)malloc(end - start + 1);
    if (!buf) return json_string(W25_WEBHOSTTER_AA608D_DEFAULT_PROFILE);
    memcpy(buf, s + start, end - start);
    buf[end - start] = '\0';
    out = json_string(buf);
    free(buf);
    return out;
}

json_t *w25_webhostter_aa608d_entry(const json_t *registry, const json_t *connection_id) {
    const json_t *conns = registry && registry->type == JSON_OBJECT
                              ? json_obj_get(registry, "connections") : NULL;
    size_t i, n;

    if (!conns || conns->type != JSON_ARRAY) return json_null();
    n = json_len(conns);
    for (i = 0; i < n; i++) {
        const json_t *c = json_get(conns, i);
        if (!c || c->type != JSON_OBJECT) continue;
        if (hs_equal(json_obj_get(c, "id"), connection_id)) return json_copy(c);
    }
    return json_null();
}

json_t *w25_webhostter_aa608d_dial_fields_changed(const json_t *before, const json_t *after) {
    size_t i;

    if (!hs_equal(json_obj_get(before, "kind"), json_obj_get(after, "kind")))
        return json_bool(1);
    for (i = 0; i < W25_WEBHOSTTER_AA608D_DIAL_FIELDS; i++) {
        const char *f = HS_DIAL_FIELDS[i];
        if (!hs_equal(json_obj_get(before, f), json_obj_get(after, f))) return json_bool(1);
    }
    if (!hs_equal(json_obj_get(before, "token"), json_obj_get(after, "token"))) return json_bool(1);
    return json_bool(!hs_equal(json_obj_get(before, "headers"), json_obj_get(after, "headers")));
}

json_t *w25_webhostter_aa608d_matches(const json_t *owner, const json_t *selected,
                                      const json_t *query_params) {
    const json_t *sel_conn, *sel_path, *owner_conn, *owner_path, *profile;
    int same;

    if (!selected || selected->type == JSON_NULL) return json_bool(0);
    if (selected->type != JSON_OBJECT) return json_bool(0);
    sel_conn = json_obj_get(selected, "connection_id");
    sel_path = json_obj_get(selected, "store_path");
    owner_conn = owner ? json_obj_get(owner, "connection_id") : NULL;
    owner_path = owner ? json_obj_get(owner, "store_path") : NULL;

    same = hs_equal(sel_conn, owner_conn) && hs_equal(sel_path, owner_path);
    if (!same) return json_bool(0);
    profile = w25_webhostter_aa608d_profile(query_params);
    {
        const json_t *want = owner ? json_obj_get(owner, "profile") : NULL;
        same = hs_equal(profile, want);
    }
    json_free((json_t *)profile);
    return json_bool(same);
}

json_t *w25_webhostter_aa608d_constants(void) {
    json_t *o = json_object();
    json_t *fields = json_array();
    size_t i;
    for (i = 0; i < W25_WEBHOSTTER_AA608D_DIAL_FIELDS; i++)
        json_append(fields, json_string(HS_DIAL_FIELDS[i]));
    json_set(o, "dial_fields", fields);
    json_set(o, "default_profile", json_string(W25_WEBHOSTTER_AA608D_DEFAULT_PROFILE));
    json_set(o, "ssh_kind", json_string("ssh"));
    return o;
}
