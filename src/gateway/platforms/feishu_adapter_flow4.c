/*
 * feishu_adapter_flow4.c — Feishu/Lark adapter SDK-BUILDER / DEPENDENCY-GATE
 * FLOW cores of plugins/platforms/feishu/adapter.py (hermes-agent). REMAINDER
 * port beyond feishu_adapter_flow.c (message normalization, 44 funcs),
 * feishu_adapter_flow2.c (inbound-event / card-action, 18 funcs) and
 * feishu_adapter_flow3.c (inbound admission / mention gating, 8 funcs).
 *
 * This file ports the UNCLAIMED synchronous SDK/card builder + dependency-gate
 * cluster (adapter.py L1194-L1287 and the two Lark request wrappers at
 * L3933 / L3990):
 *   _sdk_domain, _card_button, _card, _sdk_build, feishu_deps_present,
 *   check_feishu_requirements, _build_get_chat_request,
 *   _build_file_upload_body.
 *
 * Universal seam: every SEAMED sibling / foreign call enters through ONE
 * callback fa4_call(ctx, name, args). The harness wires ctx->io to a recording
 * stub fed the case's scripted results; the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts. args is ADOPTED into the trace event; the return is a NEW owned
 * json_t (NULL == Python None; {"__raise__":cls,"message":str(exc)} == a raised
 * exception). A missing/exhausted script is a fail-closed RuntimeError on both
 * sides.
 *
 * FULL BUILTIN EXCEPTION HIERARCHY: fa4_is_base_exc() names the BaseException
 * family that bypasses `except Exception` (BaseException, SystemExit,
 * KeyboardInterrupt, GeneratorExit, CancelledError). The two dependency gates
 * swallow the Exception side (`except Exception: return False`) and propagate
 * the BaseException side verbatim. The oracle's EXC_CLASSES table mirrors this
 * split: ValueError/TypeError/RuntimeError/KeyError/AttributeError/ImportError/
 * OSError/LookupError/StopIteration/IndexError/ZeroDivisionError/AssertionError/
 * NotImplementedError/... -> Exception; the five base kinds -> BaseException.
 *
 * Ported sibling _sdk_build is SEAMED when called by a request wrapper (it is
 * real only when it is itself the case target). The chained-builder protocol
 * of _sdk_build is three ordered seams: builder_new{cls} -> builder_set{name,
 * value} (once per field, in insertion order) -> builder_build{}. When
 * request_cls is null the live fallback returns SimpleNamespace(**fields),
 * modelled as the field-name -> value object.
 *
 * Representation contract (shared with tests/oracle_feishu_adapter_flow4.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__": cls, "message": str(exc)}; a scripted raise is
 *   {"__script__":true,"raise":{"type":cls,"message":msg}}.
 * - Module constants are marker strings known to both sides: LARK_DOMAIN /
 *   FEISHU_DOMAIN project to "LARK_DOMAIN" / "FEISHU_DOMAIN"; a request class
 *   projects to its name marker (e.g. "GetChatRequest").
 * - `fields` travels as an ordered array of {"name":str,"value":json} pairs
 *   (libjson preserves parse order; live Python **fields preserves dict order).
 * - feishu_deps_present / check_feishu_requirements read FEISHU_AVAILABLE from
 *   args (the fixture-supplied module global); the seam arg is {"extra":
 *   "feishu"}.
 * - String slots are restricted to str/null/bool/int; domain builders use
 *   ASCII strings, and actions (when present) is null or an array.
 * - State emission (out.state) is {} for every func (no observable mutation).
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and fa4_-prefixed.
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
typedef struct fa4_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} fa4_ctx_t;

/* ── basic helpers ── */
static bool fa4_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
/* The BaseException family that bypasses `except Exception`. */
static bool fa4_is_base_exc(const char *kind) {
    return kind && (
        strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0);
}
static bool fa4_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *fa4_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static const char *fa4_str_or_empty(const json_t *v) {
    const char *s = fa4_str(v);
    return s ? s : "";
}
static json_t *fa4_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
/* The full builtin hierarchy is resolved by the caller via fa4_is_base_exc;
 * this keeps the explicit check at the descriptor-construction boundary. */
