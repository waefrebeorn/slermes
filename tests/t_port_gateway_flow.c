/*
 * t_port_gateway_flow.c — oracle harness for the gateway/run.py FLOW cores
 * ported in src/gateway/run_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires each flow's seams to the SCRIPTED results in "in" (recording stubs
 * that append one trace event per seam call, in call order) and emits
 * {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...], ...}} per case.
 * tests/oracle_gateway_flow.py drives the LIVE Python with the same scripts.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "libjson/json.h"
#include "gateway_run_flow.h"

typedef struct {
    const json_t *in;
    grun_trace_t *trace;
    int attach_calls;
} script_ctx_t;

/* ── event builders ── */
static json_t *ev(const char *name) {
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    return e;
}
static void evs(json_t *e, const char *k, const char *v) {
    json_set(e, k, v ? json_string(v) : json_null());
}
static void evb(json_t *e, const char *k, bool v) { json_set(e, k, json_bool(v)); }
static void evn(json_t *e, const char *k, double v) { json_set(e, k, json_number(v)); }
static void evj(json_t *e, const char *k, const json_t *v) {
    json_set(e, k, v ? json_copy(v) : json_null());
}
static void rec(script_ctx_t *c, json_t *e) { grun_trace_add(c->trace, e); }

/* Python truthiness (harness-local copy; mirrors run_flow.c jtruth). */
static bool jtruth_h(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        default:          return v->c.count > 0;
    }
}

static const json_t *in_obj(const script_ctx_t *c, const char *k) {
    const json_t *v = json_obj_get(c->in, k);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}
static const json_t *in_arr(const script_ctx_t *c, const char *k) {
    const json_t *v = json_obj_get(c->in, k);
    return (v && v->type == JSON_ARRAY) ? v : NULL;
}
static bool in_bool(const script_ctx_t *c, const char *k) {
    return json_get_bool(c->in, k, false);
}
static const char *in_str(const script_ctx_t *c, const char *k, const char *def) {
    return json_get_str(c->in, k, def);
}

/* ── normalize_empty_agent_response seams ── */
static bool nm_overflow(void *ctx, const json_t *ar, int history_len) {
    script_ctx_t *c = ctx;
    (void)ar;
    bool r = in_bool(c, "overflow_verdict");
    json_t *e = ev("is_context_overflow");
    evn(e, "history_len", history_len);
    evb(e, "ret", r);
    rec(c, e);
    return r;
}
static bool nm_provider(void *ctx, const char *text) {
    script_ctx_t *c = ctx;
    bool r = false;
    const json_t *table = in_arr(c, "provider_verdicts");
    if (table) {
        for (size_t i = 0; i < table->c.count; i++) {
            json_t *row = table->c.items[i];
            if (row && row->type == JSON_ARRAY && row->c.count >= 2
                    && row->c.items[0]->type == JSON_STRING
                    && strcmp(row->c.items[0]->str_val, text ? text : "") == 0) {
                r = jtruth_h(row->c.items[1]);
                break;
            }
        }
    }
    json_t *e = ev("looks_like_provider_error");
    evs(e, "text", text);
    evb(e, "ret", r);
    rec(c, e);
    return r;
}
static bool nm_hidden(void *ctx, const json_t *ar) {
    script_ctx_t *c = ctx;
    (void)ar;
    bool r = in_bool(c, "hidden_verdict");
    json_t *e = ev("hidden_reasoning_incomplete");
    evb(e, "ret", r);
    rec(c, e);
    return r;
}
/* str.format-style {name} substitution from the kwargs object. */
static char *fmt_template(const char *tpl, const json_t *kwargs) {
    size_t cap = strlen(tpl) * 2 + 64;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    for (const char *p = tpl; *p; ) {
        if (*p == '{') {
            const char *end = strchr(p, '}');
            if (end) {
                size_t klen = (size_t)(end - p - 1);
                char key[128];
                if (klen < sizeof(key)) {
                    memcpy(key, p + 1, klen);
                    key[klen] = '\0';
                    const char *val = json_get_str(kwargs, key, "");
                    size_t vlen = strlen(val);
                    if (o + vlen + 1 >= cap) {
                        cap = (o + vlen + 1) * 2;
                        char *nb = realloc(out, cap);
                        if (!nb) { free(out); return NULL; }
                        out = nb;
                    }
                    memcpy(out + o, val, vlen);
                    o += vlen;
                    p = end + 1;
                    continue;
                }
            }
        }
        if (o + 2 >= cap) {
            cap *= 2;
            char *nb = realloc(out, cap);
            if (!nb) { free(out); return NULL; }
            out = nb;
        }
        out[o++] = *p++;
    }
    out[o] = '\0';
    return out;
}
static char *nm_t(void *ctx, const char *key, const json_t *kwargs) {
    script_ctx_t *c = ctx;
    json_t *e = ev("t");
    evs(e, "key", key);
    if (kwargs) {
        evj(e, "kwargs", kwargs);
    } else {
        json_set(e, "kwargs", json_object());   /* live t(**{}) records {} */
    }
    rec(c, e);
    const json_t *i18n = in_obj(c, "i18n");
    const char *tpl = json_get_str(i18n, key, "");
    return fmt_template(tpl, kwargs);
}
static void nm_log_warning(void *ctx, const char *msg) {
    script_ctx_t *c = ctx;
    json_t *e = ev("log");
    evs(e, "level", "warning");
    evs(e, "msg", msg);
    rec(c, e);
}

