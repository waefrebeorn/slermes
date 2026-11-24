/*
 * status.c - C11 port of upstream hermes_cli/web_routers/status.py
 * (gates w29_status_6b0cd0_p1 and w29_status_6b0cd0_p2).
 *
 * STATUS: behavior port.  Every HTTP/DB/process probe the module performs is a
 * seam: this file owns only what the values those probes return decide.
 *
 * PoP: w29_status_6b0cd0_is_key            @ hermes_cli/web_routers/status.py:_is_profile_platform_status_key
 * PoP: w29_status_6b0cd0_key_allowed       @ hermes_cli/web_routers/status.py:_status_platform_key_allowed
 * PoP: w29_status_6b0cd0_public_entry      @ hermes_cli/web_routers/status.py:_public_platform_entry
 * PoP: w29_status_6b0cd0_merge_platforms   @ hermes_cli/web_routers/status.py:_merge_profile_gateway_platforms
 * PoP: w29_status_6b0cd0_project_platforms @ hermes_cli/web_routers/status.py:_project_gateway_platforms
 * PoP: w29_status_6b0cd0_gateway_view      @ hermes_cli/web_routers/status.py:_resolve_gateway_status
 * PoP: w29_status_6b0cd0_component_health  @ hermes_cli/web_routers/status.py:_component_health
 * PoP: w29_status_6b0cd0_overall           @ hermes_cli/web_routers/status.py:get_status
 * PoP: w29_status_6b0cd0_fts_rebuild       @ hermes_cli/web_routers/status.py:_fts_rebuild_status
 * PoP: w29_status_6b0cd0_signed_out        @ hermes_cli/web_routers/status.py:_webapp_caller_signed_out
 * PoP: w29_status_6b0cd0_signed_out_filter @ hermes_cli/web_routers/status.py:get_status
 * PoP: w29_status_6b0cd0_active_sessions   @ hermes_cli/web_routers/status.py:_count_status_active_sessions
 * PoP: w29_status_6b0cd0_feature_state     @ hermes_cli/web_routers/status.py:_feature_state
 * PoP: w29_status_6b0cd0_portal_status     @ hermes_cli/web_routers/status.py:_get_portal_status_sync
 * PoP: w29_status_6b0cd0_log_query         @ hermes_cli/web_routers/status.py:get_logs
 * PoP: w29_status_6b0cd0_debug_share       @ hermes_cli/web_routers/status.py:run_debug_share_endpoint
 * PoP: w29_status_6b0cd0_retirement        @ hermes_cli/web_routers/status.py:post_health_retirement
 * PoP: w29_status_6b0cd0_curator_status    @ hermes_cli/web_routers/status.py:get_curator_status
 * PoP: w29_status_6b0cd0_local_reads       @ hermes_cli/web_routers/status.py:_status_local_reads
 *
 * Seams (deliberately not ported here, supplied as fixture inputs): the
 * fastapi/starlette request cycle, the session SQLite store, the config and
 * auth stores, psutil, the log tail reader, the retirement ledger, the
 * curator/portal modules, and gateway.status' normalize_updated_at.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_status_6b0cd0_p1.h"
#include "noc_w29_status_6b0cd0_p2.h"

/* ------------------------------------------------------------------ */
/* Python value semantics over a json_t tree                           */
/* ------------------------------------------------------------------ */

static int st_present(const json_t *v) { return v && v->type != JSON_NULL; }

static int st_truthy(const json_t *v) {
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

static int st_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL)   { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

static const char *st_cstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static int st_eq_str(const json_t *v, const char *s) {
    return v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, s) == 0;
}

static int st_str_in(const json_t *v, const char *const *set, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        if (st_eq_str(v, set[i])) return 1;
    return 0;
}

/* Python str(value) for the scalar shapes these payloads carry. */
static void st_py_str(const json_t *v, char *buf, size_t cap, int lower) {
    size_t i;
    buf[0] = '\0';
    if (!v) return;
    if (v->type == JSON_STRING) {
        snprintf(buf, cap, "%s", v->str_val ? v->str_val : "");
    } else if (v->type == JSON_BOOL) {
        snprintf(buf, cap, "%s", v->bool_val ? "True" : "False");
    } else if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1e15 && d < 1e15)
            snprintf(buf, cap, "%lld", (long long)d);
        else
            snprintf(buf, cap, "%.15g", d);
    }
    if (!lower) return;
    for (i = 0; buf[i]; i++)
        if (buf[i] >= 'A' && buf[i] <= 'Z') buf[i] = (char)(buf[i] - 'A' + 'a');
}

