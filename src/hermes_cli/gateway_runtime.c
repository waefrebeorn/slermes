/*
 * gateway_runtime.c — C port of upstream Hermes hermes_cli/gateway_runtime.py.
 *
 * PoP: w13_gatewayrun_de650a_constants @ hermes_cli/gateway_runtime.py (module constants)
 * PoP: w13_gatewayrun_de650a_attachable @ hermes_cli/gateway_runtime.py:attachable
 * PoP: w13_gatewayrun_de650a_control_home_for @ hermes_cli/gateway_runtime.py:control_home_for
 * PoP: w13_gatewayrun_de650a_endpoint @ hermes_cli/gateway_runtime.py:_endpoint
 *
 * _canonical_home() is injected as identity and RESTART_WAIT_REASON as
 * "restart_wait" (gateway.drain_control seam). All validation is read-only.
 */
#include "noc_w13_gatewayrun_de650a.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESTART_WAIT "restart_wait"

struct json_t *w13_gatewayrun_de650a_constants(void) {
    json_t *o = json_object();
    json_set(o, "DEFAULT_ENSURE_TIMEOUT", json_number(60.0));
    json_set(o, "RESTART_WAIT_REASON", json_string(RESTART_WAIT));
    json_set(o, "REPLACE_FAILED_DETAIL",
             json_string("an older Hermes gateway still owns this profile after a "
                         "stop; run: hermes gateway "
                         "stop"));
    json_t *r = json_array();
    json_append(r, json_string("idle_exit")); json_append(r, json_string("replaced"));
    json_set(o, "RETIRE", r);
    return o;
}

struct json_t *w13_gatewayrun_de650a_attachable(const char *state, int has_endpoint,
                                                const char *reason_code, const char *restart_reason) {
    if (!has_endpoint) return json_bool(false);
    int ok = (state && !strcmp(state, "ready")) ||
             (state && !strcmp(state, "draining") && reason_code && restart_reason &&
              !strcmp(reason_code, restart_reason));
    return json_bool(ok ? true : false);
}

struct json_t *w13_gatewayrun_de650a_control_home_for(const char *home, const char *control_home) {
    const char *src = control_home && *control_home ? control_home : (home ? home : "");
    if (!control_home || !*control_home) return json_string(src);
    char buf[4096];
    size_t n = 0;
    for (const char *p = src; *p && n + 1 < sizeof(buf); ++p)
        buf[n++] = (*p == '/') ? '\\' : *p;
    buf[n] = 0;
    return json_string(buf);
}

/* result object with endpoint set (or null). */
static json_t *make_result(const char *state, const char *reason, const char *detail, json_t *ep) {
    json_t *o = json_object();
    json_set(o, "state", json_string(state ? state : ""));
    json_set(o, "reason_code", reason ? json_string(reason) : json_null());
    json_set(o, "detail", detail ? json_string(detail) : json_null());
    json_set(o, "endpoint", ep ? ep : json_null());
    return o;
}

static int loopback_host(const char *host) {
    if (!host) return 0;
    if (!strcmp(host, "::1")) return 1;
    if (!strncmp(host, "127.", 4)) {
        int b, c, d; char extra;
        if (sscanf(host, "127.%d.%d.%d%c", &b, &c, &d, &extra) == 3 &&
            b >= 0 && b <= 255 && c >= 0 && c <= 255 && d >= 0 && d <= 255) return 1;
    }
    return 0;
}

/* Minimal urlsplit + ipaddress.is_loopback for local control origins. */
static int valid_origin(const char *origin) {
    if (!origin) return 0;
    const char *sep = strstr(origin, "://");
    if (!sep || sep == origin) return 0;
    size_t sl = (size_t)(sep - origin);
    if (!((sl == 4 && !strncmp(origin, "http", 4)) || (sl == 5 && !strncmp(origin, "https", 5)))) return 0;
    const char *end = sep + 3;
    while (*end && *end != '/' && *end != '?' && *end != '#') end++;
    size_t alen = (size_t)(end - (sep + 3));
    if (alen == 0 || alen >= 1024) return 0;
    char buf[1024];
    memcpy(buf, sep + 3, alen); buf[alen] = '\0';
    if (strchr(buf, '@')) return 0;                 /* username/password */
    char *host, *portstr = NULL;
    if (buf[0] == '[') {
        char *close = strchr(buf, ']');
        if (!close) return 0;
        *close = '\0'; host = buf + 1;
        char *rest = close + 1;
        if (*rest) { if (*rest != ':') return 0; portstr = rest + 1; }
    } else {
        char *colon = strchr(buf, ':');
        if (colon) { *colon = '\0'; host = buf; portstr = colon + 1; if (strchr(portstr, ':')) return 0; }
        else host = buf;
    }
    if (!*host || !portstr || !*portstr) return 0;
    for (char *q = portstr; *q; q++) if (!isdigit((unsigned char)*q)) return 0;
    if (strtol(portstr, NULL, 10) == 0) return 0;
    if (!loopback_host(host)) return 0;
    if (*end == '/') return 0;                      /* any path is truthy */
    const char *q = strchr(end, '?');
    if (q) {
        const char *qs = q + 1, *hash = strchr(qs, '#');
        size_t qlen = hash ? (size_t)(hash - qs) : strlen(qs);
        if (qlen > 0) return 0;
    }
    const char *h = strchr(end, '#');
    if (h && h[1]) return 0;
    return 1;
}

static int str_in_list(const struct json_t *arr, const char *s) {
    if (!json_is_array(arr) || !s) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}

