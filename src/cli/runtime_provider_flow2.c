/* runtime_provider_flow2.c — FLOW port of the UNCLAIMED remainder of
 * hermes_cli/runtime_provider.py. Claimed siblings (flow1 port_runtime_provider_flow.c,
 * runtime_provider.c, provider_registry.c, timeouts.c, util_helpers.c) are SEAMED here;
 * only defs with no strict PoP claim are ported under rp2_* with a PoP annotation.
 *
 * Representation contract (shared with tests/oracle_runtime_provider_flow2.py):
 *  - every external collaborator is the universal seam rp2_call(name, args); args carry
 *    None-valued keys DROPPED on both sides ("" kept). Scripts: absent -> None,
 *    {"$seq":[...]} per-call queue, {"__raise__":cls,"msg":...} raises (trace ret is the
 *    whole marker), any other value reused. Exhaustion -> RuntimeError.
 *  - uncaught exception -> {"__raise__":cls}; the full builtin exception hierarchy is
 *    modeled so `except Exception` matches every builtin except the BaseException trio.
 *  - None -> JSON null; integral numbers stay numbers; dict/list project verbatim.
 *  - fixture domain: ASCII URLs, dict configs, str/num/bool/null args; object doubles
 *    (pool entries, provider configs, profiles) are attribute dicts read via rp2_get.
 *    Container-str() and non-ASCII repr stringification are out of scope.
 *
 * libjson-only. Self-contained: the harness #includes "cli/runtime_provider_flow2.c".
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

typedef struct rp2_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted constants/flags */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} rp2_ctx;

/* ── predicates / accessors ─────────────────────────────────────────────────── */
static bool rp2_eq(const char *a, const char *b) { return strcmp(a ? a : "", b ? b : "") == 0; }
static bool rp2_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool rp2_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool rp2_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static json_t *rp2_get(const json_t *o, const char *k) { return rp2_obj(o) ? json_obj_get(o, k) : NULL; }
/* dict.get: present-null keeps the null; absent -> def (borrowed). */
static json_t *rp2_copy_null(const json_t *v) { return v ? json_copy(v) : json_null(); }
static bool rp2_truth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
/* forward declarations for helpers used before definition */
static json_t *rp2_pair(json_t *a, json_t *b);
static json_t *rp2_pair3(json_t *a, json_t *b, json_t *c);
static bool rp2_host_match(rp2_ctx *c, const char *url, const char *host);

/* str(v) for the narrow fixture domain */

/* ── string helpers (caller frees) ─────────────────────────────────────────── */

/* ── full builtin exception hierarchy ──────────────────────────────────────── */
static bool rp2_exc_pending(const rp2_ctx *c) { return c && c->exc != NULL; }
static const char *rp2_exc_class(const rp2_ctx *c) { return json_get_str(c->exc, "__raise__", ""); }

/* ── universal seam ────────────────────────────────────────────────────────── */
static json_t *rp2_script_result(rp2_ctx *c, const json_t *s) {
    if (!s || s->type == JSON_NULL) return NULL;
    if (s->type == JSON_OBJECT) {
        json_t *r = json_obj_get(s, "__raise__");
        if (r && r->type == JSON_STRING) { if (c->exc) json_free(c->exc); c->exc = json_copy(s); return NULL; }
    }
    return json_copy(s);
}
/* args are BORROWED: rp2_call deep-copies them into the trace event; the caller
 * keeps ownership (and typically frees them). Result is NEW or NULL. */
static json_t *rp2_call(rp2_ctx *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *ret = NULL;
    json_t *script = c->io ? json_obj_get(c->io, name) : NULL;
    const json_t *chosen = script;
    if (script && script->type == JSON_OBJECT) {
        json_t *seq = json_obj_get(script, "$seq");
        if (seq && seq->type == JSON_ARRAY) {
            double cur = json_get_num(c->counters, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= seq->c.count) {
                if (c->exc) json_free(c->exc);
                c->exc = json_object();
                json_set(c->exc, "__raise__", json_string("RuntimeError"));
                char msg[256]; snprintf(msg, sizeof msg, "script exhausted: %s", name);
                json_set(c->exc, "msg", json_string(msg)); chosen = c->exc;
            } else { json_set(c->counters, name, json_number(cur+1)); chosen = seq->c.items[idx]; }
        }
    }
    if (chosen != c->exc) ret = rp2_script_result(c, chosen);
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", json_copy(args));
    if (c->exc && chosen == c->exc) json_set(ev, "ret", json_copy(c->exc));
    else if (c->exc && chosen && chosen->type == JSON_OBJECT && json_obj_get(chosen, "__raise__"))
        json_set(ev, "ret", json_copy(c->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(c->trace, ev);
    return ret;
}
static void rp2_arg(json_t *a, const char *k, const json_t *v) { if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v)); }
static void rp2_arg_s(json_t *a, const char *k, const char *v) { if (v) json_set(a, k, json_string(v)); }
static void rp2_arg_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
/* build a runtime dict; extra keys copied from `extra` object. */
/* seam helper: one string arg, one result */