static void st_put(json_t *obj, const char *key, const json_t *v) {
    json_set(obj, key, v ? json_copy(v) : json_null());
}

static char *st_fmt(const char *fmt, ...) {
    va_list ap, ap2;
    int need;
    char *buf;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0) { va_end(ap2); return NULL; }
    buf = (char *)malloc((size_t)need + 1);
    if (buf) vsnprintf(buf, (size_t)need + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

/* Python int() over the scalar shapes the request models carry. */
static long long st_py_int(const json_t *v, long long def) {
    double d;
    if (!v) return def;
    if (st_number(v, &d)) return (long long)d;
    if (v->type == JSON_STRING && v->str_val) {
        char *end = NULL;
        long long r = strtoll(v->str_val, &end, 10);
        if (end == v->str_val) return def;
        return r;
    }
    return def;
}

/* ------------------------------------------------------------------ */
/* p1 - platform key grammar and public projection                     */
/* ------------------------------------------------------------------ */

static int st_segment_ok(const char *s, size_t n) {
    size_t i;
    if (n < 1 || n > 64) return 0;
    if (!((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= '0' && s[0] <= '9'))) return 0;
    for (i = 1; i < n; i++) {
        char c = s[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') continue;
        return 0;
    }
    return 1;
}

/* ^[a-z0-9][a-z0-9_-]{0,63}:[a-z0-9][a-z0-9_-]{0,63}$  (exactly one colon) */
static int st_is_key_str(const char *s) {
    const char *colon;
    size_t left, right, n;
    if (!s) return 0;
    colon = strchr(s, ':');
    if (!colon) return 0;
    if (strchr(colon + 1, ':')) return 0;
    left = (size_t)(colon - s);
    n = strlen(s);
    right = n - left - 1;
    return st_segment_ok(s, left) && st_segment_ok(colon + 1, right);
}

static int st_key_allowed_str(const char *s, const json_t *configured) {
    size_t i;
    if (!s) return 0;
    if (strchr(s, ':')) return st_is_key_str(s);
    /* `configured is None or key in configured`: only an ABSENT set passes keys
     * through. An empty set is a loaded-but-empty config and admits nothing --
     * treating it as None was failing open, which is exactly what the docstring
     * in the upstream module warns about. */
    if (!configured || configured->type == JSON_NULL) return 1;
    if (configured->type != JSON_ARRAY) return 0;
    for (i = 0; i < json_len(configured); i++)
        if (st_eq_str(json_get(configured, i), s)) return 1;
    return 0;
}

json_t *w29_status_6b0cd0_is_key(const json_t *key) {
    const char *s = st_cstr(key);
    return json_bool(s ? st_is_key_str(s) == 1 : false);
}

json_t *w29_status_6b0cd0_key_allowed(const json_t *key, const json_t *configured) {
    return json_bool(st_key_allowed_str(st_cstr(key), configured) ? true : false);
}

json_t *w29_status_6b0cd0_public_entry(const json_t *value) {
    json_t *out;
    size_t i;
    if (!value || value->type != JSON_OBJECT) return value ? json_copy(value) : json_null();
    out = json_object();
    for (i = 0; i < json_object_size(value); i++) {
        const char *k = json_object_get_key_at(value, i);
        if (!k) continue;
        if (!strcmp(k, "writer_pid") || !strcmp(k, "writer_start_time")) continue;
        st_put(out, k, json_object_get_at(value, i));
    }
    return out;
}

json_t *w29_status_6b0cd0_merge_platforms(const json_t *gateway_platforms,
                                          const json_t *profile_platforms,
                                          const json_t *active) {
    json_t *merged = json_object();
    size_t i, j;

    if (gateway_platforms && gateway_platforms->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(gateway_platforms); i++) {
            const char *k = json_object_get_key_at(gateway_platforms, i);
            if (k) st_put(merged, k, json_object_get_at(gateway_platforms, i));
        }
    }
    if (profile_platforms && profile_platforms->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(profile_platforms); i++) {
            const char *prof = json_object_get_key_at(profile_platforms, i);
            const json_t *plats = json_object_get_at(profile_platforms, i);
            if (!prof) continue;
            if (st_eq_str(active, prof)) continue;              /* active: already present */
            if (!plats || plats->type != JSON_OBJECT) continue; /* not a dict */
            for (j = 0; j < json_object_size(plats); j++) {
                const char *key = json_object_get_key_at(plats, j);
                const json_t *val = json_object_get_at(plats, j);
                char *namespaced;
                if (!key || strchr(key, ':')) continue;
                if (!val || val->type != JSON_OBJECT) continue;
                namespaced = st_fmt("%s:%s", prof, key);
                if (!namespaced) continue;
                if (st_is_key_str(namespaced) && !json_obj_get(merged, namespaced))
                    json_set(merged, namespaced, w29_status_6b0cd0_public_entry(val));
                free(namespaced);
            }
        }
    }
    return merged;
}

