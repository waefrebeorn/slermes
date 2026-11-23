/*
 * gateway_migrate.c — C port of upstream Hermes hermes_cli/gateway_migrate.py.
 *
 * PoP: w13_gatewaymig_a82d78_constants @ hermes_cli/gateway_migrate.py (module constants)
 * PoP: w13_gatewaymig_a82d78_service_label @ hermes_cli/gateway_migrate.py:_service_label
 * PoP: w13_gatewaymig_a82d78_remove_verb @ hermes_cli/gateway_migrate.py:_remove_verb
 * PoP: w13_gatewaymig_a82d78_service_dict @ hermes_cli/gateway_migrate.py:_service_dict
 * PoP: w13_gatewaymig_a82d78_credential_key_names @ hermes_cli/gateway_migrate.py:_credential_key_names
 * PoP: w13_gatewaymig_a82d78_listener_url @ hermes_cli/gateway_migrate.py:_listener_url
 * PoP: w13_gatewaymig_a82d78_manifest_secondaries @ hermes_cli/gateway_migrate.py:_manifest_secondaries
 * PoP: w13_gatewaymig_a82d78_recorded_services @ hermes_cli/gateway_migrate.py:_recorded_services
 * PoP: w13_gatewaymig_a82d78_recorded_run_as_user @ hermes_cli/gateway_migrate.py:_recorded_run_as_user
 * PoP: w13_gatewaymig_a82d78_target_from_manifest @ hermes_cli/gateway_migrate.py:_target_from_manifest
 * PoP: w13_gatewaymig_a82d78_manifest_not_yet_served @ hermes_cli/gateway_migrate.py:_manifest_not_yet_served
 *
 * Seams injected by the fixture: credential_env_keys(), format_url_host (identity),
 * profile_is_parked(). No filesystem or environment is read directly.
 */
#include "noc_w13_gatewaymig_a82d78.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

struct json_t *w13_gatewaymig_a82d78_constants(void) {
    json_t *o = json_object();
    json_set(o, "MANIFEST_NAME", json_string("gateway_migration.json"));
    json_set(o, "MIGRATE_COMMAND", json_string("hermes gateway migrate --multiplex"));
    json_set(o, "SERVED_WAIT_SECONDS", json_number(90.0));
    return o;
}

struct json_t *w13_gatewaymig_a82d78_service_label(const char *kind, int system) {
    const char *k = kind ? kind : "";
    char buf[256];
    if (!strcmp(k, "systemd")) snprintf(buf, sizeof(buf), "systemd (%s)", system ? "system" : "user");
    else if (!strcmp(k, "s6")) snprintf(buf, sizeof(buf), "s6 slot");
    else if (!strcmp(k, "windows")) snprintf(buf, sizeof(buf), "Windows scheduled task");
    else snprintf(buf, sizeof(buf), "%s", k);
    return json_string(buf);
}

struct json_t *w13_gatewaymig_a82d78_remove_verb(const char *kind) {
    return json_string(kind && !strcmp(kind, "s6") ? "park" : "uninstall");
}

struct json_t *w13_gatewaymig_a82d78_service_dict(const char *kind, int system) {
    json_t *o = json_object();
    json_set(o, "kind", json_string(kind ? kind : ""));
    json_set(o, "system", json_bool(system ? true : false));
    return o;
}

/* sorted keys whose value == platform_value, joined by "/", else "the <pv> token". */
struct json_t *w13_gatewaymig_a82d78_credential_key_names(const char *platform_value,
                                                         const struct json_t *env_keys) {
    const char *names[64];
    size_t n = 0;
    if (json_is_object(env_keys)) {
        for (size_t i = 0; i < json_object_size(env_keys) && n < 64; i++) {
            const char *key = json_object_get_key_at(env_keys, i);
            json_t *v = json_object_get_at(env_keys, i);
            if (key && v && v->type == JSON_STRING && !strcmp(v->str_val, platform_value)) names[n++] = key;
        }
    }
    /* insertion sort for deterministic ascending order */
    for (size_t i = 1; i < n; i++) {
        const char *t = names[i];
        size_t j = i;
        while (j > 0 && strcmp(names[j - 1], t) > 0) { names[j] = names[j - 1]; j--; }
        names[j] = t;
    }
    if (n == 0) {
        char buf[512];
        snprintf(buf, sizeof(buf), "the %s token", platform_value ? platform_value : "");
        return json_string(buf);
    }
    size_t len = 1;
    for (size_t i = 0; i < n; i++) len += strlen(names[i]) + 1;
    char *out = (char *)calloc(1, len + 1);
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        if (i) out[k++] = '/';
        size_t l = strlen(names[i]); memcpy(out + k, names[i], l); k += l;
    }
    out[k] = '\0';
    json_t *r = json_string(out); free(out);
    return r;
}

struct json_t *w13_gatewaymig_a82d78_listener_url(const char *platform_value, const char *profile,
                                                  const char *extra_host, const char *extra_port) {
    const char *pv = platform_value ? platform_value : "";
    const char *host = "<host>", *port = "<port>", *tail = "/...";
    if (!strcmp(pv, "api_server")) { host = "127.0.0.1"; port = "8642"; tail = "/v1/..."; }
    else if (!strcmp(pv, "webhook")) { host = "0.0.0.0"; port = "8644"; tail = "/webhooks/<route>"; }
    if (extra_host && *extra_host) host = extra_host;
    if (extra_port && *extra_port) port = extra_port;
    char buf[2048];
    snprintf(buf, sizeof(buf), "http://%s:%s/p/%s%s", host, port, profile ? profile : "", tail);
    return json_string(buf);
}

