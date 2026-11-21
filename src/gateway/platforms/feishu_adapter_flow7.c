/*
 * feishu_adapter_flow7.c — Feishu/Lark adapter proxy / onboarding / registration
 * / reaction / batch-helper FLOW cores of plugins/platforms/feishu/adapter.py
 * (hermes-agent).
 *
 * SEVENTH remainder port beyond feishu_adapter_flow.c (message normalization),
 * feishu_adapter_flow2.c (inbound-event / card-action), feishu_adapter_flow3.c
 * (inbound admission / mention gating), feishu_adapter_flow4.c (SDK-build /
 * dependency gates), feishu_adapter_flow5.c (outbound request/response) and
 * feishu_adapter_flow6.c (media/extension/response/header resolution).
 *
 * This file ports 18 GENUINELY UNCLAIMED defs (no def claimed by flows 1-6 is
 * re-ported), a coherent "proxy + onboarding + reactions + batch" slice:
 *   proxy helpers:              __getattr__, __repr__
 *   pure URL/bot resolvers:     _accounts_base_url, _onboard_open_base_url,
 *                               _parse_bot_response, _reactions_enabled,
 *                               _text_batch_is_compatible
 *   onboarding/registration:    _begin_registration, _init_registration,
 *                               _apply_yaml_config, _build_onboard_client,
 *                               probe_bot, _probe_bot_sdk
 *   reactions:                  _add_reaction, _remove_reaction
 *   batch helpers:              _media_batch_key, _media_batch_is_compatible,
 *                               _get_cached_sender_name
 *
 * Ported siblings that are seamed when composed (real only when themselves the
 * case target): _accounts_base_url, _build_onboard_client, _parse_bot_response,
 * _text_batch_is_compatible, _probe_bot_sdk. All other collaborators (_target,
 * getattr_target, repr_target, _get_scoped_secret, _post_registration,
 * _apply_yaml_bridge, _build_lark_client, _sdk_domain, _load_lark_oapi,
 * _probe_bot_http, _reaction_call, getattr_reaction, _text_batch_key, cache_get,
 * time_now, cache_pop, _tenant_get_request, client_request, getattr_raw,
 * getattr_content, json_loads, logger_debug) are universal seams.
 *
 * Universal seam: every seamed call enters through ONE callback
 * fa7_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting the
 * identical event shape {"name","args","ret"} from the SAME scripts. args is
 * ADOPTED into the trace event; the return is a NEW owned json_t (NULL == Python
 * None; {"__raise__":cls,"message":str(exc)} == a raised exception). A
 * missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * FULL BUILTIN EXCEPTION HIERARCHY / BASEEXCEPTION PASSTHROUGH: fa7_is_base_exc
 * names the family that bypasses `except Exception`. _probe_bot_sdk wraps its
 * body in try/except Exception: Exception-side scripted raises are swallowed
 * (with a logger_debug seam) while BaseException-side raises propagate verbatim.
 * Its handler's own raise propagates. All other defs have no try/except and
 * propagate every scripted raise verbatim. The oracle's EXC_CLASSES table mirrors
 * the full builtin hierarchy so make_exc() materializes the exact class.
 *
 * Representation contract (shared with tests/oracle_feishu_adapter_flow7.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__":cls,"message":str(exc)}; a scripted raise is
 *   {"__script__":true,"raise":{"type":cls,"message":msg}}.
 * - Carriers (proxy target, reaction data, event, cache, client, resp) are JSON
 *   objects/values; attribute reads are modeled as universal seams or direct
 *   JSON field reads. The oracle materializes equivalent attribute doubles.
 * - Fixture domain: ASCII strings, scalar/JSON-dict/list values, POSIX paths.
 *   Out.state is {} for every func.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and fa7_-prefixed.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct fa7_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} fa7_ctx_t;

/* ── basic helpers ── */
static bool fa7_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool fa7_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool fa7_is_base_exc(const char *kind) {
    return kind && (
        strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0);
}
static bool fa7_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *fa7_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static const char *fa7_str_or_empty(const json_t *v) {
    const char *s = fa7_str(v);
    return s ? s : "";
}
static json_t *fa7_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *fa7_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    (void)fa7_is_base_exc(type);
    return fa7_err(type, msg);
}
/* The one universal seam. args is ADOPTED into the trace event; the return is a
 * fresh owned node (scripted value copy or raise descriptor). */