/* Public projection of one runtime's platform map. */
static json_t *st_project(const json_t *gateway_platforms, const json_t *configured,
                          int running, const json_t *gateway_state) {
    json_t *out = json_object();
    size_t i;

    if (gateway_platforms && gateway_platforms->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(gateway_platforms); i++) {
            const char *k = json_object_get_key_at(gateway_platforms, i);
            if (!k || !st_key_allowed_str(k, configured)) continue;
            json_set(out, k, w29_status_6b0cd0_public_entry(json_object_get_at(gateway_platforms, i)));
        }
    }
    if (running) return out;                       /* live gateway: keep every allowed entry */
    if (st_eq_str(gateway_state, "startup_failed")) {
        json_t *fatal = json_object();
        for (i = 0; i < json_object_size(out); i++) {
            const char *k = json_object_get_key_at(out, i);
            const json_t *v = json_object_get_at(out, i);
            if (v && v->type == JSON_OBJECT && st_eq_str(json_obj_get(v, "state"), "fatal"))
                json_set(fatal, k, json_copy(v));
        }
        json_free(out);
        return fatal;
    }
    json_free(out);
    return json_object();
}

json_t *w29_status_6b0cd0_project_platforms(const json_t *gateway_platforms,
                                            const json_t *configured,
                                            const json_t *gateway_running,
                                            const json_t *gateway_state) {
    return st_project(gateway_platforms, configured, st_truthy(gateway_running), gateway_state);
}

/* The state ladder after the liveness ladder (status.py:_resolve_gateway_status). */
json_t *w29_status_6b0cd0_gateway_view(const json_t *req) {
    static const char *const LIVE_STATES[] = {"running", "degraded", "starting"};
    const json_t *local_runtime = json_obj_get(req, "local_runtime");
    const json_t *remote = json_obj_get(req, "remote_body");
    const json_t *retained = json_obj_get(req, "retained_state");
    const json_t *heartbeat = json_obj_get(req, "heartbeat_age");
    const json_t *configured = json_obj_get(req, "configured");
    const json_t *runtime;
    int running = st_truthy(json_obj_get(req, "gateway_running"));
    int stale = st_truthy(json_obj_get(req, "stale"));
    int remote_present = st_present(remote);
    json_t *state, *platforms, *exit_reason, *stale_s, *shared;
    json_t *out = json_object();

    runtime = st_truthy(local_runtime) ? local_runtime : NULL;
    /* Prefer the remote /health/detailed body when the local file is absent. */
    if (!runtime && st_truthy(remote) && st_truthy(json_obj_get(remote, "gateway_state")))
        runtime = remote;

    state = json_null();
    platforms = json_object();
    exit_reason = json_null();
    stale_s = json_null();

    if (runtime && st_truthy(runtime)) {
        const json_t *gs = json_obj_get(runtime, "gateway_state");
        if (!running) {
            /* A durable operator stop outranks a retained startup_failed/degraded. */
            gs = retained;
        } else if (remote_present && (!st_present(gs) || st_eq_str(gs, "stopped"))) {
            /* The health probe confirmed the gateway is alive; the local file may be stale. */
            gs = json_string("running");
        } else if (st_str_in(gs, LIVE_STATES, 3) && stale) {
            stale_s = heartbeat ? json_copy(heartbeat) : json_null();
        }
        state = gs ? json_copy(gs) : json_null();
        platforms = st_project(json_obj_get(runtime, "platforms"), configured, running, state);
        exit_reason = st_eq_str(state, "stopped")
                          ? json_null()
                          : (st_present(json_obj_get(runtime, "exit_reason"))
                                 ? json_copy(json_obj_get(runtime, "exit_reason"))
                                 : json_null());
    }
    /* No runtime info at all but the health probe confirmed alive. */
    if (running && !st_present(state) && remote_present) {
        if (state) json_free(state);
        state = json_string("running");
    }

    shared = json_null();
    if (running && st_truthy(runtime)) {
        const json_t *sw = json_obj_get(runtime, "served_profiles");
        if (sw && sw->type == JSON_ARRAY) {
            json_t *arr = json_array();
            size_t i;
            for (i = 0; i < json_len(sw); i++) {
                char buf[256];
                st_py_str(json_get(sw, i), buf, sizeof buf, 0);
                json_append(arr, json_string(buf));
            }
            shared = arr;
        }
    }

    json_set(out, "gateway_running", json_bool(running ? true : false));
    st_put(out, "gateway_pid", json_obj_get(req, "pid"));
    json_set(out, "gateway_state", state);
    json_set(out, "gateway_platforms", platforms);
    json_set(out, "gateway_exit_reason", exit_reason);
    json_set(out, "gateway_heartbeat_stale_s", stale_s);
    json_set(out, "gateway_shared_with", shared);
    return out;
}