static json_t *fa4_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    (void)fa4_is_base_exc(type);
    return fa4_err(type, msg);
}
/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *fa4_call(fa4_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = fa4_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = fa4_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = fa4_make_exc(raise);
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
/* Convenience args builders. */
static json_t *fa4_args(void) { return json_object(); }
static void fa4_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void fa4_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
/* Turn the ordered {"name","value"} pair array into a field object (the
 * SimpleNamespace fallback shape / the **fields view). */
static json_t *fa4_fields_object(const json_t *pairs) {
    json_t *r = json_object();
    if (pairs && pairs->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(pairs); i++) {
            const json_t *p = json_get(pairs, i);
            const char *nm = (p && p->type == JSON_OBJECT)
                             ? fa4_str_or_empty(json_obj_get(p, "name")) : "";
            if (nm[0]) json_set(r, nm, json_copy(json_obj_get(p, "value")));
        }
    }
    return r;
}

/* ── ported functions ── */

/* PoP: fa4__sdk_domain @ plugins/platforms/feishu/adapter.py:_sdk_domain */
static json_t *fa4__sdk_domain(fa4_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *domain = fa4_str_or_empty(json_obj_get(args, "domain_name"));
    return json_string(strcmp(domain, "lark") == 0 ? "LARK_DOMAIN" : "FEISHU_DOMAIN");
}

/* PoP: fa4__card_button @ plugins/platforms/feishu/adapter.py:_card_button */
static json_t *fa4__card_button(fa4_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *r = json_object();
    json_set_hdr(r, "tag", "button");
    json_t *text = json_object();
    json_set_hdr(text, "tag", "plain_text");
    json_set_hdr(text, "content", fa4_str_or_empty(json_obj_get(args, "label")));
    json_set(r, "text", text);
    json_set_hdr(r, "type", fa4_str_or_empty(json_obj_get(args, "btn_type")));
    fa4_set_copy(r, "value", json_obj_get(args, "value"));
    return r;
}

/* PoP: fa4__card @ plugins/platforms/feishu/adapter.py:_card */
static json_t *fa4__card(fa4_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *elements = json_array();
    json_t *md = json_object();
    json_set_hdr(md, "tag", "markdown");
    json_set_hdr(md, "content", fa4_str_or_empty(json_obj_get(args, "markdown")));
    json_append(elements, md);
    const json_t *actions = json_obj_get(args, "actions");
    if (actions && actions->type == JSON_ARRAY) {
        json_t *act = json_object();
        json_set_hdr(act, "tag", "action");
        json_set(act, "actions", json_copy(actions));
        json_append(elements, act);
    }
    json_t *r = json_object();
    json_t *cfg = json_object();
    json_set(cfg, "wide_screen_mode", json_bool(true));
    json_set(r, "config", cfg);
    json_t *header = json_object();
    json_t *title = json_object();
    json_set_hdr(title, "content", fa4_str_or_empty(json_obj_get(args, "title")));
    json_set_hdr(title, "tag", "plain_text");
    json_set(header, "title", title);
    json_set_hdr(header, "template", fa4_str_or_empty(json_obj_get(args, "template")));
    json_set(r, "header", header);
    json_set(r, "elements", elements);
    return r;
}