static json_t *fa7_call(fa7_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = fa7_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = fa7_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = fa7_make_exc(raise);
            } else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}
static json_t *fa7_args(void) { return json_object(); }
static void fa7_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void fa7_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* Python str(x) for the scalar fixture domain. Returns either v->str_val (borrowed)
 * or a pointer into buf. */
static const char *fa7_pystr(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return buf; }
    if (v->type == JSON_STRING) return v->str_val;
    if (v->type == JSON_BOOL) { snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return buf; }
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, cap, "%.0f", v->num_val);
        else
            snprintf(buf, cap, "%g", v->num_val);
        return buf;
    }
    snprintf(buf, cap, "None");
    return buf;
}

/* Python repr of a list of scalars (for interpolated error messages). */
static char *fa7_pyrepr_list(const json_t *arr) {
    size_t cap = 64, n = 0;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    out[n++] = '['; out[n] = '\0';
    if (arr && arr->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(arr); i++) {
            if (i) { if (n + 2 >= cap) { cap *= 2; out = (char *)realloc(out, cap); if (!out) return NULL; }
                     out[n++] = ','; out[n++] = ' '; out[n] = '\0'; }
            const json_t *it = json_get(arr, i);
            char tmp[256];
            const char *s;
            char need[300];
            if (it && it->type == JSON_STRING) {
                snprintf(need, sizeof need, "'%s'", it->str_val); s = need;
            } else {
                s = fa7_pystr(it, tmp, sizeof tmp);
            }
            size_t sl = strlen(s);
            while (n + sl + 2 >= cap) { cap *= 2; out = (char *)realloc(out, cap); if (!out) return NULL; }
            memcpy(out + n, s, sl); n += sl; out[n] = '\0';
        }
    }
    if (n + 1 >= cap) { cap += 2; out = (char *)realloc(out, cap); if (!out) return NULL; }
    out[n++] = ']'; out[n] = '\0';
    return out;
}

/* Deep equality of JSON values (Python == for the scalar/container fixture domain). */
static bool fa7_json_equal(const json_t *a, const json_t *b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->type != b->type) {
        /* Python: 0 == False, 1 == True, 0 == 0.0 */
        if (a->type == JSON_NUMBER && b->type == JSON_BOOL) return a->num_val == (b->bool_val ? 1.0 : 0.0);
        if (a->type == JSON_BOOL && b->type == JSON_NUMBER) return b->num_val == (a->bool_val ? 1.0 : 0.0);
        return false;
    }
    switch (a->type) {
        case JSON_NULL: return true;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val, b->str_val) == 0;
        case JSON_ARRAY:
            if (json_len(a) != json_len(b)) return false;
            for (size_t i = 0; i < json_len(a); i++)
                if (!fa7_json_equal(json_get(a, i), json_get(b, i))) return false;
            return true;
        case JSON_OBJECT:
            if (json_object_size(a) != json_object_size(b)) return false;
            for (size_t i = 0; i < json_object_size(a); i++) {
                const char *k = json_object_get_key_at(a, i);
                if (!fa7_json_equal(json_object_get_at(a, i), json_obj_get(b, k))) return false;
            }
            return true;
    }
    return false;
}