/* Component-level health rollup: counts and status enums only. */
json_t *w29_status_6b0cd0_component_health(const json_t *req) {
    static const char *const OK_STATES[] = {"running", "draining"};
    int running = st_truthy(json_obj_get(req, "gateway_running"));
    const json_t *state = json_obj_get(req, "gateway_state");
    const json_t *platforms = json_obj_get(req, "gateway_platforms");
    const json_t *dashboard = json_obj_get(req, "dashboard");
    const json_t *storage = json_obj_get(req, "storage");
    int storage_error = st_truthy(json_obj_get(req, "storage_error"));
    json_t *out = json_object();
    json_t *gw = json_object();
    json_t *st;
    json_t *rollup;
    size_t i, configured = 0, connected = 0;

    json_set(gw, "status", json_string((running && st_str_in(state, OK_STATES, 2)) ? "ok" : "degraded"));
    if (st_truthy(state)) json_set(gw, "state", json_copy(state));
    else json_set(gw, "state", json_string(running ? "running" : "stopped"));
    json_set(out, "gateway", gw);
    st_put(out, "dashboard", dashboard);

    st = json_object();
    if (!storage_error && storage && storage->type == JSON_OBJECT) {
        const json_t *st_status = json_obj_get(storage, "status");
        if (st_status) json_set(st, "status", json_copy(st_status));
        else json_set(st, "status", json_string("degraded"));
        if (st_eq_str(json_obj_get(storage, "detail"), "corrupt"))
            json_set(st, "reason", json_string("corrupt"));
    } else {
        json_set(st, "status", json_string("degraded"));
    }
    json_set(out, "storage", st);

    if (platforms && platforms->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(platforms); i++) {
            const json_t *v = json_object_get_at(platforms, i);
            const json_t *s;
            char buf[256];
            if (!v || v->type != JSON_OBJECT) continue;
            s = json_obj_get(v, "state");
            if (!st_truthy(s)) s = json_obj_get(v, "status");
            if (!st_truthy(s)) s = NULL;              /* "" */
            st_py_str(s, buf, sizeof buf, 1);
            if (!strcmp(buf, "disabled")) continue;   /* informational, not degraded */
            configured++;
            if (!strcmp(buf, "connected") || !strcmp(buf, "running") || !strcmp(buf, "ok")) connected++;
        }
    }
    rollup = json_object();
    json_set(rollup, "status", json_string(connected == configured ? "ok" : "degraded"));
    json_set(rollup, "configured", json_number((double)configured));
    json_set(rollup, "connected", json_number((double)connected));
    json_set(out, "platforms", rollup);
    return out;
}

/* The inline verdict: "ok" only when every component's status is "ok". */
json_t *w29_status_6b0cd0_overall(const json_t *components) {
    size_t i;
    int all_ok = 1;
    if (!components || components->type != JSON_OBJECT) return json_string("degraded");
    for (i = 0; i < json_object_size(components); i++) {
        const json_t *item = json_object_get_at(components, i);
        if (!item || item->type != JSON_OBJECT || !st_eq_str(json_obj_get(item, "status"), "ok")) {
            all_ok = 0;
            break;
        }
    }
    return json_string(all_ok ? "ok" : "degraded");
}