/* ── build_replay_entry seams ── */
static json_t *rp_metadata(void *ctx, const json_t *msg) {
    script_ctx_t *c = ctx;
    (void)msg;
    rec(c, ev("replay_metadata_fields"));
    return (json_t *)in_obj(c, "metadata_fields");
}
static void rp_identity(void *ctx, const json_t *src, json_t *dst) {
    script_ctx_t *c = ctx;
    json_t *e = ev("copy_identity_fields");
    evj(e, "src", src);
    rec(c, e);
    const json_t *idf = in_obj(c, "identity_fields");
    if (idf) {
        for (size_t i = 0; i < idf->c.count; i++)
            json_set(dst, idf->c.keys[i], json_copy(idf->c.items[i]));
    }
}

/* ── cached_agent_for_hygiene seams ── */
static void ca_acquire(void *ctx) { rec(ctx, ev("lock_acquire")); }
static void ca_release(void *ctx) { rec(ctx, ev("lock_release")); }
static json_t *ca_get(void *ctx, const char *key, bool *is_tuple, bool *raised) {
    script_ctx_t *c = ctx;
    *is_tuple = false;
    *raised = false;
    if (in_bool(c, "cache_get_raises")) {
        json_t *e = ev("cache_get");
        evs(e, "key", key);
        evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        return NULL;
    }
    const json_t *entry = json_obj_get(c->in, "entry_tuple");
    if (entry) {
        *is_tuple = true;
    } else {
        entry = json_obj_get(c->in, "entry_value");
    }
    json_t *e = ev("cache_get");
    evs(e, "key", key);
    evj(e, "ret", entry);
    evb(e, "is_tuple", *is_tuple);
    rec(c, e);
    return (json_t *)entry;
}

/* ── get_or_create_pool seam ── */
static json_t *pool_make(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *desc = in_obj(c, "make_pool_ret");
    json_t *e = ev("make_pool");
    evj(e, "ret", desc);
    rec(c, e);
    return (json_t *)desc;
}

/* ── stop_pool seam ── */
static bool sp_shutdown(void *ctx, bool wait, bool cancel_futures) {
    script_ctx_t *c = ctx;
    bool raised = cancel_futures && in_bool(c, "shutdown_typeerror");
    json_t *e = ev("shutdown");
    evb(e, "wait", wait);
    evb(e, "cancel_futures", cancel_futures);
    evs(e, "raised", raised ? "TypeError" : NULL);
    rec(c, e);
    return raised;
}

/* ── resolve_session_key_or_none seam ── */
static char *sk_derive(void *ctx, const json_t *source, bool *raised) {
    script_ctx_t *c = ctx;
    (void)source;
    *raised = false;
    if (in_bool(c, "derive_raises")) {
        json_t *e = ev("session_key_for_source");
        evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        return NULL;
    }
    const char *ret = in_str(c, "derive_ret", "");
    json_t *e = ev("session_key_for_source");
    evs(e, "ret", ret);
    rec(c, e);
    return strdup(ret);
}