/* Python str.strip() ASCII whitespace + lower, for _reactions_enabled. */
static char *fa7_trim_lower(const char *s) {
    static const char *ws = " \t\n\r\v\f";
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && strchr(ws, s[a]) != NULL) a++;
    while (b > a && strchr(ws, s[b - 1]) != NULL) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < b - a; i++) {
        char c = s[a + i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[b - a] = '\0';
    return out;
}

/* ── seeded module constants ── */
static const char *fa7_ACCOUNTS_URLS[][2] = {
    {"feishu", "https://accounts.feishu.cn"},
    {"lark", "https://accounts.larksuite.com"},
};
static const char *fa7_OPEN_URLS[][2] = {
    {"feishu", "https://open.feishu.cn"},
    {"lark", "https://open.larksuite.com"},
};
static const char *fa7_url_lookup(const char *domain, const char *const tbl[][2], size_t n,
                                   const char *fallback) {
    for (size_t i = 0; i < n; i++) if (strcmp(domain, tbl[i][0]) == 0) return tbl[i][1];
    return fallback;
}

/* ══ PoP functions ══ */

/* PoP: fa7__accounts_base_url @ plugins/platforms/feishu/adapter.py:_accounts_base_url */
static json_t *fa7__accounts_base_url(fa7_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *d = fa7_str_or_empty(json_obj_get(args, "domain"));
    return json_string(fa7_url_lookup(d, fa7_ACCOUNTS_URLS, 2, fa7_ACCOUNTS_URLS[0][1]));
}

/* PoP: fa7__onboard_open_base_url @ plugins/platforms/feishu/adapter.py:_onboard_open_base_url */
static json_t *fa7__onboard_open_base_url(fa7_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *d = fa7_str_or_empty(json_obj_get(args, "domain"));
    return json_string(fa7_url_lookup(d, fa7_OPEN_URLS, 2, fa7_OPEN_URLS[0][1]));
}

/* PoP: fa7__parse_bot_response @ plugins/platforms/feishu/adapter.py:_parse_bot_response */
static json_t *fa7__parse_bot_response(fa7_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *data = json_obj_get(args, "data");
    const json_t *code = data ? json_obj_get(data, "code") : NULL;
    bool code_ok = false;
    if (code) {
        if (code->type == JSON_NUMBER && code->num_val == 0.0) code_ok = true;
        else if (code->type == JSON_BOOL && !code->bool_val) code_ok = true;
    }
    if (!code_ok) return json_null();
    const json_t *bot = data ? json_obj_get(data, "bot") : NULL;
    if (!fa7_truthy(bot)) {
        const json_t *inner = data ? json_obj_get(data, "data") : NULL;
        const json_t *cand = (inner && inner->type == JSON_OBJECT) ? json_obj_get(inner, "bot") : NULL;
        if (fa7_truthy(cand)) bot = cand;
        else bot = NULL;
    }
    json_t *out = json_object();
    const json_t *nm = bot ? json_obj_get(bot, "app_name") : NULL;
    if (!fa7_truthy(nm) && bot) nm = json_obj_get(bot, "bot_name");
    json_set(out, "bot_name", fa7_truthy(nm) ? json_copy(nm) : json_null());
    const json_t *oid = bot ? json_obj_get(bot, "open_id") : NULL;
    json_set(out, "bot_open_id", fa7_truthy(oid) ? json_copy(oid) : json_null());
    return out;
}

/* PoP: fa7_reactions_enabled @ plugins/platforms/feishu/adapter.py:FeishuAdapter._reactions_enabled */
static json_t *fa7_reactions_enabled(fa7_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = fa7_args();
    fa7_set_s(a, "key", "FEISHU_REACTIONS");
    json_t *v = fa7_call(ctx, "_get_scoped_secret", a);
    if (fa7_is_raise(v)) return v;
    char vb[256];
    char *low = fa7_trim_lower(fa7_pystr(v, vb, sizeof vb));
    json_free(v);
    bool off = low && (strcmp(low, "false") == 0 || strcmp(low, "0") == 0 || strcmp(low, "no") == 0);
    free(low);
    return json_bool(!off);
}

/* PoP: fa7_text_batch_is_compatible @ plugins/platforms/feishu/adapter.py:FeishuAdapter._text_batch_is_compatible */
static json_t *fa7_text_batch_is_compatible(fa7_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *e = json_obj_get(args, "existing");
    const json_t *in = json_obj_get(args, "incoming");
    if (!e || !in) return json_bool(false);
    if (!fa7_json_equal(json_obj_get(e, "reply_to_message_id"), json_obj_get(in, "reply_to_message_id")))
        return json_bool(false);
    if (!fa7_json_equal(json_obj_get(e, "reply_to_text"), json_obj_get(in, "reply_to_text")))
        return json_bool(false);
    const json_t *es = json_obj_get(e, "source"), *ins = json_obj_get(in, "source");
    if (!fa7_json_equal(es ? json_obj_get(es, "thread_id") : NULL,
                        ins ? json_obj_get(ins, "thread_id") : NULL))
        return json_bool(false);
    return json_bool(true);
}

/* PoP: fa7__begin_registration @ plugins/platforms/feishu/adapter.py:_begin_registration */
static json_t *fa7__begin_registration(fa7_ctx_t *ctx, const json_t *args) {
    const char *domain = fa7_str_or_empty(json_obj_get(args, "domain"));
    json_t *ba = fa7_args();
    fa7_set_s(ba, "domain", domain);
    json_t *base = fa7_call(ctx, "_accounts_base_url", ba);
    if (fa7_is_raise(base)) return base;
    char bb[512];
    const char *base_url = fa7_pystr(base, bb, sizeof bb);
    json_t *pa = fa7_args();
    fa7_set_s(pa, "base_url", base_url);
    json_t *body = json_object();
    fa7_set_s(body, "action", "begin");
    fa7_set_s(body, "archetype", "PersonalAgent");
    fa7_set_s(body, "auth_method", "client_secret");
    fa7_set_s(body, "request_user_info", "open_id");
    json_set(pa, "body", body);
    json_free(base);
    json_t *res = fa7_call(ctx, "_post_registration", pa);
    if (fa7_is_raise(res)) return res;
    const json_t *dc = json_obj_get(res, "device_code");
    if (!fa7_truthy(dc)) {
        json_free(res);
        return fa7_err("RuntimeError", "Feishu / Lark registration did not return a device_code");
    }
    const json_t *qv = json_obj_get(res, "verification_uri_complete");
    const char *q = fa7_str(qv) ? fa7_str(qv) : "";
    char qr[1024];
    snprintf(qr, sizeof qr, "%s%sfrom=hermes&tp=hermes", q, (strchr(q, '?') ? "&" : "?"));
    json_t *out = json_object();
    fa7_set_copy(out, "device_code", dc);
    fa7_set_s(out, "qr_url", qr);
    const json_t *uc = json_obj_get(res, "user_code");
    fa7_set_s(out, "user_code", fa7_truthy(uc) ? fa7_str_or_empty(uc) : "");
    const json_t *iv = json_obj_get(res, "interval");
    json_set(out, "interval", fa7_truthy(iv) ? json_copy(iv) : json_number(5));
    const json_t *ev = json_obj_get(res, "expire_in");
    json_set(out, "expire_in", fa7_truthy(ev) ? json_copy(ev) : json_number(600));
    json_free(res);
    return out;
}

/* PoP: fa7__init_registration @ plugins/platforms/feishu/adapter.py:_init_registration */
static json_t *fa7__init_registration(fa7_ctx_t *ctx, const json_t *args) {
    const char *domain = fa7_str_or_empty(json_obj_get(args, "domain"));
    json_t *ba = fa7_args();
    fa7_set_s(ba, "domain", domain);
    json_t *base = fa7_call(ctx, "_accounts_base_url", ba);
    if (fa7_is_raise(base)) return base;
    char bb[512];
    const char *base_url = fa7_pystr(base, bb, sizeof bb);
    json_t *pa = fa7_args();
    fa7_set_s(pa, "base_url", base_url);
    json_t *body = json_object();
    fa7_set_s(body, "action", "init");
    json_set(pa, "body", body);
    json_free(base);
    json_t *res = fa7_call(ctx, "_post_registration", pa);
    if (fa7_is_raise(res)) return res;
    const json_t *methods = json_obj_get(res, "supported_auth_methods");
    if (!fa7_truthy(methods)) methods = NULL;
    bool supported = false;
    if (methods && methods->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(methods); i++) {
            const char *m = fa7_str(json_get(methods, i));
            if (m && strcmp(m, "client_secret") == 0) { supported = true; break; }
        }
    }
    json_free(res);
    if (!supported) {
        char *repr = fa7_pyrepr_list(methods);
        char msg[1024];
        snprintf(msg, sizeof msg,
                 "Feishu / Lark registration environment does not support client_secret auth. Supported: %s",
                 repr ? repr : "[]");
        free(repr);
        return fa7_err("RuntimeError", msg);
    }
    return json_null();
}