/* =========================== PORTED FUNCTIONS ============================== */
/* Every collaborator is a seam. Ported siblings are ALSO seamed (each def is driven
 * standalone by the oracle), which keeps each C function self-contained. */

/* PoP: rp2_load_config @ hermes_cli/runtime_provider.py:load_config */
static json_t *rp2_load_config(rp2_ctx *c, json_t *in) {
    (void)in;
    return rp2_call(c, "load_config", json_object());
}
/* PoP: rp2_get_compatible_custom_providers @ hermes_cli/runtime_provider.py:get_compatible_custom_providers */
static json_t *rp2_get_compatible_custom_providers(rp2_ctx *c, json_t *in) {
    json_t *a = json_object(); rp2_arg(a, "config", rp2_get(in, "config"));
    return rp2_call(c, "get_compatible_custom_providers", a);
}
/* PoP: rp2_normalize_extra_headers @ hermes_cli/runtime_provider.py:normalize_extra_headers */
static json_t *rp2_normalize_extra_headers(rp2_ctx *c, json_t *in) {
    json_t *a = json_object(); rp2_arg(a, "value", rp2_get(in, "value"));
    return rp2_call(c, "normalize_extra_headers", a);
}
/* PoP: rp2__effective_model @ hermes_cli/runtime_provider.py:_effective_model */
static json_t *rp2__effective_model(rp2_ctx *c, json_t *in) {
    (void)c;
    json_t *t = rp2_get(in, "target_model");
    if (rp2_truth(t)) return json_copy(t);
    json_t *d = rp2_get(rp2_get(in, "model_cfg"), "default");
    if (rp2_truth(d)) return json_copy(d);
    return json_string("");
}
/* PoP: rp2__pool_entry_api_key @ hermes_cli/runtime_provider.py:_pool_entry_api_key */
static json_t *rp2__pool_entry_api_key(rp2_ctx *c, json_t *in) {
    (void)c;
    json_t *e = rp2_get(in, "entry");
    json_t *v = rp2_get(e, "runtime_api_key");
    if (rp2_truth(v)) return json_copy(v);
    v = rp2_get(e, "access_token");
    return v ? json_copy(v) : json_string("");
}
/* PoP: rp2__pool_entry_base_url @ hermes_cli/runtime_provider.py:_pool_entry_base_url */
static json_t *rp2__pool_entry_base_url(rp2_ctx *c, json_t *in) {
    (void)c;
    json_t *e = rp2_get(in, "entry");
    json_t *v = rp2_get(e, "runtime_base_url");
    if (rp2_truth(v)) return json_copy(v);
    v = rp2_get(e, "base_url");
    if (rp2_truth(v)) return json_copy(v);
    return json_string("");
}
/* PoP: rp2__nous_min_key_ttl @ hermes_cli/runtime_provider.py:_nous_min_key_ttl */
static json_t *rp2__nous_min_key_ttl(rp2_ctx *c, json_t *in) {
    (void)in;
    json_t *a = json_object();
    rp2_arg_s(a, "key", "HERMES_NOUS_MIN_KEY_TTL_SECONDS"); rp2_arg_n(a, "default", 1800);
    json_t *r = rp2_call(c, "env_int", a);
    if (rp2_exc_pending(c) || !rp2_num(r)) { json_free(r); return json_number(1800); }
    double v = r->num_val; json_free(r);
    return json_number(v > 60 ? v : 60);
}
/* PoP: rp2__finalize_base_url @ hermes_cli/runtime_provider.py:_finalize_base_url */
static json_t *rp2__finalize_base_url(rp2_ctx *c, json_t *in) {
    json_t *base = rp2_copy_null(rp2_get(in, "base_url"));
    json_t *a = json_object(); rp2_arg(a, "provider", rp2_get(in, "provider"));
    json_t *fam = rp2_call(c, "opencode_provider_family", a);
    bool has = !rp2_exc_pending(c) && !rp2_none(fam); json_free(fam);
    if (has) {
        json_t *b = json_object();
        rp2_arg(b, "provider", rp2_get(in, "provider")); rp2_arg(b, "api_mode", rp2_get(in, "api_mode"));
        rp2_arg(b, "base_url", base);
        json_t *nb = rp2_call(c, "normalize_opencode_base_url", b);
        if (rp2_exc_pending(c)) return NULL;
        json_free(base); base = nb;
    }
    const char *prov = json_get_str(in, "provider", "");
    if (rp2_eq(prov, "lmstudio") || rp2_eq(prov, "actual")) {
        json_t *b = json_object(); rp2_arg(b, "base_url", base);
        json_t *nb = rp2_call(c, rp2_eq(prov,"lmstudio") ? "normalize_lmstudio_base_url" : "normalize_actual_base_url", b);
        if (rp2_exc_pending(c)) { json_free(base); return NULL; }
        json_free(base); base = nb;
    }
    return base ? base : json_string("");
}
/* helper for the two-element tuple results above */
/* host match via seam (both sides use the scripted answer) */
/* PoP: rp2__actual_local_key @ hermes_cli/runtime_provider.py:_actual_local_key */
static json_t *rp2__actual_local_key(rp2_ctx *c, json_t *in) {
    json_t *key=rp2_get(in,"api_key");
    const char *prov=json_get_str(in,"provider","");
    if (rp2_eq(prov,"actual") && !rp2_truth(key)) {
        json_t *a=json_object(); rp2_arg(a,"base_url",rp2_get(in,"base_url"));
        json_t *local=rp2_call(c,"is_actual_local_base_url",a);
        if (rp2_exc_pending(c)) { json_free(local); return NULL; }
        bool yes=rp2_truth(local); json_free(local);
        if (yes) return json_string("dummy-actual-local-api-key");
    }
    return rp2_copy_null(key);
}
/* PoP: rp2__actual_url @ hermes_cli/runtime_provider.py:_actual_url */
static json_t *rp2__actual_url(rp2_ctx *c, json_t *in) {
    const char *prov=json_get_str(in,"provider","");
    if (rp2_eq(prov,"actual")) {
        json_t *a=json_object(); rp2_arg(a,"base_url",rp2_get(in,"base_url"));
        return rp2_call(c,"normalize_actual_base_url",a);
    }
    return rp2_copy_null(rp2_get(in,"base_url"));
}
static json_t *rp2__tag(rp2_ctx *c, json_t *in) {
    (void)c;
    json_t *rt=rp2_get(in,"runtime");
    if (rp2_none(rt)) return json_null();
    json_t *r=json_copy(rt);
    json_set(r,"requested_provider",rp2_copy_null(rp2_get(in,"requested_provider")));
    return r;
}
/* PoP: rp2__named_custom_rung @ hermes_cli/runtime_provider.py:_named_custom_rung */
static json_t *rp2__named_custom_rung(rp2_ctx *c, json_t *in) {
    json_t *a=json_object(); rp2_arg_s(a,"requested_provider",json_get_str(in,"requested_provider",""));
    rp2_arg(a,"explicit_api_key",rp2_get(in,"explicit_api_key")); rp2_arg(a,"explicit_base_url",rp2_get(in,"explicit_base_url"));
    rp2_arg(a,"target_model",rp2_get(in,"target_model"));
    json_t *raw=rp2_call(c,"resolve_named_custom_runtime",a);
    if (rp2_exc_pending(c)) { json_free(raw); return NULL; }
    json_t *ta=json_object(); json_set(ta,"runtime",raw); rp2_arg_s(ta,"requested_provider",json_get_str(in,"requested_provider",""));
    json_t *tagged=rp2__tag(c,ta); json_free(ta);
    if (rp2_exc_pending(c)) return NULL;
    if (rp2_eq(json_get_str(tagged,"provider",""),"custom")) {
        json_t *mc=rp2_call(c,"get_model_config",json_object());
        if (rp2_exc_pending(c)) { json_free(mc); json_free(tagged); return NULL; }
        json_t *ma=json_object(); rp2_arg_s(ma,"provider","custom");
        const char *am=json_get_str(tagged,"api_mode","");
        rp2_arg_s(ma,"api_mode",am[0]?am:"chat_completions"); json_set(ma,"model_cfg",mc);
        rp2_arg_s(ma,"requested_provider",json_get_str(in,"requested_provider",""));
        json_t *mode=rp2_call(c,"maybe_apply_codex_app_server_runtime",ma); json_free(ma);
        if (rp2_exc_pending(c)) { json_free(mode); json_free(tagged); return NULL; }
        json_set(tagged,"api_mode",mode);
    }
    return tagged;
}
/* PoP: rp2__openrouter_fallback @ hermes_cli/runtime_provider.py:_openrouter_fallback */
static json_t *rp2__openrouter_fallback(rp2_ctx *c, json_t *in) {
    json_t *a=json_object(); rp2_arg_s(a,"requested_provider",json_get_str(in,"requested_provider",""));
    rp2_arg(a,"explicit_api_key",rp2_get(in,"explicit_api_key")); rp2_arg(a,"explicit_base_url",rp2_get(in,"explicit_base_url"));
    json_t *raw=rp2_call(c,"resolve_openrouter_runtime",a);
    if (rp2_exc_pending(c)) { json_free(raw); return NULL; }
    json_t *ta=json_object(); json_set(ta,"runtime",raw); rp2_arg_s(ta,"requested_provider",json_get_str(in,"requested_provider",""));
    json_t *r=rp2__tag(c,ta); json_free(ta); return r;
}
/* PoP: rp2__owned_url_credential @ hermes_cli/runtime_provider.py:_owned_url_credential */
static json_t *rp2__owned_url_credential(rp2_ctx *c, json_t *in) {
    json_t *a=json_object();
    rp2_arg(a,"base_url",rp2_get(in,"base_url")); rp2_arg(a,"model_cfg",rp2_get(in,"model_cfg"));
    rp2_arg(a,"provider",rp2_get(in,"provider"));
    return rp2_call(c,"owned_config_url_credential",a);
}

/* Fixture-driven dispatch is in tests/t_port_runtime_provider_flow2.c. */