/* ── cron_profile_gate seams ── */
static json_t *cg_liveness(void *ctx, const char *home, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = false;
    if (in_bool(c, "probe_raises")) {
        json_t *e = ev("resolve_liveness");
        evs(e, "home", home);
        evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        return NULL;
    }
    const json_t *liv = in_obj(c, "liveness");
    json_t *e = ev("resolve_liveness");
    evs(e, "home", home);
    evj(e, "ret", liv);
    rec(c, e);
    return (json_t *)liv;
}
static void cg_log_debug(void *ctx, const char *msg) {
    script_ctx_t *c = ctx;
    json_t *e = ev("log");
    evs(e, "level", "debug");
    evs(e, "msg", msg);
    rec(c, e);
}

/* ── cron_tick_profile_homes seams ── */
static json_t *ct_homes(void *ctx, const json_t *config) {
    script_ctx_t *c = ctx;
    (void)config;
    const json_t *homes = in_arr(c, "homes");
    json_t *e = ev("multiplex_homes");
    evj(e, "ret", homes);
    rec(c, e);
    return (json_t *)homes;
}
static char *ct_active(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *v = json_obj_get(c->in, "active");
    const char *s = (v && v->type == JSON_STRING) ? v->str_val : NULL;
    json_t *e = ev("active_profile");
    evs(e, "ret", s);
    rec(c, e);
    return s ? strdup(s) : NULL;
}
static char *ct_dir(void *ctx, const char *name, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = false;
    if (in_bool(c, "dir_raises")) {
        json_t *e = ev("profile_dir");
        evs(e, "name", name);
        evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        return NULL;
    }
    const json_t *v = json_obj_get(c->in, "profile_dir");
    const char *s = (v && v->type == JSON_STRING) ? v->str_val : NULL;
    json_t *e = ev("profile_dir");
    evs(e, "name", name);
    evs(e, "ret", s);
    rec(c, e);
    return s ? strdup(s) : NULL;
}

/* ── handoff_watch_scopes seams ── */
static json_t *hf_homes(void *ctx, const json_t *config, bool *raised) {
    script_ctx_t *c = ctx;
    (void)config;
    *raised = false;
    if (in_bool(c, "homes_raise")) {
        json_t *e = ev("multiplex_homes");
        evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        return NULL;
    }
    const json_t *homes = in_arr(c, "homes");
    json_t *e = ev("multiplex_homes");
    evj(e, "ret", homes);
    rec(c, e);
    return (json_t *)homes;
}