/* PoP: fa7__apply_yaml_config @ plugins/platforms/feishu/adapter.py:_apply_yaml_config */
static json_t *fa7__apply_yaml_config(fa7_ctx_t *ctx, const json_t *args) {
    json_t *a = fa7_args();
    fa7_set_copy(a, "feishu_cfg", json_obj_get(args, "feishu_cfg"));
    json_t *seeded = fa7_call(ctx, "_apply_yaml_bridge", a);
    if (fa7_is_raise(seeded)) return seeded;
    if (!fa7_truthy(seeded)) { json_free(seeded); return json_null(); }
    const json_t *ab = json_obj_get(seeded, "allow_bots");
    if (!ab) { json_free(seeded); return fa7_err("KeyError", "'allow_bots'"); }
    char vb[256];
    char *low = fa7_trim_lower(fa7_pystr(ab, vb, sizeof vb));
    json_free(seeded);
    json_t *out = json_object();
    fa7_set_s(out, "allow_bots", low ? low : "");
    free(low);
    return out;
}

/* PoP: fa7__build_onboard_client @ plugins/platforms/feishu/adapter.py:_build_onboard_client */
static json_t *fa7__build_onboard_client(fa7_ctx_t *ctx, const json_t *args) {
    json_t *da = fa7_args();
    fa7_set_copy(da, "domain_name", json_obj_get(args, "domain"));
    json_t *sd = fa7_call(ctx, "_sdk_domain", da);
    if (fa7_is_raise(sd)) return sd;
    json_t *ba = fa7_args();
    fa7_set_copy(ba, "app_id", json_obj_get(args, "app_id"));
    fa7_set_copy(ba, "app_secret", json_obj_get(args, "app_secret"));
    json_set(ba, "sdk_domain", sd); /* adopted */
    return fa7_call(ctx, "_build_lark_client", ba);
}

