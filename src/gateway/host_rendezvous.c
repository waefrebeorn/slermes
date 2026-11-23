/*
 * host_rendezvous.c — C home for upstream Hermes gateway/host_rendezvous.py.
 *
 * PoP: w9_hostrendezv_37dd94_describe @ gateway/host_rendezvous.py:HostRecord.from_json+describe
 * PoP: w9_hostrendezv_37dd94_dial @ gateway/host_rendezvous.py:HostRecord.from_json+dial_host
 * PoP: w9_hostrendezv_37dd94_validated_role @ gateway/host_rendezvous.py:_validated_role
 * PoP: w9_hostrendezv_37dd94_from_json_ok @ gateway/host_rendezvous.py:HostRecord.from_json
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_hostrendezv_37dd94.h"

#define HOST_BUF 8192

static bool valid_role(const char *r) {
    return r && (!strcmp(r, "gateway") || !strcmp(r, "serve") || !strcmp(r, "desktop-serve"));
}
static bool dec_ll(const json_t *v, long long *out) {
    if (!v || v->type != JSON_STRING || !v->str_val) return false;
    char *end = NULL;
    long long x = strtoll(v->str_val, &end, 10);
    if (!end || *end) return false;
    *out = x;
    return true;
}
/* HostRecord.from_json coercions for the fields describe()/dial_host() use.
 * pid/port arrive as decimal strings; null/absent means None. */
typedef struct {
    long long pid;
    long long port;      /* 0 == None */
    const char *host;
    const json_t *profiles;
} host_view;

static bool host_view_from(const json_t *in, host_view *v) {
    memset(v, 0, sizeof(*v));
    if (!dec_ll(json_obj_get(in, "pid"), &v->pid) || v->pid <= 0) return false;
    const char *role = json_get_str(in, "role", "");
    if (!valid_role(role)) return false;
    const json_t *pv = json_obj_get(in, "port");
    if (pv && pv->type == JSON_STRING) {
        long long p = 0;
        if (dec_ll(pv, &p) && p > 0 && p <= 65535) v->port = p;
    }
    const json_t *hv = json_obj_get(in, "host");
    v->host = (hv && hv->type == JSON_STRING && hv->str_val && hv->str_val[0]) ? hv->str_val : "";
    v->profiles = json_obj_get(in, "profiles");
    return true;
}

json_t *w9_hostrendezv_37dd94_describe(const struct json_t *in) {
    host_view v;
    if (!host_view_from(in, &v)) return json_string("");
    char buf[HOST_BUF];
    char where[256];
    if (v.port) snprintf(where, sizeof(where), "%s:%lld", v.host[0] ? v.host : "127.0.0.1", v.port);
    else snprintf(where, sizeof(where), "no bound port");
    char profiles[HOST_BUF / 2];
    profiles[0] = '\0';
    size_t used = 0;
    size_t np = (v.profiles && v.profiles->type == JSON_ARRAY) ? json_len(v.profiles) : 0;
    for (size_t i = 0; i < np; i++) {
        const json_t *item = json_get(v.profiles, i);
        const char *s = (item && item->type == JSON_STRING && item->str_val) ? item->str_val : "";
        size_t sl = strlen(s);
        if (used + sl + 3 < sizeof(profiles)) {
            if (used) { memcpy(profiles + used, ", ", 2); used += 2; }
            memcpy(profiles + used, s, sl); used += sl; profiles[used] = '\0';
        }
    }
    if (!used) { memcpy(profiles, "unknown", 7); used = 7; profiles[used] = '\0'; }
    snprintf(buf, sizeof(buf), "PID %lld (%s; profiles: %s)", v.pid, where, profiles);
    return json_string(buf);
}

json_t *w9_hostrendezv_37dd94_dial(const struct json_t *in) {
    host_view v;
    if (!host_view_from(in, &v)) return json_string("");
    const char *host = v.host[0] ? v.host : "127.0.0.1";
    if (!strcmp(host, "0.0.0.0") || !strcmp(host, "::") || !strcmp(host, "*") || !strcmp(host, "")) host = "127.0.0.1";
    return json_string(host);
}

json_t *w9_hostrendezv_37dd94_validated_role(const struct json_t *in) {
    const char *role = json_get_str(in, "role", "");
    return valid_role(role) ? json_string(role) : json_null();
}

json_t *w9_hostrendezv_37dd94_from_json_ok(const struct json_t *in) {
    host_view v;
    return json_bool(host_view_from(in, &v));
}