/* ── claim_host_gateway_role seams ── */
static char *cl_home(void *ctx) {
    script_ctx_t *c = ctx;
    const char *h = in_str(c, "hermes_home", "");
    json_t *e = ev("hermes_home");
    evs(e, "ret", h);
    rec(c, e);
    return strdup(h);
}
static json_t *cl_attach(void *ctx, const char *home, const json_t *owner) {
    script_ctx_t *c = ctx;
    c->attach_calls++;
    const json_t *dec = (c->attach_calls == 1) ? in_obj(c, "attach_decision")
                                               : in_obj(c, "attach_decision_recheck");
    json_t *e = ev("attach_decision");
    evs(e, "home", home);
    evj(e, "owner", owner);
    evj(e, "ret", dec);
    rec(c, e);
    return (json_t *)dec;
}
static bool cl_claim(void *ctx, const char *role, char **outcome, char **error) {
    script_ctx_t *c = ctx;
    *outcome = NULL;
    *error = NULL;
    if (in_bool(c, "claim_raises")) {
        json_t *e = ev("claim_host_lock");
        evs(e, "role", role);
        evb(e, "raised", true);
        rec(c, e);
        return true;
    }
    const char *oc = in_str(c, "claim_outcome", "held-by-other");
    const json_t *errv = json_obj_get(c->in, "claim_error");
    const char *err = (errv && errv->type == JSON_STRING) ? errv->str_val : NULL;
    json_t *e = ev("claim_host_lock");
    evs(e, "role", role);
    evs(e, "outcome", oc);
    evs(e, "error", err);
    rec(c, e);
    *outcome = strdup(oc);
    *error = err ? strdup(err) : NULL;
    return false;
}
static void cl_publish(void *ctx, const char *role, const json_t *profiles, const char *home) {
    script_ctx_t *c = ctx;
    json_t *e = ev("publish_record");
    evs(e, "role", role);
    evj(e, "profiles", profiles);
    evs(e, "home", home);
    rec(c, e);
}
static void cl_cleanup(void *ctx, const char *role) {
    script_ctx_t *c = ctx;
    json_t *e = ev("cleanup_on_exit");
    evs(e, "role", role);
    rec(c, e);
}
static json_t *cl_read(void *ctx, const char *role, bool include_stale, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = false;
    if (in_bool(c, "read_record_raises")) {
        json_t *e = ev("read_record");
        evs(e, "role", role);
        evb(e, "include_stale", include_stale);
        evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        return NULL;
    }
    const json_t *owner = in_obj(c, "read_record");
    json_t *e = ev("read_record");
    evs(e, "role", role);
    evb(e, "include_stale", include_stale);
    evj(e, "ret", owner);
    rec(c, e);
    return (json_t *)owner;
}
static char *cl_describe(void *ctx, const json_t *owner) {
    script_ctx_t *c = ctx;
    (void)owner;
    const char *d = in_str(c, "describe_ret", "");
    json_t *e = ev("describe");
    evs(e, "ret", d);
    rec(c, e);
    return strdup(d);
}
static bool cl_tenant(void *ctx, const char *owner_home, const char *home) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "tenant");
    json_t *e = ev("launched_by_other_tenant");
    evs(e, "owner_home", owner_home);
    evs(e, "home", home);
    evb(e, "ret", r);
    rec(c, e);
    return r;
}
static bool cl_standalone(void *ctx, const char *home) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "standalone_profile");
    json_t *e = ev("profile_is_standalone");
    evs(e, "home", home);
    evb(e, "ret", r);
    rec(c, e);
    return r;
}
static json_t *cl_host_gw(void *ctx, double wait_for_channel) {
    script_ctx_t *c = ctx;
    const json_t *owner = in_obj(c, "live_owner");
    json_t *e = ev("host_gateway");
    evn(e, "wait_for_channel", wait_for_channel);
    evj(e, "ret", owner);
    rec(c, e);
    return (json_t *)owner;
}
static bool cl_owner_standalone(void *ctx) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "owner_standalone");
    json_t *e = ev("owner_is_standalone");
    evb(e, "ret", r);
    rec(c, e);
    return r;
}
static char *cl_migrate(void *ctx) {
    script_ctx_t *c = ctx;
    const char *m = in_str(c, "migrate_command", "hermes gateway migrate --multiplex");
    json_t *e = ev("migrate_command");
    evs(e, "ret", m);
    rec(c, e);
    return strdup(m);
}
static void cl_refuse(void *ctx, const json_t *owner) {
    script_ctx_t *c = ctx;
    json_t *e = ev("refuse_second_host_gateway");
    evj(e, "owner", owner);
    rec(c, e);
}
static void cl_print(void *ctx, const char *text) {
    script_ctx_t *c = ctx;
    json_t *e = ev("print");
    evs(e, "text", text);
    rec(c, e);
}
static void cl_log(void *ctx, const char *level, const char *msg) {
    script_ctx_t *c = ctx;
    json_t *e = ev("log");
    evs(e, "level", level);
    evs(e, "msg", msg);
    rec(c, e);
}
static void cl_log_error(void *ctx, const char *msg) { cl_log(ctx, "error", msg); }

/* ── owner_is_standalone seams ── */
static json_t *ow_host_gw(void *ctx, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = false;
    if (in_bool(c, "host_gateway_raises")) {
        json_t *e = ev("host_gateway");
        evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        return NULL;
    }
    const json_t *owner = in_obj(c, "owner");
    json_t *e = ev("host_gateway");
    evj(e, "ret", owner);
    rec(c, e);
    return (json_t *)owner;
}
static char *ow_home(void *ctx, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = false;
    if (in_bool(c, "home_raises")) {
        json_t *e = ev("hermes_home");
        evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        return NULL;
    }
    const char *h = in_str(c, "hermes_home", "");
    json_t *e = ev("hermes_home");
    evs(e, "ret", h);
    rec(c, e);
    return strdup(h);
}
static char *ow_name(void *ctx, const char *home, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = false;
    if (in_bool(c, "name_raises")) {
        json_t *e = ev("profile_name_for_home");
        evs(e, "home", home);
        evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        return NULL;
    }
    const char *n = in_str(c, "profile_name", "");
    json_t *e = ev("profile_name_for_home");
    evs(e, "home", home);
    evs(e, "ret", n);
    rec(c, e);
    return strdup(n);
}
static json_t *ow_serve(void *ctx, const char *name, const json_t *owner, bool *raised) {
    script_ctx_t *c = ctx;
    (void)owner;
    *raised = false;
    if (in_bool(c, "serve_raises")) {
        json_t *e = ev("request_serve_profile");
        evs(e, "name", name);
        evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        return NULL;
    }
    const json_t *ans = in_obj(c, "answered");
    json_t *e = ev("request_serve_profile");
    evs(e, "name", name);
    evj(e, "ret", ans);
    rec(c, e);
    return (json_t *)ans;
}