/* PoP: fa7_probe_bot @ plugins/platforms/feishu/adapter.py:probe_bot */
static json_t *fa7_probe_bot(fa7_ctx_t *ctx, const json_t *args) {
    json_t *la = fa7_call(ctx, "_load_lark_oapi", fa7_args());
    if (fa7_is_raise(la)) return la;
    bool use_sdk = fa7_truthy(la);
    json_free(la);
    json_t *a = fa7_args();
    fa7_set_copy(a, "app_id", json_obj_get(args, "app_id"));
    fa7_set_copy(a, "app_secret", json_obj_get(args, "app_secret"));
    fa7_set_copy(a, "domain", json_obj_get(args, "domain"));
    if (use_sdk) {
        json_t *out = fa7_call(ctx, "_probe_bot_sdk", a);
        return out;
    }
    return fa7_call(ctx, "_probe_bot_http", a);
}

/* PoP: fa7__add_reaction @ plugins/platforms/feishu/adapter.py:FeishuAdapter._add_reaction */
static json_t *fa7__add_reaction(fa7_ctx_t *ctx, const json_t *args) {
    const json_t *client = json_obj_get(args, "client");
    const json_t *mid = json_obj_get(args, "message_id");
    const json_t *emoji = json_obj_get(args, "emoji_type");
    if (!fa7_truthy(client) || !fa7_truthy(mid) || !fa7_truthy(emoji)) return json_null();
    json_t *a = fa7_args();
    fa7_set_s(a, "verb", "Add");
    fa7_set_copy(a, "message_id", mid);
    fa7_set_copy(a, "ident", emoji);
    fa7_set_s(a, "build_request", "<build>");
    fa7_set_s(a, "api", "<api>");
    json_t *data = fa7_call(ctx, "_reaction_call", a);
    if (fa7_is_raise(data)) return data;
    if (fa7_is_null(data)) { json_free(data); return json_null(); }
    json_t *ga = fa7_args();
    json_set(ga, "data", data); /* adopted */
    fa7_set_s(ga, "name", "reaction_id");
    json_t *got = fa7_call(ctx, "getattr_reaction", ga);
    /* getattr(obj, name, default) swallows AttributeError only. */
    if (fa7_is_raise(got) && strcmp(json_get_str(got, "__raise__", ""), "AttributeError") == 0) {
        json_free(got);
        return json_null();
    }
    return got;
}

/* PoP: fa7__remove_reaction @ plugins/platforms/feishu/adapter.py:FeishuAdapter._remove_reaction */
static json_t *fa7__remove_reaction(fa7_ctx_t *ctx, const json_t *args) {
    const json_t *client = json_obj_get(args, "client");
    const json_t *mid = json_obj_get(args, "message_id");
    const json_t *rid = json_obj_get(args, "reaction_id");
    if (!fa7_truthy(client) || !fa7_truthy(mid) || !fa7_truthy(rid)) return json_bool(false);
    json_t *a = fa7_args();
    fa7_set_s(a, "verb", "Remove");
    fa7_set_copy(a, "message_id", mid);
    fa7_set_copy(a, "ident", rid);
    fa7_set_s(a, "build_request", "<build>");
    fa7_set_s(a, "api", "<api>");
    json_t *data = fa7_call(ctx, "_reaction_call", a);
    if (fa7_is_raise(data)) return data;
    bool present = !fa7_is_null(data);
    json_free(data);
    return json_bool(present);
}