/* Deferred FTS rebuild progress (percent clamped at 100), or null. */
json_t *w29_status_6b0cd0_fts_rebuild(const json_t *req) {
    const json_t *rebuild = json_obj_get(req, "rebuild");
    double total, indexed;
    long long pct;
    json_t *out;
    if (!st_truthy(json_obj_get(req, "store_exists"))) return json_null();
    if (!st_present(rebuild) || rebuild->type != JSON_OBJECT) return json_null();
    total = json_get_num(rebuild, "total", 0.0);
    indexed = json_get_num(rebuild, "indexed", 0.0);
    pct = (long long)(100.0 * indexed / total);   /* int() truncates toward zero */
    if (pct > 100) pct = 100;
    out = json_object();
    json_set(out, "pending", json_bool(true));
    st_put(out, "total", json_obj_get(rebuild, "total"));
    st_put(out, "indexed", json_obj_get(rebuild, "indexed"));
    json_set(out, "percent", json_number((double)pct));
    return out;
}

/* ------------------------------------------------------------------ */
/* p2 - Webapp signed-out gate and its key allowlist                   */
/* ------------------------------------------------------------------ */

json_t *w29_status_6b0cd0_signed_out(const json_t *req) {
    int verified = st_truthy(json_obj_get(req, "verified"));
    int webapp = st_eq_str(json_obj_get(req, "surface"), "webapp");
    int gated = st_truthy(json_obj_get(req, "auth_required"));
    return json_bool((!verified && webapp && gated) ? true : false);
}

json_t *w29_status_6b0cd0_signed_out_filter(const json_t *status) {
    static const char *const KEEP[] = {
        "version", "release_date", "gateway_running", "gateway_state", "overall",
        "components", "auth_required", "auth_providers", "auth_flows",
    };
    json_t *out = json_object();
    size_t i, k;
    if (!status || status->type != JSON_OBJECT) return out;
    for (i = 0; i < json_object_size(status); i++) {
        const char *key = json_object_get_key_at(status, i);
        if (!key) continue;
        for (k = 0; k < sizeof KEEP / sizeof KEEP[0]; k++) {
            if (strcmp(key, KEEP[k])) continue;
            st_put(out, key, json_object_get_at(status, i));
            break;
        }
    }
    return out;
}

/* Sessions with no ended_at whose last activity is under 300s old. */
json_t *w29_status_6b0cd0_active_sessions(const json_t *req) {
    const json_t *sessions = json_obj_get(req, "sessions");
    double now = json_get_num(req, "now", 0.0);
    long long count = 0;
    size_t i;
    if (sessions && sessions->type == JSON_ARRAY) {
        for (i = 0; i < json_len(sessions); i++) {
            const json_t *row = json_get(sessions, i);
            const json_t *ended;
            double last_active;
            if (!row || row->type != JSON_OBJECT) continue;
            ended = json_obj_get(row, "ended_at");
            if (st_present(ended)) continue;          /* ended_at is not None */
            last_active = json_get_num(row, "last_active", 0.0);
            if ((now - last_active) < 300) count++;
        }
    }
    return json_number((double)count);
}

json_t *w29_status_6b0cd0_feature_state(const json_t *feat) {
    const json_t *provider;
    if (st_truthy(json_obj_get(feat, "managed_by_nous"))) return json_string("via Nous Portal");
    if (st_truthy(json_obj_get(feat, "active"))) {
        provider = json_obj_get(feat, "current_provider");
        if (st_truthy(provider)) return json_copy(provider);
        return json_string("active");
    }
    return json_string("not configured");
}

json_t *w29_status_6b0cd0_portal_status(const json_t *req) {
    const json_t *cfg = json_obj_get(req, "cfg");
    const json_t *auth = json_obj_get(req, "auth");
    const json_t *feats = json_obj_get(req, "features");
    const json_t *model_cfg;
    const json_t *provider;
    int unavailable = st_truthy(json_obj_get(req, "features_unavailable"));
    json_t *out = json_object();
    json_t *features = json_array();
    char buf[512];
    size_t i;

    if (!cfg || cfg->type != JSON_OBJECT) cfg = NULL;
    if (!auth || auth->type != JSON_OBJECT) auth = NULL;

    model_cfg = cfg ? json_obj_get(cfg, "model") : NULL;
    if (!model_cfg || model_cfg->type != JSON_OBJECT) model_cfg = NULL;
    provider = model_cfg ? json_obj_get(model_cfg, "provider") : NULL;
    st_py_str(st_truthy(provider) ? provider : NULL, buf, sizeof buf, 0);

    if (!unavailable && feats && feats->type == JSON_ARRAY) {
        for (i = 0; i < json_len(feats); i++) {
            const json_t *feat = json_get(feats, i);
            json_t *row = json_object();
            const json_t *label = json_obj_get(feat, "label");
            json_set(row, "label", label ? json_copy(label) : json_string(""));
            json_set(row, "state", w29_status_6b0cd0_feature_state(feat));
            json_append(features, row);
        }
    }

    json_set(out, "logged_in", json_bool(st_truthy(json_obj_get(auth, "logged_in"))));
    st_put(out, "portal_url", json_obj_get(auth, "portal_base_url"));
    st_put(out, "inference_url", json_obj_get(auth, "inference_base_url"));
    json_set(out, "provider", json_string(buf));
    json_set(out, "free_tier", json_bool(st_truthy(json_obj_get(auth, "free_tier"))));
    st_put(out, "account_tier", json_obj_get(auth, "account_tier"));
    json_set(out, "subscription_url", json_string("https://portal.nousresearch.com/manage-subscription"));
    json_set(out, "features", features);
    return out;
}