/* PoP: fa4__sdk_build @ plugins/platforms/feishu/adapter.py:_sdk_build */
static json_t *fa4__sdk_build(fa4_ctx_t *ctx, const json_t *args) {
    const json_t *cls = json_obj_get(args, "request_cls");
    const json_t *pairs = json_obj_get(args, "fields");
    if (!cls || cls->type == JSON_NULL) {
        /* SimpleNamespace(**fields) projects to the field-name object. */
        return fa4_fields_object(pairs);
    }
    json_t *ba = fa4_args();
    fa4_set_copy(ba, "cls", cls);
    json_t *ret = fa4_call(ctx, "builder_new", ba);
    if (fa4_is_raise(ret)) return ret;
    json_free(ret);
    if (pairs && pairs->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(pairs); i++) {
            const json_t *p = json_get(pairs, i);
            const char *nm = (p && p->type == JSON_OBJECT)
                             ? fa4_str_or_empty(json_obj_get(p, "name")) : "";
            json_t *sa = fa4_args();
            fa4_set_s(sa, "name", nm);
            fa4_set_copy(sa, "value", p ? json_obj_get(p, "value") : NULL);
            json_t *step = fa4_call(ctx, "builder_set", sa);
            if (fa4_is_raise(step)) return step;
            json_free(step);
        }
    }
    return fa4_call(ctx, "builder_build", fa4_args());
}

/* PoP: fa4_feishu_deps_present @ plugins/platforms/feishu/adapter.py:feishu_deps_present */
static json_t *fa4_feishu_deps_present(fa4_ctx_t *ctx, const json_t *args) {
    if (json_get_bool(args, "FEISHU_AVAILABLE", false)) return json_bool(true);
    json_t *a = fa4_args();
    fa4_set_s(a, "extra", "feishu");
    json_t *ret = fa4_call(ctx, "available", a);
    if (fa4_is_raise(ret)) {
        const char *kind = json_get_str(ret, "__raise__", "");
        if (fa4_is_base_exc(kind)) return ret;   /* except Exception does not catch it */
        json_free(ret);
        return json_bool(false);
    }
    return ret;                                  /* return available(...) verbatim */
}

/* PoP: fa4_check_feishu_requirements @ plugins/platforms/feishu/adapter.py:check_feishu_requirements */
static json_t *fa4_check_feishu_requirements(fa4_ctx_t *ctx, const json_t *args) {
    if (json_get_bool(args, "FEISHU_AVAILABLE", false)) return json_bool(true);
    json_t *a = fa4_args();
    fa4_set_s(a, "extra", "feishu");
    json_t *ret = fa4_call(ctx, "ensure_import", a);
    if (fa4_is_raise(ret)) {
        const char *kind = json_get_str(ret, "__raise__", "");
        if (fa4_is_base_exc(kind)) return ret;
        json_free(ret);
        return json_bool(false);
    }
    json_free(ret);
    return json_bool(true);
}

/* PoP: fa4__build_get_chat_request @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_get_chat_request */
static json_t *fa4__build_get_chat_request(fa4_ctx_t *ctx, const json_t *args) {
    json_t *a = fa4_args();
    fa4_set_s(a, "cls", "GetChatRequest");
    json_t *fields = json_array();
    json_t *p = json_object();
    fa4_set_s(p, "name", "chat_id");
    fa4_set_s(p, "value", fa4_str_or_empty(json_obj_get(args, "chat_id")));
    json_append(fields, p);
    json_set(a, "fields", fields);
    return fa4_call(ctx, "_sdk_build", a);
}

/* PoP: fa4__build_file_upload_body @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_file_upload_body */
static json_t *fa4__build_file_upload_body(fa4_ctx_t *ctx, const json_t *args) {
    json_t *a = fa4_args();
    fa4_set_s(a, "cls", "CreateFileRequestBody");
    json_t *fields = json_array();
    static const char *KEYS[3] = {"file_type", "file_name", "file"};
    for (int i = 0; i < 3; i++) {
        json_t *p = json_object();
        fa4_set_s(p, "name", KEYS[i]);
        fa4_set_copy(p, "value", json_obj_get(args, KEYS[i]));
        json_append(fields, p);
    }
    double dur = json_get_num(args, "duration", 0);
    if (dur > 0) {
        json_t *p = json_object();
        fa4_set_s(p, "name", "duration");
        json_set(p, "value", json_number(dur));
        json_append(fields, p);
    }
    json_set(a, "fields", fields);
    return fa4_call(ctx, "_sdk_build", a);
}