/* PoP: fa7__media_batch_key @ plugins/platforms/feishu/adapter.py:FeishuAdapter._media_batch_key */
static json_t *fa7__media_batch_key(fa7_ctx_t *ctx, const json_t *args) {
    const json_t *ev = json_obj_get(args, "event");
    json_t *ka = fa7_args();
    fa7_set_copy(ka, "event", ev);
    json_t *key = fa7_call(ctx, "_text_batch_key", ka);
    if (fa7_is_raise(key)) return key;
    char kb[512];
    const char *ks = fa7_pystr(key, kb, sizeof kb);
    const json_t *mt = ev ? json_obj_get(ev, "message_type") : NULL;
    const json_t *vv = (mt && mt->type == JSON_OBJECT) ? json_obj_get(mt, "value") : NULL;
    char vb[256];
    const char *val = fa7_pystr(vv, vb, sizeof vb);
    char out[1024];
    snprintf(out, sizeof out, "%s:media:%s", ks, val);
    json_free(key);
    return json_string(out);
}

/* PoP: fa7__media_batch_is_compatible @ plugins/platforms/feishu/adapter.py:FeishuAdapter._media_batch_is_compatible */
static json_t *fa7__media_batch_is_compatible(fa7_ctx_t *ctx, const json_t *args) {
    const json_t *e = json_obj_get(args, "existing");
    const json_t *in = json_obj_get(args, "incoming");
    const json_t *emt = e ? json_obj_get(e, "message_type") : NULL;
    const json_t *imt = in ? json_obj_get(in, "message_type") : NULL;
    if (!fa7_json_equal(emt, imt)) return json_bool(false); /* `and` short-circuit */
    json_t *a = fa7_args();
    fa7_set_copy(a, "existing", e);
    fa7_set_copy(a, "incoming", in);
    return fa7_call(ctx, "_text_batch_is_compatible", a);
}

/* PoP: fa7__get_cached_sender_name @ plugins/platforms/feishu/adapter.py:FeishuAdapter._get_cached_sender_name */
static json_t *fa7__get_cached_sender_name(fa7_ctx_t *ctx, const json_t *args) {
    const json_t *sid = json_obj_get(args, "sender_id");
    if (!fa7_truthy(sid)) return json_null();
    json_t *ga = fa7_args();
    fa7_set_copy(ga, "sender_id", sid);
    json_t *cached = fa7_call(ctx, "cache_get", ga);
    if (fa7_is_raise(cached)) return cached;
    if (fa7_is_null(cached) || cached->type != JSON_ARRAY || json_len(cached) < 2) {
        json_free(cached);
        return json_null();
    }
    const json_t *name = json_get(cached, 0);
    const json_t *expire = json_get(cached, 1);
    json_t *ta = fa7_call(ctx, "time_now", fa7_args());
    if (fa7_is_raise(ta)) { json_free(cached); return ta; }
    double now = (ta->type == JSON_NUMBER) ? ta->num_val : 0.0;
    json_free(ta);
    double exp = (expire->type == JSON_NUMBER) ? expire->num_val : 0.0;
    json_t *r;
    if (now < exp) {
        r = json_copy(name);
        json_free(cached);
        return r;
    }
    json_free(cached);
    json_t *pa = fa7_args();
    fa7_set_copy(pa, "sender_id", sid);
    json_t *pop = fa7_call(ctx, "cache_pop", pa);
    if (fa7_is_raise(pop)) return pop;
    json_free(pop);
    return json_null();
}