struct json_t *w13_gatewaymig_a82d78_manifest_secondaries(const struct json_t *manifest) {
    if (!json_is_object(manifest)) return json_null();
    json_t *recs = json_obj_get(manifest, "secondaries");
    if (!recs) return json_array();       /* Python .get("secondaries", []) */
    if (!json_is_array(recs)) return json_null();
    for (size_t i = 0; i < json_len(recs); i++) {
        json_t *r = json_get(recs, i);
        json_t *p = json_is_object(r) ? json_obj_get(r, "profile") : NULL;
        json_t *h = json_is_object(r) ? json_obj_get(r, "home") : NULL;
        int p_ok = p && p->type == JSON_STRING && p->str_val[0];
        int h_ok = h && h->type == JSON_STRING && h->str_val[0];
        if (!p_ok || !h_ok) return json_null();
    }
    return json_copy(recs);
}

static json_t *service_from_dict(const struct json_t *service) {
    if (!json_is_object(service)) return NULL;
    json_t *k = json_obj_get(service, "kind");
    if (!k || k->type != JSON_STRING || !k->str_val[0]) return NULL;
    json_t *s = json_obj_get(service, "system");
    int system = (s && s->type == JSON_BOOL && s->bool_val) ? 1 : 0;
    json_t *pair = json_array();
    json_append(pair, json_string(k->str_val));
    json_append(pair, json_bool(system ? true : false));
    return pair;
}

struct json_t *w13_gatewaymig_a82d78_recorded_services(const struct json_t *rec) {
    json_t *out = json_array();
    if (!json_is_object(rec)) return out;
    json_t *recorded = json_obj_get(rec, "services");
    if (json_is_array(recorded)) {
        for (size_t i = 0; i < json_len(recorded); i++) {
            json_t *pair = service_from_dict(json_get(recorded, i));
            if (pair) json_append(out, pair);
        }
        return out;
    }
    json_t *single = service_from_dict(json_obj_get(rec, "service"));
    if (single) json_append(out, single);
    return out;
}

struct json_t *w13_gatewaymig_a82d78_recorded_run_as_user(const struct json_t *rec) {
    if (!json_is_object(rec)) return json_null();
    json_t *u = json_obj_get(rec, "run_as_user");
    return (u && u->type == JSON_STRING && u->str_val[0]) ? json_copy(u) : json_null();
}

/* recs = [manifest.default, *secondaries] where dict; target = first service of first rec that has one;
 * user = first non-empty run_as_user. */
struct json_t *w13_gatewaymig_a82d78_target_from_manifest(const struct json_t *manifest) {
    json_t *result = json_array();
    json_append(result, json_null());
    json_append(result, json_null());
    if (!json_is_object(manifest)) return result;
    json_t *recs = json_array();
    json_t *d = json_obj_get(manifest, "default");
    if (json_is_object(d)) json_append(recs, json_copy(d));
    json_t *secs = w13_gatewaymig_a82d78_manifest_secondaries(manifest);
    if (json_is_array(secs)) {
        for (size_t i = 0; i < json_len(secs); i++) {
            json_t *s = json_get(secs, i);
            if (json_is_object(s)) json_append(recs, json_copy(s));
        }
    }
    json_free(secs);
    int have_target = 0, have_user = 0;
    for (size_t i = 0; i < json_len(recs); i++) {
        json_t *rec = json_get(recs, i);
        if (!have_target) {
            json_t *services = w13_gatewaymig_a82d78_recorded_services(rec);
            if (json_len(services) > 0) {
                json_t *first = json_get(services, 0);
                json_array_set(result, 0, json_copy(first));
                have_target = 1;
            }
            json_free(services);
        }
        if (!have_user) {
            json_t *u = w13_gatewaymig_a82d78_recorded_run_as_user(rec);
            if (u->type == JSON_STRING) { json_array_set(result, 1, u); have_user = 1; }
            else json_free(u);
        }
    }
    json_free(recs);
    return result;
}

static int in_list(const struct json_t *arr, const char *s) {
    if (!json_is_array(arr) || !s) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}

struct json_t *w13_gatewaymig_a82d78_manifest_not_yet_served(const struct json_t *manifest,
                                                             const struct json_t *live_served,
                                                             const struct json_t *parked) {
    if (!json_is_object(manifest)) return json_bool(false);
    json_t *recs = json_array();
    json_t *d = json_obj_get(manifest, "default");
    if (json_is_object(d)) json_append(recs, json_copy(d));
    json_t *secs = w13_gatewaymig_a82d78_manifest_secondaries(manifest);
    if (json_is_array(secs)) {
        for (size_t i = 0; i < json_len(secs); i++) {
            json_t *s = json_get(secs, i);
            if (json_is_object(s)) json_append(recs, json_copy(s));
        }
    }
    json_free(secs);
    const char *migrated[128]; size_t nm = 0;
    migrated[nm++] = "default";
    for (size_t i = 0; i < json_len(recs) && nm < 128; i++) {
        json_t *rec = json_get(recs, i);
        json_t *home = json_obj_get(rec, "home");
        int home_ok = home && home->type == JSON_STRING && home->str_val[0];
        if (home_ok && in_list(parked, home->str_val)) continue;
        json_t *p = json_obj_get(rec, "profile");
        const char *name = (p && p->type == JSON_STRING && p->str_val[0]) ? p->str_val : "default";
        int seen = 0;
        for (size_t j = 0; j < nm; j++) if (!strcmp(migrated[j], name)) { seen = 1; break; }
        if (!seen) migrated[nm++] = name;
    }
    json_free(recs);
    int missing = 0;
    for (size_t i = 0; i < nm; i++) if (!in_list(live_served, migrated[i])) { missing = 1; break; }
    return json_bool(missing ? true : false);
}