/* ------------------------------------------------------------------ */
/* p2 - /api/logs query parsing                                        */
/* ------------------------------------------------------------------ */

static int st_equals_ci(const char *a, const char *b) {
    size_t i;
    if (!a || !b) return 0;
    for (i = 0; a[i] && b[i]; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return 0;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static int st_ci_contains(const char *hay, const char *needle) {
    size_t nl = strlen(needle), i, j;
    if (nl == 0) return 1;
    for (i = 0; hay[i]; i++) {
        for (j = 0; j < nl; j++) {
            char a = hay[i + j], b = needle[j];
            if (!a) return 0;
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
        }
        if (j == nl) return 1;
    }
    return 0;
}

static int st_cmp_key(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Python seq[-n:] start index. */
static size_t st_slice_start(size_t len, long long n) {
    if (n == 0) return 0;                       /* [-0:] == [0:] */
    if (n > 0) return ((size_t)n >= len) ? 0 : len - (size_t)n;
    {
        size_t s = (size_t)(-n);
        return s > len ? len : s;
    }
}

json_t *w29_status_6b0cd0_log_query(const json_t *req) {
    const char *file = json_get_str(req, "file", "agent");
    long long lines = st_py_int(json_obj_get(req, "lines"), 100);
    const json_t *level = json_obj_get(req, "level");
    const json_t *component = json_obj_get(req, "component");
    const json_t *search = json_obj_get(req, "search");
    const json_t *log_files = json_obj_get(req, "log_files");
    const json_t *prefix_map = json_obj_get(req, "component_prefixes");
    const json_t *raw = json_obj_get(req, "lines_content");
    const json_t *name = NULL;
    const char *min_level = NULL;
    const json_t *comp_prefixes = NULL;
    int has_filters;
    long long tail_n;
    json_t *out;

    if (log_files && log_files->type == JSON_OBJECT)
        name = json_obj_get(log_files, file ? file : "");
    if (!st_truthy(name)) {
        char *detail = st_fmt("Unknown log file: %s", file ? file : "");
        out = json_object();
        if (detail) {
            json_set(out, "error", json_string(detail));
            free(detail);
        } else {
            json_set(out, "error", json_string("Unknown log file"));
        }
        return out;
    }
    if (!st_truthy(json_obj_get(req, "path_exists"))) {
        out = json_object();
        json_set(out, "file", json_string(file ? file : ""));
        json_set(out, "lines", json_array());
        return out;
    }

    if (st_truthy(level)) {
        const char *lv = st_cstr(level);
        if (lv && st_equals_ci(lv, "ALL")) min_level = NULL;
        else min_level = lv;
    }
    if (st_truthy(component)) {
        const char *comp = st_cstr(component);
        if (comp && !st_equals_ci(comp, "all")) {
            if (prefix_map && prefix_map->type == JSON_OBJECT)
                comp_prefixes = json_obj_get(prefix_map, comp);
            if (!st_present(comp_prefixes)) {
                size_t i, n = prefix_map && prefix_map->type == JSON_OBJECT
                                  ? json_object_size(prefix_map) : 0;
                const char **keys = (const char **)calloc(n ? n : 1, sizeof(char *));
                char *available = NULL;
                size_t used = 0, cap = 64;
                char *msg;
                available = (char *)calloc(cap, 1);
                if (keys && available) {
                    for (i = 0; i < n; i++) {
                        const char *k = json_object_get_key_at(prefix_map, i);
                        if (k) keys[used++] = k;
                    }
                    qsort((void *)keys, used, sizeof(char *), st_cmp_key);
                    for (i = 0; i < used; i++) {
                        size_t need = strlen(available) + strlen(keys[i]) + 3;
                        if (need > cap) {
                            size_t ncap = cap;
                            char *grown;
                            while (ncap < need) ncap *= 2;
                            grown = (char *)realloc(available, ncap);
                            if (!grown) break;
                            available = grown;
                            cap = ncap;
                        }
                        if (i) strcat(available, ", ");
                        strcat(available, keys[i]);
                    }
                }
                msg = st_fmt("Unknown component: %s. Available: %s",
                             comp, available ? available : "");
                out = json_object();
                json_set(out, "error", json_string(msg ? msg : "Unknown component"));
                free(msg);
                free(available);
                free((void *)keys);
                return out;
            }
        }
    }

    has_filters = min_level != NULL || st_truthy(comp_prefixes) || st_truthy(search);
    tail_n = st_truthy(search) ? 2000 : (lines < 500 ? lines : 500);

    {
        json_t *kept = json_array();
        size_t i, len = (raw && raw->type == JSON_ARRAY) ? json_len(raw) : 0;
        if (st_truthy(search)) {
            const char *needle = st_cstr(search);
            long long n = lines < 500 ? lines : 500;
            size_t start;
            json_t *sliced = json_array();
            for (i = 0; i < len; i++) {
                const json_t *row = json_get(raw, i);
                const char *s = st_cstr(row);
                if (s && needle && st_ci_contains(s, needle)) json_append(kept, json_copy(row));
            }
            start = st_slice_start(json_len(kept), n);
            for (i = start; i < json_len(kept); i++)
                json_append(sliced, json_copy(json_get(kept, i)));
            json_free(kept);
            out = json_object();
            json_set(out, "file", json_string(file ? file : ""));
            json_set(out, "lines", sliced);
        } else {
            /* _read_tail already bounded the result; only the search path trims. */
            for (i = 0; i < len; i++) json_append(kept, json_copy(json_get(raw, i)));
            out = json_object();
            json_set(out, "file", json_string(file ? file : ""));
            json_set(out, "lines", kept);
        }
    }
    json_set(out, "tail_n", json_number((double)tail_n));
    json_set(out, "has_filters", json_bool(has_filters ? true : false));
    if (min_level) json_set(out, "min_level", json_string(min_level));
    else json_set(out, "min_level", json_null());
    if (st_present(comp_prefixes)) st_put(out, "component_prefixes", comp_prefixes);
    else json_set(out, "component_prefixes", json_null());
    return out;
}

/* ------------------------------------------------------------------ */
/* p2 - diagnostics body guards                                        */
/* ------------------------------------------------------------------ */

json_t *w29_status_6b0cd0_debug_share(const json_t *req) {
    json_t *out;
    long long n;
    if (!st_truthy(json_obj_get(req, "consent"))) {
        out = json_object();
        json_set(out, "error", json_number(400));
        json_set(out, "detail",
                 json_string("Debug share requires explicit consent (consent: true)"));
        return out;
    }
    n = st_py_int(json_obj_get(req, "lines"), 100);
    if (n < 1) n = 1;
    if (n > 5000) n = 5000;
    out = json_object();
    json_set(out, "log_lines", json_number((double)n));
    return out;
}

json_t *w29_status_6b0cd0_retirement(const json_t *req) {
    static const char *const ACTIONS[] = {"prepare", "commit", "cancel"};
    json_t *out;
    const json_t *body, *action, *token;
    int i, is_action = 0;

    if (st_truthy(json_obj_get(req, "mounted"))) {
        json_t *result = json_object();
        json_set(result, "ok", json_bool(false));
        json_set(result, "idle", json_bool(false));
        json_set(result, "reason", json_string("gateway_owns_lifecycle"));
        out = json_object();
        json_set(out, "error", json_null());
        json_set(out, "detail", json_null());
        json_set(out, "result", result);
        return out;
    }
    if (st_truthy(json_obj_get(req, "json_invalid"))) {
        out = json_object();
        json_set(out, "error", json_number(400));
        json_set(out, "detail", json_string("JSON body required"));
        json_set(out, "result", json_null());
        return out;
    }
    body = json_obj_get(req, "body");
    action = (body && body->type == JSON_OBJECT) ? json_obj_get(body, "action") : NULL;
    for (i = 0; i < 3; i++)
        if (st_eq_str(action, ACTIONS[i])) is_action = 1;
    if (!body || body->type != JSON_OBJECT || !is_action) {
        out = json_object();
        json_set(out, "error", json_number(400));
        json_set(out, "detail", json_string("action must be prepare, commit, or cancel"));
        json_set(out, "result", json_null());
        return out;
    }
    if (st_eq_str(action, "prepare")) {
        out = json_object();
        json_set(out, "error", json_null());
        json_set(out, "detail", json_null());
        st_put(out, "result", json_obj_get(req, "prepare_result"));
        return out;
    }
    token = json_obj_get(body, "token");
    if (!st_cstr(token) || !st_cstr(token)[0]) {
        out = json_object();
        json_set(out, "error", json_number(400));
        json_set(out, "detail", json_string("token required"));
        json_set(out, "result", json_null());
        return out;
    }
    out = json_object();
    json_set(out, "error", json_null());
    json_set(out, "detail", json_null());
    {
        const json_t *ar = json_obj_get(req, "action_result");
        json_t *result = json_object();
        size_t i;
        if (ar && ar->type == JSON_OBJECT) {
            for (i = 0; i < json_object_size(ar); i++) {
                const char *k = json_object_get_key_at(ar, i);
                if (k) st_put(result, k, json_object_get_at(ar, i));
            }
        } else {
            json_set(result, "ok", json_bool(true));
        }
        json_set(result, "token", json_string(token->str_val));
        json_set(out, "result", result);
    }
    return out;
}

/* ------------------------------------------------------------------ */
/* p2 - curator status and the local reads payload                     */
/* ------------------------------------------------------------------ */

static int st_list_has(const json_t *list, const char *name) {
    size_t i;
    if (!list || list->type != JSON_ARRAY) return 0;
    for (i = 0; i < json_len(list); i++)
        if (st_eq_str(json_get(list, i), name)) return 1;
    return 0;
}

static const json_t *st_curator_get(const json_t *curator, const json_t *raises, const char *name) {
    if (!curator || curator->type != JSON_OBJECT) return NULL;
    if (st_list_has(raises, name)) return NULL;   /* accessor raises -> default */
    return json_obj_get(curator, name);
}

json_t *w29_status_6b0cd0_curator_status(const json_t *req) {
    const json_t *curator = json_obj_get(req, "curator");
    const json_t *raises = json_obj_get(req, "raises");
    const json_t *state = st_curator_get(curator, raises, "load_state");
    json_t *out = json_object();
    struct { const char *key; const char *name; const char *def_str; int def_bool; int def_kind; } rows[] = {
        {"enabled", "is_enabled", NULL, 1, 0},
        {"paused", "is_paused", NULL, 0, 0},
        {"interval_hours", "get_interval_hours", NULL, 0, 1},
    };
    size_t i;

    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        const json_t *v = st_curator_get(curator, raises, rows[i].name);
        if (v) st_put(out, rows[i].key, v);
        else if (rows[i].def_kind == 0) json_set(out, rows[i].key, json_bool(rows[i].def_bool ? true : false));
        else json_set(out, rows[i].key, json_null());
    }
    st_put(out, "last_run_at", (state && state->type == JSON_OBJECT)
                                   ? json_obj_get(state, "last_run_at") : NULL);
    {
        static const char *const EXTRA[] = {"min_idle_hours", "stale_after_days", "archive_after_days"};
        size_t k;
        for (k = 0; k < 3; k++) {
            char name[64];
            const json_t *v;
            snprintf(name, sizeof name, "get_%s", EXTRA[k]);
            v = st_curator_get(curator, raises, name);
            if (v) st_put(out, EXTRA[k], v);
            else json_set(out, EXTRA[k], json_null());
        }
    }
    return out;
}

json_t *w29_status_6b0cd0_local_reads(const json_t *req) {
    const json_t *cfgver = json_obj_get(req, "config_version");
    json_t *out = json_object();
    st_put(out, "version", json_obj_get(req, "version"));
    st_put(out, "config_version",
          (cfgver && cfgver->type == JSON_ARRAY) ? json_get(cfgver, 0) : NULL);
    st_put(out, "latest_config_version",
          (cfgver && cfgver->type == JSON_ARRAY) ? json_get(cfgver, 1) : NULL);
    json_set(out, "can_update_hermes",
             json_bool(st_truthy(json_obj_get(req, "managed_externally")) ? false : true));
    st_put(out, "nous_session_valid", json_obj_get(req, "nous"));
    return out;
}