/* PoP: fa7__probe_bot_sdk @ plugins/platforms/feishu/adapter.py:_probe_bot_sdk */
static json_t *fa7__probe_bot_sdk(fa7_ctx_t *ctx, const json_t *args) {
    /* try: */
    json_t *ba = fa7_args();
    fa7_set_copy(ba, "app_id", json_obj_get(args, "app_id"));
    fa7_set_copy(ba, "app_secret", json_obj_get(args, "app_secret"));
    fa7_set_copy(ba, "domain", json_obj_get(args, "domain"));
    json_t *client = fa7_call(ctx, "_build_onboard_client", ba);
    if (fa7_is_raise(client)) goto sdk_except;
    json_t *ra = fa7_args();
    fa7_set_s(ra, "uri", "/open-apis/bot/v3/info");
    json_t *req = fa7_call(ctx, "_tenant_get_request", ra);
    if (fa7_is_raise(req)) { json_free(client); goto sdk_except; }
    json_t *ca = fa7_args();
    json_set(ca, "client", client); /* adopted */
    json_set(ca, "request", req);   /* adopted */
    json_t *resp = fa7_call(ctx, "client_request", ca);
    if (fa7_is_raise(resp)) goto sdk_except;
    /* getattr(resp, "raw", None) */
    json_t *rwa = fa7_args();
    json_set(rwa, "resp", resp); /* adopted */
    fa7_set_s(rwa, "name", "raw");
    json_t *raw = fa7_call(ctx, "getattr_raw", rwa);
    if (fa7_is_raise(raw)) goto sdk_except;
    /* getattr(raw, "content", None) */
    json_t *coa = fa7_args();
    json_set(coa, "raw", raw); /* adopted */
    fa7_set_s(coa, "name", "content");
    json_t *content = fa7_call(ctx, "getattr_content", coa);
    if (fa7_is_raise(content)) goto sdk_except;
    if (fa7_is_null(content)) {
        json_free(content);
        return json_null();
    }
    /* json.loads(content) */
    char cb[2048];
    const char *ct = fa7_pystr(content, cb, sizeof cb);
    json_t *ja = fa7_args();
    fa7_set_s(ja, "text", ct);
    json_t *data = fa7_call(ctx, "json_loads", ja);
    json_free(content);
    if (fa7_is_raise(data)) goto sdk_except;
    json_t *pa = fa7_args();
    json_set(pa, "data", data); /* adopted */
    json_t *result = fa7_call(ctx, "_parse_bot_response", pa);
    if (fa7_is_raise(result)) goto sdk_except;
    return result;
sdk_except:
    {
        /* The pending raise is the last trace event's ret descriptor. */
        json_t *last = json_len(ctx->trace) ? json_get(ctx->trace, json_len(ctx->trace) - 1) : NULL;
        json_t *ret = last ? json_obj_get(last, "ret") : NULL;
        const char *rk = ret ? json_get_str(ret, "__raise__", "Exception") : "Exception";
        if (fa7_is_base_exc(rk)) return ret ? json_copy(ret) : fa7_err("RuntimeError", "");
        /* except Exception as exc: logger.debug(..., exc); return None */
        char emsg[512];
        const char *msg = ret ? json_get_str(ret, "message", "") : "";
        snprintf(emsg, sizeof emsg, "[Feishu onboard] SDK probe failed: %s", msg);
        json_t *la = fa7_args();
        fa7_set_s(la, "message", emsg);
        json_t *lg = fa7_call(ctx, "logger_debug", la);
        if (fa7_is_raise(lg)) return lg; /* raise in handler propagates */
        json_free(lg);
        return json_null();
    }
}

/* PoP: fa7__getattr__ @ plugins/platforms/feishu/adapter.py:_ThreadLocalLoopProxy.__getattr__ */
static json_t *fa7__getattr__(fa7_ctx_t *ctx, const json_t *args) {
    json_t *ta = fa7_call(ctx, "_target", fa7_args());
    if (fa7_is_raise(ta)) return ta;
    json_t *ga = fa7_args();
    json_set(ga, "target", ta); /* adopted */
    fa7_set_copy(ga, "name", json_obj_get(args, "name"));
    return fa7_call(ctx, "getattr_target", ga);
}

/* PoP: fa7__repr__ @ plugins/platforms/feishu/adapter.py:_ThreadLocalLoopProxy.__repr__ */
static json_t *fa7__repr__(fa7_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *ta = fa7_call(ctx, "_target", fa7_args());
    if (fa7_is_raise(ta)) return ta;
    json_t *ra = fa7_args();
    json_set(ra, "target", ta); /* adopted */
    json_t *rep = fa7_call(ctx, "repr_target", ra);
    if (fa7_is_raise(rep)) return rep;
    char rb[1024];
    const char *rs = fa7_pystr(rep, rb, sizeof rb);
    char out[1200];
    snprintf(out, sizeof out, "<ThreadLocalLoopProxy target=%s>", rs);
    json_free(rep);
    return json_string(out);
}