static json_t *sorted_strings(const struct json_t *arr) {
    json_t *out = json_array();
    size_t n = json_is_array(arr) ? json_len(arr) : 0;
    const char *vals[256];
    if (n > 256) n = 256;
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(arr, i);
        vals[i] = (v && v->type == JSON_STRING) ? v->str_val : "";
    }
    for (size_t i = 1; i < n; i++) {
        const char *t = vals[i]; size_t j = i;
        while (j > 0 && strcmp(vals[j - 1], t) > 0) { vals[j] = vals[j - 1]; j--; }
        vals[j] = t;
    }
    for (size_t i = 0; i < n; i++) json_append(out, json_string(vals[i]));
    return out;
}

static int integral_number(const json_t *v) {
    return v && v->type == JSON_NUMBER && v->num_val == (double)(long long)v->num_val;
}

struct json_t *w13_gatewayrun_de650a_endpoint(const struct json_t *payload, const char *home,
                                              const char *control_home) {
    if (!json_is_object(payload)) return make_result("incompatible", "runtime_protocol", NULL, NULL);
    json_t *proto = json_obj_get(payload, "runtime_protocol");
    if (!integral_number(proto) || proto->num_val != 1.0)
        return make_result("incompatible", "runtime_protocol", NULL, NULL);
    json_t *profiles = json_obj_get(payload, "served_profiles");
    if (!json_is_array(profiles)) return make_result("inaccessible", "profile_mismatch", NULL, NULL);
    const char *wanted = home ? home : "";
    const char *pid = NULL;
    int matches = 0;
    for (size_t i = 0; i < json_len(profiles); i++) {
        json_t *p = json_get(profiles, i);
        if (!json_is_object(p)) continue;
        json_t *ph = json_obj_get(p, "home");
        if (!ph || ph->type != JSON_STRING || !ph->str_val) continue;
        if (strcmp(ph->str_val, wanted) != 0) continue;
        json_t *id = json_obj_get(p, "profile_id");
        if (!id || id->type != JSON_STRING || !id->str_val[0]) continue;
        pid = id->str_val;
        matches++;
    }
    if (matches != 1) return make_result("inaccessible", "profile_mismatch", NULL, NULL);
    const char *state = json_get_str(payload, "state", "");
    json_t *drain = json_obj_get(payload, "drain_reason");
    const char *dr = (drain && drain->type == JSON_STRING) ? drain->str_val : NULL;
    int restart_wait = !strcmp(state, "draining") && dr && !strcmp(dr, RESTART_WAIT);
    if ((!strcmp(state, "starting") || !strcmp(state, "draining") || !strcmp(state, "conflict")) && !restart_wait) {
        int idle = !strcmp(state, "draining") && dr && !strcmp(dr, "idle_exit");
        return make_result(state, idle ? "idle_exit" : NULL, NULL, NULL);
    }
    if (strcmp(state, "ready") && !restart_wait) return make_result("inaccessible", "invalid_runtime_state", NULL, NULL);
    json_t *caps = json_obj_get(payload, "capabilities");
    int caps_ok = json_is_array(caps);
    if (caps_ok) for (size_t i = 0; i < json_len(caps); i++) {
        json_t *c = json_get(caps, i);
        if (!c || c->type != JSON_STRING) { caps_ok = 0; break; }
    }
    if (!str_in_list(caps, "session-authority-v1")) caps_ok = 0;
    if (!caps_ok) return make_result("incompatible", "session_authority_unavailable", NULL, NULL);
    json_t *epoch = json_obj_get(payload, "authority_epoch");
    json_t *inst = json_obj_get(payload, "instance_id");
    if (!integral_number(epoch) || epoch->num_val <= 0 ||
        !inst || inst->type != JSON_STRING || !inst->str_val[0])
        return make_result("inaccessible", "invalid_runtime_identity", NULL, NULL);
    json_t *origin = json_obj_get(payload, "api_origin");
    if (!origin || origin->type != JSON_STRING || !valid_origin(origin->str_val))
        return make_result("inaccessible", "invalid_api_origin", NULL, NULL);
    json_t *sup = json_obj_get(payload, "supervisor");
    const char *supervisors[] = {"none", "systemd", "launchd", "windows", "external"};
    int sup_ok = 0;
    if (sup && sup->type == JSON_STRING)
        for (int i = 0; i < 5; i++) if (!strcmp(sup->str_val, supervisors[i])) { sup_ok = 1; break; }
    if (!sup_ok) return make_result("inaccessible", "invalid_supervisor", NULL, NULL);

    json_t *ep = json_object();
    json_set(ep, "profile_id", json_string(pid));
    json_set(ep, "instance_id", json_string(inst->str_val));
    json_set(ep, "authority_epoch", json_number((double)(long long)epoch->num_val));
    json_set(ep, "runtime_protocol", json_number(1));
    json_set(ep, "api_origin", json_string(origin->str_val));
    json_set(ep, "supervisor", json_string(sup->str_val));
    json_set(ep, "capabilities", sorted_strings(caps));
    json_set(ep, "control_home", control_home && *control_home ? json_string(control_home) : json_null());
    json_t *sha = json_obj_get(payload, "code_sha");
    json_set(ep, "code_sha", (sha && sha->type == JSON_STRING && sha->str_val[0]) ? json_copy(sha) : json_null());
    return make_result(restart_wait ? "draining" : "ready",
                       restart_wait ? RESTART_WAIT : NULL, NULL, ep);
}