/* ── refresh_host_gateway_record seams ── */
static bool rf_owns(void *ctx, const char *role, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = false;
    if (in_bool(c, "owns_raises")) {
        json_t *e = ev("owns_host_lock");
        evs(e, "role", role);
        evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        return false;
    }
    bool r = in_bool(c, "owns");
    json_t *e = ev("owns_host_lock");
    evs(e, "role", role);
    evb(e, "ret", r);
    rec(c, e);
    return r;
}
static json_t *rf_served(void *ctx, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = false;
    if (in_bool(c, "served_raises")) {
        json_t *e = ev("served_profile_names");
        evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        return NULL;
    }
    const json_t *names = in_arr(c, "served_names");
    json_t *e = ev("served_profile_names");
    evj(e, "ret", names);
    rec(c, e);
    return (json_t *)names;
}

/* ── dispatch ── */
static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, NULL, 0 };
    grun_trace_t trace = { NULL };
    ctx.trace = &trace;

    if (strcmp(func, "normalize_empty_agent_response") == 0) {
        grun_normalize_seams_t s = {
            .ctx = &ctx, .is_context_overflow = nm_overflow,
            .looks_like_provider_error = nm_provider,
            .hidden_reasoning_incomplete = nm_hidden,
            .t = nm_t, .log_warning = nm_log_warning,
        };
        char *r = grun_flow_normalize_empty_agent_response(
            in_obj(&ctx, "agent_result"), in_str(&ctx, "response", ""),
            (int)json_get_num(in, "history_len", 0), &s);
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "build_replay_entry") == 0) {
        grun_replay_seams_t s = {
            .ctx = &ctx, .replay_metadata_fields = rp_metadata,
            .copy_identity_fields = rp_identity,
        };
        json_t *r = grun_flow_build_replay_entry(
            in_str(&ctx, "role", ""), json_obj_get(in, "content"),
            in_obj(&ctx, "msg"), in_bool(&ctx, "preserve_timestamp"), &s);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "has_replayable_sidecar") == 0) {
        bool r = grun_flow_has_replayable_sidecar(
            json_obj_get(in, "role"), json_obj_get(in, "content"), in_obj(&ctx, "msg"));
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "command_origin_for_source") == 0) {
        json_t *r = grun_flow_command_origin_for_source(
            in_obj(&ctx, "source"), in_bool(&ctx, "platform_raises"));
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "cached_agent_for_hygiene") == 0) {
        grun_cache_seams_t s = {
            .ctx = &ctx, .lock_acquire = ca_acquire, .lock_release = ca_release,
            .cache_get = ca_get,
        };
        json_t *r = grun_flow_cached_agent_for_hygiene(
            in_obj(&ctx, "gateway"), in_str(&ctx, "session_key", ""), &s);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "get_or_create_pool") == 0) {
        grun_pool_seams_t s = { .ctx = &ctx, .make_pool = pool_make };
        json_t *state = json_copy(in_obj(&ctx, "state"));
        if (!state) state = json_object();
        bool raised = false;
        json_t *r = grun_flow_get_or_create_pool(
            state, in_str(&ctx, "attr", "_executor"), &s, &raised);
        if (raised) {
            json_t *e = ev("runtime_error");
            evs(e, "msg", "Gateway is shutting down; executor unavailable");
            rec(&ctx, e);
        }
        json_set(out, "ret", r ? json_copy(r) : json_null());
        json_set(out, "state", state);
    } else if (strcmp(func, "stop_pool") == 0) {
        grun_stop_pool_seams_t s = { .ctx = &ctx, .shutdown = sp_shutdown };
        json_t *r = grun_flow_stop_pool(json_obj_get(in, "executor"), &s);
        json_set(out, "ret", r ? r : json_array());
    } else if (strcmp(func, "resolve_session_key_or_none") == 0) {
        grun_session_key_seams_t s = { .ctx = &ctx, .session_key_for_source = sk_derive };
        const json_t *sk = json_obj_get(in, "session_key");
        char *r = grun_flow_resolve_session_key_or_none(
            json_obj_get(in, "source"),
            (sk && sk->type == JSON_STRING) ? sk->str_val : NULL, &s);
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "cron_profile_gate") == 0) {
        grun_cron_gate_seams_t s = {
            .ctx = &ctx, .resolve_liveness = cg_liveness,
            .self_pid = (long long)json_get_num(in, "self_pid", 0),
            .probe_error = in_str(&ctx, "probe_error", ""),
            .log_debug = cg_log_debug,
        };
        bool r = grun_flow_cron_profile_gate(
            in_str(&ctx, "name", ""), in_str(&ctx, "home", ""), &s);
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "cron_tick_profile_homes") == 0) {
        grun_cron_tick_seams_t s = {
            .ctx = &ctx, .multiplex_homes = ct_homes,
            .active_profile = ct_active, .profile_dir = ct_dir,
        };
        json_t *r = grun_flow_cron_tick_profile_homes(json_obj_get(in, "config"), &s);
        json_set(out, "ret", r ? r : json_array());
    } else if (strcmp(func, "handoff_watch_scopes") == 0) {
        grun_handoff_seams_t s = {
            .ctx = &ctx, .multiplex_homes = hf_homes, .log_debug = cg_log_debug,
        };
        json_t *r = grun_flow_handoff_watch_scopes(in_obj(&ctx, "runner"), &s);
        json_set(out, "ret", r ? r : json_array());
    } else if (strcmp(func, "claim_host_gateway_role") == 0) {
        grun_claim_seams_t s = {
            .ctx = &ctx, .hermes_home = cl_home, .attach_decision = cl_attach,
            .claim_host_lock = cl_claim, .publish_record = cl_publish,
            .cleanup_on_exit = cl_cleanup, .read_record = cl_read,
            .describe = cl_describe, .launched_by_other_tenant = cl_tenant,
            .profile_is_standalone = cl_standalone, .host_gateway = cl_host_gw,
            .owner_is_standalone = cl_owner_standalone, .migrate_command = cl_migrate,
            .refuse_second = cl_refuse, .print_line = cl_print, .log = cl_log,
        };
        int code = grun_flow_claim_host_gateway_role(in_bool(&ctx, "force"), &s);
        if (code == 75) {
            json_t *e = ev("system_exit");
            evn(e, "code", 75);
            rec(&ctx, e);
        }
        json_set(out, "ret", json_null());
        json_set(out, "exit", json_number(code));
    } else if (strcmp(func, "owner_is_standalone") == 0) {
        grun_owner_seams_t s = {
            .ctx = &ctx, .host_gateway = ow_host_gw,
            .self_pid = (long long)json_get_num(in, "self_pid", 0),
            .hermes_home = ow_home, .profile_name_for_home = ow_name,
            .request_serve_profile = ow_serve, .log_debug = cg_log_debug,
        };
        bool r = grun_flow_owner_is_standalone(&s);
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "refuse_second_host_gateway") == 0) {
        grun_refuse_seams_t s = {
            .ctx = &ctx, .describe = cl_describe, .migrate_command = cl_migrate,
            .log_error = cl_log_error, .print_line = cl_print,
        };
        int code = grun_flow_refuse_second_host_gateway(json_obj_get(in, "owner"), &s);
        if (code == 75) {
            json_t *e = ev("system_exit");
            evn(e, "code", 75);
            rec(&ctx, e);
        }
        json_set(out, "ret", json_null());
        json_set(out, "exit", json_number(code));
    } else if (strcmp(func, "refresh_host_gateway_record") == 0) {
        grun_refresh_seams_t s = {
            .ctx = &ctx, .owns_host_lock = rf_owns, .hermes_home = ow_home,
            .served_profile_names = rf_served, .profile_name_for_home = ow_name,
            .publish_record = cl_publish, .log_debug = cg_log_debug,
        };
        grun_flow_refresh_host_gateway_record(in_obj(&ctx, "runner"), &s);
        json_set(out, "ret", json_null());
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "trace", trace.events ? trace.events : json_array());
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    char line[1 << 20];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case line: %s\n", err ? err : "not an object");
            free(err);
            json_free(c);
            continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        char *func_s = json_escape_string(func);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    if (f != stdin) fclose(f);
    return 0;
}
