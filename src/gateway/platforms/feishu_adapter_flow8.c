/*
 * feishu_adapter_flow8.c — Feishu/Lark adapter card/payload-builder, lifecycle,
 * batch-dispatch and connection-internals FLOW cores of
 * plugins/platforms/feishu/adapter.py (hermes-agent).
 *
 * EIGHTH remainder port beyond feishu_adapter_flow.c (message normalization),
 * feishu_adapter_flow2..flow7 (inbound/card, admission, SDK gates, outbound,
 * media/response, proxy/onboarding/reactions/batch). Ports 16 GENUINELY
 * UNCLAIMED defs (a dotted-name-aware claim parser confirmed none are claimed
 * by flows 1-7):
 *   card builders:   _build_update_prompt_card, _build_resolved_approval_card,
 *                    _build_resolved_update_prompt_card
 *   payload builders: _build_outbound_payload, _build_media_post_payload
 *   lifecycle:       _cancel_pending_tasks, _disable_websocket_auto_reconnect,
 *                    _clear_webhook_anomaly, _dedup_persist_lock_or_create
 *   event/batch:     _build_event_handler, _dispatch_inbound_event,
 *                    _delayed_flush, _dispatch_text_batch
 *   connections:     _connect_with_retry, _connect_websocket, _connect_webhook
 *
 * Universal seam: every seamed call enters through ONE callback
 * fa8_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting the
 * identical {"name","args","ret"} event from the SAME scripts. args is ADOPTED
 * into the trace event; the return is a NEW owned json_t (NULL == Python None;
 * {"__raise__":cls,"message":...} == a raised exception). A missing/exhausted
 * script is a fail-closed RuntimeError on both sides.
 *
 * FULL BASEEXCEPTION PASSTHROUGH: fa8_is_base_exc names the family that bypasses
 * `except Exception`. _disable_websocket_auto_reconnect swallows Exception-side
 * auto_reconnect_set raises (finally still fires) and propagates BaseException.
 * _connect_with_retry catches Exception per attempt and re-raises on the last.
 * All other defs propagate every scripted raise verbatim.
 *
 * Representation contract (shared with tests/oracle_feishu_adapter_flow8.py):
 * - NULL json_t* == Python None; JSON_NULL node is a real null value.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__":cls,"message":str(exc)}; a scripted raise is
 *   {"__script__":true,"raise":{"type":cls,"message":msg}}.
 * - Carriers (settings, tasks, event, client, resp) are JSON objects/values;
 *   attribute reads are modeled as universal seams or direct JSON field reads.
 *   The oracle materializes equivalent attribute doubles.
 * - Fixture domain: ASCII strings, scalar/JSON-dict/list values, POSIX paths.
 *   Out.state is {} for every func.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and fa8_-prefixed.
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
typedef struct fa8_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} fa8_ctx_t;

/* ── basic helpers ── */
static bool fa8_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool fa8_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool fa8_is_base_exc(const char *kind) {
    return kind && (
        strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "asyncio.CancelledError") == 0);
}
static bool fa8_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *fa8_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static const char *fa8_str_or(const json_t *v, const char *d) {
    const char *s = fa8_str(v);
    return s ? s : d;
}
static json_t *fa8_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *fa8_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return fa8_err(type, msg);
}
/* The one universal seam. args is ADOPTED; the return is a fresh owned node. */
static json_t *fa8_call(fa8_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = fa8_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = fa8_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = fa8_make_exc(raise);
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
static json_t *fa8_args(void) { return json_object(); }
static void fa8_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void fa8_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
/* Python str(x) for the scalar fixture domain. */
static const char *fa8_pystr(const json_t *v, char *buf, size_t cap) {
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
/* Python repr of a value for interpolated messages (scalar domain). */
static const char *fa8_repr(const json_t *v, char *buf, size_t cap) {
    if (v && v->type == JSON_STRING) { snprintf(buf, cap, "'%s'", v->str_val); return buf; }
    return fa8_pystr(v, buf, cap);
}
/* Deep equality of JSON values (Python == for the scalar/container fixture domain). */
static bool fa8_json_equal(const json_t *a, const json_t *b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->type != b->type) {
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
                if (!fa8_json_equal(json_get(a, i), json_get(b, i))) return false;
            return true;
        case JSON_OBJECT:
            if (json_object_size(a) != json_object_size(b)) return false;
            for (size_t i = 0; i < json_object_size(a); i++) {
                const char *k = json_object_get_key_at(a, i);
                if (!fa8_json_equal(json_object_get_at(a, i), json_obj_get(b, k))) return false;
            }
            return true;
    }
    return false;
}

/* ══ PoP functions ══ */

/* PoP: fa8_build_update_prompt_card @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_update_prompt_card */
static json_t *fa8_build_update_prompt_card(fa8_ctx_t *ctx, const json_t *args) {
    const json_t *prompt = json_obj_get(args, "prompt");
    const json_t *def = json_obj_get(args, "default");
    const json_t *pid = json_obj_get(args, "prompt_id");
    char pb[1024]; char vb[1024];
    const char *ps = fa8_pystr(prompt, pb, sizeof pb);
    const char *ds = fa8_pystr(def, vb, sizeof vb);
    /* default_hint = t(key, default=default) if default else "" */
    char hint[2048]; const char *hs = "";
    if (fa8_truthy(def)) {
        json_t *ta = fa8_args();
        fa8_set_s(ta, "key", "platform.feishu.update_prompt.default_hint");
        fa8_set_s(ta, "default", ds);
        json_t *h = fa8_call(ctx, "t", ta);
        if (fa8_is_raise(h)) return h;
        hs = fa8_pystr(h, hint, sizeof hint);
        /* keep h alive until after str use: released after building actions below */
        char full[4096];
        snprintf(full, sizeof full, "%s%s", ps, hs);
        /* _btn(yes) */
        json_t *ty = fa8_args();
        fa8_set_s(ty, "key", "platform.feishu.update_prompt.answer_yes");
        json_t *ly = fa8_call(ctx, "t", ty);
        if (fa8_is_raise(ly)) { json_free(h); return ly; }
        char yb[1024]; const char *ys = fa8_pystr(ly, yb, sizeof yb);
        json_t *by = fa8_args();
        fa8_set_s(by, "label", ys); fa8_set_s(by, "type", "primary");
        fa8_set_s(by, "body", "{\"hermes_update_prompt_action\": \"y\", \"update_prompt_id\": <pid>}");
        (void)by; /* replaced below with structured body */
        json_free(by);
        json_t *body_y = json_object();
        fa8_set_s(body_y, "hermes_update_prompt_action", "y");
        json_set(body_y, "update_prompt_id", pid ? json_copy(pid) : json_null());
        json_t *ay = fa8_args();
        fa8_set_s(ay, "label", ys); fa8_set_s(ay, "type", "primary");
        json_set(ay, "body", body_y);
        json_t *byr = fa8_call(ctx, "card_button", ay);
        if (fa8_is_raise(byr)) { json_free(h); json_free(ly); return byr; }
        /* _btn(no) */
        json_t *tn = fa8_args();
        fa8_set_s(tn, "key", "platform.feishu.update_prompt.answer_no");
        json_t *ln = fa8_call(ctx, "t", tn);
        if (fa8_is_raise(ln)) { json_free(h); json_free(ly); json_free(byr); return ln; }
        char nb[1024]; const char *ns = fa8_pystr(ln, nb, sizeof nb);
        json_t *body_n = json_object();
        fa8_set_s(body_n, "hermes_update_prompt_action", "n");
        json_set(body_n, "update_prompt_id", pid ? json_copy(pid) : json_null());
        json_t *an = fa8_args();
        fa8_set_s(an, "label", ns); fa8_set_s(an, "type", "danger");
        json_set(an, "body", body_n);
        json_t *bnr = fa8_call(ctx, "card_button", an);
        if (fa8_is_raise(bnr)) { json_free(h); json_free(ly); json_free(byr); json_free(ln); return bnr; }
        json_t *acts = json_array();
        json_append(acts, byr); json_append(acts, bnr);
        json_t *tc = fa8_args();
        fa8_set_s(tc, "key", "platform.feishu.update_prompt.title");
        json_t *title = fa8_call(ctx, "t", tc);
        if (fa8_is_raise(title)) { json_free(h); json_free(ly); json_free(ln); json_free(acts); return title; }
        char tb[1024]; const char *ts = fa8_pystr(title, tb, sizeof tb);
        json_t *ca = fa8_args();
        fa8_set_s(ca, "title", ts); fa8_set_s(ca, "template", "orange");
        fa8_set_s(ca, "text", full);
        json_set(ca, "actions", acts);
        json_free(h); json_free(ly); json_free(ln); json_free(title);
        return fa8_call(ctx, "card", ca);
    }
    /* default falsy: no default_hint seam; default_hint = "" */
    char full[4096];
    snprintf(full, sizeof full, "%s", ps);
    json_t *ty = fa8_args();
    fa8_set_s(ty, "key", "platform.feishu.update_prompt.answer_yes");
    json_t *ly = fa8_call(ctx, "t", ty);
    if (fa8_is_raise(ly)) return ly;
    char yb[1024]; const char *ys = fa8_pystr(ly, yb, sizeof yb);
    json_t *body_y = json_object();
    fa8_set_s(body_y, "hermes_update_prompt_action", "y");
    json_set(body_y, "update_prompt_id", pid ? json_copy(pid) : json_null());
    json_t *ay = fa8_args();
    fa8_set_s(ay, "label", ys); fa8_set_s(ay, "type", "primary");
    json_set(ay, "body", body_y);
    json_t *byr = fa8_call(ctx, "card_button", ay);
    if (fa8_is_raise(byr)) { json_free(ly); return byr; }
    json_t *tn = fa8_args();
    fa8_set_s(tn, "key", "platform.feishu.update_prompt.answer_no");
    json_t *ln = fa8_call(ctx, "t", tn);
    if (fa8_is_raise(ln)) { json_free(ly); json_free(byr); return ln; }
    char nb[1024]; const char *ns = fa8_pystr(ln, nb, sizeof nb);
    json_t *body_n = json_object();
    fa8_set_s(body_n, "hermes_update_prompt_action", "n");
    json_set(body_n, "update_prompt_id", pid ? json_copy(pid) : json_null());
    json_t *an = fa8_args();
    fa8_set_s(an, "label", ns); fa8_set_s(an, "type", "danger");
    json_set(an, "body", body_n);
    json_t *bnr = fa8_call(ctx, "card_button", an);
    if (fa8_is_raise(bnr)) { json_free(ly); json_free(byr); json_free(ln); return bnr; }
    json_t *acts = json_array();
    json_append(acts, byr); json_append(acts, bnr);
    json_t *tc = fa8_args();
    fa8_set_s(tc, "key", "platform.feishu.update_prompt.title");
    json_t *title = fa8_call(ctx, "t", tc);
    if (fa8_is_raise(title)) { json_free(ly); json_free(ln); json_free(acts); return title; }
    char tb[1024]; const char *ts = fa8_pystr(title, tb, sizeof tb);
    json_t *ca = fa8_args();
    fa8_set_s(ca, "title", ts); fa8_set_s(ca, "template", "orange");
    fa8_set_s(ca, "text", full);
    json_set(ca, "actions", acts);
    json_free(ly); json_free(ln); json_free(title);
    return fa8_call(ctx, "card", ca);
}

/* PoP: fa8_build_resolved_approval_card @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_resolved_approval_card */
static json_t *fa8_build_resolved_approval_card(fa8_ctx_t *ctx, const json_t *args) {
    const char *choice = fa8_str_or(json_obj_get(args, "choice"), "");
    const char *user = fa8_str_or(json_obj_get(args, "user_name"), "");
    const json_t *body = json_obj_get(args, "body");
    bool deny = strcmp(choice, "deny") == 0;
    const char *icon = deny ? "❌" : "✅";
    const char *lkey;
    if (strcmp(choice, "once") == 0) lkey = "platform.feishu.approval.resolved_once";
    else if (strcmp(choice, "session") == 0) lkey = "platform.feishu.approval.resolved_session";
    else if (strcmp(choice, "always") == 0) lkey = "platform.feishu.approval.resolved_always";
    else if (deny) lkey = "platform.feishu.approval.resolved_deny";
    else lkey = "platform.feishu.approval.resolved_fallback";
    json_t *la = fa8_args(); fa8_set_s(la, "key", lkey);
    json_t *label = fa8_call(ctx, "t", la);
    if (fa8_is_raise(label)) return label;
    char lb[1024]; const char *ls = fa8_pystr(label, lb, sizeof lb);
    json_t *da = fa8_args();
    fa8_set_s(da, "key", "platform.feishu.approval.resolved_by_user");
    fa8_set_s(da, "icon", icon); fa8_set_s(da, "label", ls); fa8_set_s(da, "user", user);
    json_t *dec = fa8_call(ctx, "t", da);
    if (fa8_is_raise(dec)) { json_free(label); return dec; }
    char db[2048]; const char *ds = fa8_pystr(dec, db, sizeof db);
    char text[4096];
    if (fa8_truthy(body)) {
        char bb[2048]; const char *bs = fa8_pystr(body, bb, sizeof bb);
        snprintf(text, sizeof text, "%s\n\n%s", bs, ds);
    } else {
        snprintf(text, sizeof text, "%s", ds);
    }
    char title[2048];
    snprintf(title, sizeof title, "%s %s", icon, ls);
    json_t *ca = fa8_args();
    fa8_set_s(ca, "title", title);
    fa8_set_s(ca, "template", deny ? "red" : "green");
    fa8_set_s(ca, "text", text);
    json_free(label); json_free(dec);
    return fa8_call(ctx, "card", ca);
}

/* PoP: fa8_build_resolved_update_prompt_card @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_resolved_update_prompt_card */
static json_t *fa8_build_resolved_update_prompt_card(fa8_ctx_t *ctx, const json_t *args) {
    const char *ans = fa8_str_or(json_obj_get(args, "answer"), "");
    const char *user = fa8_str_or(json_obj_get(args, "user_name"), "");
    bool yes = strcmp(ans, "y") == 0;
    /* inner t(word_yes|word_no) is evaluated as the `answer` kwarg of title t() */
    json_t *wa = fa8_args();
    fa8_set_s(wa, "key", yes ? "platform.feishu.update_prompt.word_yes"
                              : "platform.feishu.update_prompt.word_no");
    json_t *word = fa8_call(ctx, "t", wa);
    if (fa8_is_raise(word)) return word;
    char wb[1024]; const char *ws = fa8_pystr(word, wb, sizeof wb);
    json_t *ta = fa8_args();
    fa8_set_s(ta, "key", "platform.feishu.update_prompt.answered_title");
    fa8_set_s(ta, "icon", yes ? "✅" : "❌");
    fa8_set_s(ta, "answer", ws);
    json_t *title = fa8_call(ctx, "t", ta);
    if (fa8_is_raise(title)) { json_free(word); return title; }
    char tb[2048]; const char *ts = fa8_pystr(title, tb, sizeof tb);
    json_t *ba = fa8_args();
    fa8_set_s(ba, "key", "platform.feishu.update_prompt.answered_by");
    fa8_set_s(ba, "user", user);
    json_t *by = fa8_call(ctx, "t", ba);
    if (fa8_is_raise(by)) { json_free(word); json_free(title); return by; }
    char bb[2048]; const char *bs = fa8_pystr(by, bb, sizeof bb);
    json_t *ca = fa8_args();
    fa8_set_s(ca, "title", ts);
    fa8_set_s(ca, "template", yes ? "green" : "red");
    fa8_set_s(ca, "text", bs);
    json_free(word); json_free(title); json_free(by);
    return fa8_call(ctx, "card", ca);
}

/* PoP: fa8_build_outbound_payload @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_outbound_payload */
static json_t *fa8_build_outbound_payload(fa8_ctx_t *ctx, const json_t *args) {
    const json_t *content = json_obj_get(args, "content");
    bool prefer = json_get_bool(args, "prefer_post", false);
    bool use_post = prefer;
    if (!use_post) {
        json_t *ha = fa8_args();
        fa8_set_copy(ha, "content", content);
        json_t *m = fa8_call(ctx, "markdown_hint", ha);
        if (fa8_is_raise(m)) return m;
        use_post = fa8_truthy(m);
        json_free(m);
    }
    json_t *out = json_array();
    if (use_post) {
        json_t *pa = fa8_args();
        fa8_set_copy(pa, "content", content);
        json_t *payload = fa8_call(ctx, "md_post", pa);
        if (fa8_is_raise(payload)) { json_free(out); return payload; }
        json_append(out, json_string("post"));
        json_append(out, payload);
        return out;
    }
    json_t *obj = json_object();
    fa8_set_copy(obj, "text", content);
    json_t *da = fa8_args();
    json_set(da, "obj", obj); /* adopted */
    json_t *ser = fa8_call(ctx, "py_dumps", da);
    if (fa8_is_raise(ser)) { json_free(out); return ser; }
    json_append(out, json_string("text"));
    json_append(out, ser);
    return out;
}

/* PoP: fa8_build_media_post_payload @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_media_post_payload */
static json_t *fa8_build_media_post_payload(fa8_ctx_t *ctx, const json_t *args) {
    const json_t *caption = json_obj_get(args, "caption");
    const json_t *tag = json_obj_get(args, "media_tag");
    json_t *pa = fa8_args();
    fa8_set_copy(pa, "content", caption);
    json_t *ser = fa8_call(ctx, "md_post", pa);
    if (fa8_is_raise(ser)) return ser;
    char cb[8192];
    const char *cs = fa8_pystr(ser, cb, sizeof cb);
    json_t *payload = json_parse(cs, NULL);
    json_free(ser);
    if (!payload || payload->type != JSON_OBJECT) {
        if (payload) json_free(payload);
        return fa8_err("ValueError", "md_post did not return a JSON object");
    }
    json_t *zh = json_obj_get(payload, "zh_cn");
    if (!zh || zh->type != JSON_OBJECT) {
        zh = json_object();
        json_set(payload, "zh_cn", zh);
    }
    json_t *content = json_obj_get(zh, "content");
    if (!content || content->type != JSON_ARRAY) {
        content = json_array();
        json_set(zh, "content", content);
    }
    json_t *row = json_array();
    json_append(row, tag ? json_copy(tag) : json_null());
    json_append(content, row);
    json_t *da = fa8_args();
    json_set(da, "obj", json_copy(payload)); /* oracle passes the post-mutation payload */
    json_free(payload);
    return fa8_call(ctx, "py_dumps", da);
}

/* PoP: fa8_cancel_pending_tasks @ plugins/platforms/feishu/adapter.py:FeishuAdapter._cancel_pending_tasks */
static json_t *fa8_cancel_pending_tasks(fa8_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *vals = fa8_call(ctx, "tasks_values", fa8_args());
    if (fa8_is_raise(vals)) return vals;
    json_t *pending = json_array();
    if (vals->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(vals); i++) {
            const json_t *t = json_get(vals, i);
            json_t *da = fa8_args();
            fa8_set_copy(da, "task", t);
            json_t *done = fa8_call(ctx, "task_done", da);
            if (fa8_is_raise(done)) { json_free(vals); json_free(pending); return done; }
            bool keep = fa8_truthy(t) && !fa8_truthy(done);
            json_free(done);
            if (keep) json_append(pending, json_copy(t));
        }
    }
    json_free(vals);
    for (size_t i = 0; i < json_len(pending); i++) {
        json_t *ca = fa8_args();
        fa8_set_copy(ca, "task", json_get(pending, i));
        json_t *r = fa8_call(ctx, "task_cancel", ca);
        if (fa8_is_raise(r)) { json_free(pending); return r; }
        json_free(r);
    }
    if (json_len(pending) > 0) {
        json_t *ga = fa8_args();
        json_set(ga, "tasks", pending); /* adopted */
        json_t *r = fa8_call(ctx, "gather", ga);
        if (fa8_is_raise(r)) return r;
        json_free(r);
    } else {
        json_free(pending);
    }
    json_t *c = fa8_call(ctx, "tasks_clear", fa8_args());
    if (fa8_is_raise(c)) return c;
    json_free(c);
    return json_null();
}

/* PoP: fa8_disable_websocket_auto_reconnect @ plugins/platforms/feishu/adapter.py:FeishuAdapter._disable_websocket_auto_reconnect */
static json_t *fa8_disable_websocket_auto_reconnect(fa8_ctx_t *ctx, const json_t *args) {
    const json_t *client = json_obj_get(args, "ws_client");
    if (fa8_is_null(client)) return json_null();
    json_t *sa = fa8_args();
    fa8_set_copy(sa, "client", client);
    fa8_set_s(sa, "name", "_auto_reconnect");
    json_set(sa, "value", json_bool(false));
    json_t *r = fa8_call(ctx, "auto_reconnect_set", sa);
    json_t *pending = fa8_is_raise(r) ? r : NULL;
    if (!pending) { json_free(r); }
    /* finally: self._ws_client = None (fires even on raise) */
    json_t *fa = fa8_args();
    fa8_set_s(fa, "name", "_ws_client");
    json_set(fa, "value", json_null());
    json_t *fr = fa8_call(ctx, "self_setattr", fa);
    if (fa8_is_raise(fr)) { if (pending) json_free(pending); return fr; }
    json_free(fr);
    if (pending) {
        const char *k = json_get_str(pending, "__raise__", "Exception");
        if (fa8_is_base_exc(k)) return pending;
        json_free(pending);
    }
    return json_null();
}

/* PoP: fa8_clear_webhook_anomaly @ plugins/platforms/feishu/adapter.py:FeishuAdapter._clear_webhook_anomaly */
static json_t *fa8_clear_webhook_anomaly(fa8_ctx_t *ctx, const json_t *args) {
    json_t *pa = fa8_args();
    fa8_set_copy(pa, "key", json_obj_get(args, "remote_ip"));
    json_t *r = fa8_call(ctx, "anomaly_pop", pa);
    if (fa8_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: fa8_dedup_persist_lock_or_create @ plugins/platforms/feishu/adapter.py:FeishuAdapter._dedup_persist_lock_or_create */
static json_t *fa8_dedup_persist_lock_or_create(fa8_ctx_t *ctx, const json_t *args) {
    const json_t *lock = json_obj_get(args, "dedup_persist_lock");
    if (!fa8_is_null(lock)) return json_copy(lock);
    json_t *nl = fa8_call(ctx, "new_lock", fa8_args());
    if (fa8_is_raise(nl)) return nl;
    json_t *sa = fa8_args();
    fa8_set_s(sa, "name", "_dedup_persist_lock");
    json_set(sa, "value", json_copy(nl));
    json_t *r = fa8_call(ctx, "self_setattr", sa);
    if (fa8_is_raise(r)) { json_free(nl); return r; }
    json_free(r);
    return nl;
}

/* PoP: fa8_build_event_handler @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_event_handler */
static json_t *fa8_build_event_handler(fa8_ctx_t *ctx, const json_t *args) {
    if (!json_get_bool(args, "handler_available", false)) return json_null();
    json_t *ba = fa8_args();
    fa8_set_copy(ba, "encrypt_key", json_obj_get(args, "encrypt_key"));
    fa8_set_copy(ba, "verification_token", json_obj_get(args, "verification_token"));
    json_t *b = fa8_call(ctx, "builder", ba);
    if (fa8_is_raise(b)) return b;
    json_free(b);
    static const char *events[] = {
        "p2_im_message_message_read_v1", "p2_im_message_receive_v1",
        "p2_im_message_reaction_created_v1", "p2_im_message_reaction_deleted_v1",
        "p2_card_action_trigger", "p2_im_chat_member_bot_added_v1",
        "p2_im_chat_member_bot_deleted_v1", "p2_im_chat_access_event_bot_p2p_chat_entered_v1",
        "p2_im_message_recalled_v1", "drive.notice.comment_add_v1", "vc.bot.meeting_invited_v1",
    };
    static const char *handlers[] = {
        "_on_message_read_event", "_on_message_event", "lambda:created", "lambda:deleted",
        "_on_card_action_trigger", "_on_bot_added_to_chat", "_on_bot_removed_from_chat",
        "_on_p2p_chat_entered", "_on_message_recalled", "on_drive_comment_event",
        "on_meeting_invited_event",
    };
    for (size_t i = 0; i < sizeof(events) / sizeof(events[0]); i++) {
        json_t *ra = fa8_args();
        fa8_set_s(ra, "event", events[i]);
        fa8_set_s(ra, "handler", handlers[i]);
        json_t *r = fa8_call(ctx, "register", ra);
        if (fa8_is_raise(r)) return r;
        json_free(r);
    }
    return fa8_call(ctx, "build", fa8_args());
}

/* PoP: fa8_dispatch_inbound_event @ plugins/platforms/feishu/adapter.py:FeishuAdapter._dispatch_inbound_event */
static json_t *fa8_dispatch_inbound_event(fa8_ctx_t *ctx, const json_t *args) {
    const json_t *ev = json_obj_get(args, "event");
    const char *mt = fa8_str_or(json_obj_get(ev, "message_type"), "");
    json_t *ca = fa8_args();
    fa8_set_copy(ca, "event", ev);
    if (strcmp(mt, "text") == 0) {
        json_t *ic = fa8_call(ctx, "is_command", ca); /* adopted ca */
        if (fa8_is_raise(ic)) return ic;
        bool command = fa8_truthy(ic);
        json_free(ic);
        if (!command) {
            json_t *ea = fa8_args();
            fa8_set_copy(ea, "event", ev);
            json_t *r = fa8_call(ctx, "enqueue_text", ea);
            if (fa8_is_raise(r)) return r;
            json_free(r);
            return json_null();
        }
    } else {
        json_free(ca);
    }
    json_t *ba = fa8_args();
    fa8_set_copy(ba, "event", ev);
    json_t *sb = fa8_call(ctx, "should_batch_media", ba);
    if (fa8_is_raise(sb)) return sb;
    bool batch = fa8_truthy(sb);
    json_free(sb);
    if (batch) {
        json_t *ea = fa8_args();
        fa8_set_copy(ea, "event", ev);
        json_t *r = fa8_call(ctx, "enqueue_media", ea);
        if (fa8_is_raise(r)) return r;
        json_free(r);
        return json_null();
    }
    json_t *ha = fa8_args();
    fa8_set_copy(ha, "event", ev);
    json_t *r = fa8_call(ctx, "handle_guards", ha);
    if (fa8_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: fa8_delayed_flush @ plugins/platforms/feishu/adapter.py:FeishuAdapter._delayed_flush */
static json_t *fa8_delayed_flush(fa8_ctx_t *ctx, const json_t *args) {
    const json_t *key = json_obj_get(args, "key");
    const json_t *delay = json_obj_get(args, "delay");
    json_t *cur = fa8_call(ctx, "current_task", fa8_args());
    if (fa8_is_raise(cur)) return cur;
    json_t *sl = fa8_args();
    fa8_set_copy(sl, "delay", delay);
    json_t *sr = fa8_call(ctx, "async_sleep", sl);
    json_t *pending = fa8_is_raise(sr) ? sr : NULL;
    if (!pending) {
        json_free(sr);
        json_t *fa = fa8_args();
        fa8_set_copy(fa, "key", key);
        json_t *fr = fa8_call(ctx, "flush_now", fa);
        if (fa8_is_raise(fr)) pending = fr;
        else json_free(fr);
    }
    /* finally: if task_map.get(key) is current_task: task_map.pop(key) */
    json_t *ga = fa8_args();
    fa8_set_copy(ga, "key", key);
    json_t *got = fa8_call(ctx, "task_map_get", ga);
    json_t *final = NULL;
    if (fa8_is_raise(got)) {
        final = got;
    } else {
        bool same = fa8_json_equal(got, cur);
        json_free(got);
        if (same) {
            json_t *pa = fa8_args();
            fa8_set_copy(pa, "key", key);
            json_t *pop = fa8_call(ctx, "task_map_pop", pa);
            if (fa8_is_raise(pop)) final = pop;
            else json_free(pop);
        }
    }
    json_free(cur);
    if (final) return final;
    if (pending) return pending;
    return json_null();
}

/* PoP: fa8_dispatch_text_batch @ plugins/platforms/feishu/adapter.py:FeishuAdapter._dispatch_text_batch */
static json_t *fa8_dispatch_text_batch(fa8_ctx_t *ctx, const json_t *args) {
    json_t *ha = fa8_args();
    fa8_set_copy(ha, "event", json_obj_get(args, "event"));
    json_t *r = fa8_call(ctx, "handle_guards", ha);
    if (fa8_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: fa8_connect_with_retry @ plugins/platforms/feishu/adapter.py:FeishuAdapter._connect_with_retry */
static json_t *fa8_connect_with_retry(fa8_ctx_t *ctx, const json_t *args) {
    const char *mode = fa8_str_or(json_obj_get(args, "connection_mode"), "");
    bool ws = strcmp(mode, "websocket") == 0;
    for (int attempt = 0; attempt < 3; attempt++) {
        json_t *r = ws ? fa8_call(ctx, "connect_websocket", fa8_args())
                       : fa8_call(ctx, "connect_webhook", fa8_args());
        if (!fa8_is_raise(r)) { json_free(r); return json_null(); }
        json_t *exc = r; /* the raised descriptor */
        const char *exc_kind = json_get_str(exc, "__raise__", "Exception");
        if (fa8_is_base_exc(exc_kind)) return exc; /* except Exception does not catch */
        /* self._running = False */
        json_t *a1 = fa8_args(); fa8_set_s(a1, "name", "_running"); json_set(a1, "value", json_bool(false));
        json_t *x1 = fa8_call(ctx, "self_setattr", a1);
        if (fa8_is_raise(x1)) { json_free(exc); return x1; } json_free(x1);
        /* self._disable_websocket_auto_reconnect() */
        json_t *x2 = fa8_call(ctx, "disable_ws_reconnect", fa8_args());
        if (fa8_is_raise(x2)) { json_free(exc); return x2; } json_free(x2);
        /* self._ws_future = None */
        json_t *a3 = fa8_args(); fa8_set_s(a3, "name", "_ws_future"); json_set(a3, "value", json_null());
        json_t *x3 = fa8_call(ctx, "self_setattr", a3);
        if (fa8_is_raise(x3)) { json_free(exc); return x3; } json_free(x3);
        /* await self._stop_webhook_server() */
        json_t *x4 = fa8_call(ctx, "stop_webhook_server", fa8_args());
        if (fa8_is_raise(x4)) { json_free(exc); return x4; } json_free(x4);
        if (attempt >= 2) return exc; /* raise */
        /* logger.warning with formatted message */
        char emsg[512]; const char *msg = json_get_str(exc, "message", "");
        int wait = 1 << attempt;
        char num[64]; snprintf(num, sizeof num, "%d", wait);
        char num2[64]; snprintf(num2, sizeof num2, "%d", attempt + 1);
        snprintf(emsg, sizeof emsg,
                 "[Feishu] Connect attempt %s/3 failed; retrying in %ss: %s",
                 num2, num, msg);
        json_t *la = fa8_args(); fa8_set_s(la, "message", emsg);
        json_t *lg = fa8_call(ctx, "logger_warning", la);
        if (fa8_is_raise(lg)) { json_free(exc); return lg; } json_free(lg);
        json_free(exc);
        /* await asyncio.sleep(wait_seconds) */
        json_t *sa = fa8_args(); json_set(sa, "delay", json_number((double)wait));
        json_t *sl = fa8_call(ctx, "async_sleep", sa);
        if (fa8_is_raise(sl)) return sl; json_free(sl);
    }
    return json_null();
}

/* PoP: fa8_connect_websocket @ plugins/platforms/feishu/adapter.py:FeishuAdapter._connect_websocket */
static json_t *fa8_connect_websocket(fa8_ctx_t *ctx, const json_t *args) {
    if (!json_get_bool(args, "ws_available", false))
        return fa8_err("RuntimeError", "websockets not installed; websocket mode unavailable");
    json_t *d = fa8_call(ctx, "prepare_client", fa8_args());
    if (fa8_is_raise(d)) return d;
    const json_t *loop = json_obj_get(args, "loop_obj");
    if (fa8_is_null(loop) || json_get_bool(args, "loop_closed", false)) {
        json_free(d);
        return fa8_err("RuntimeError", "adapter loop is not ready");
    }
    json_t *h = fa8_call(ctx, "hydrate_bot_identity", fa8_args());
    if (fa8_is_raise(h)) { json_free(d); return h; } json_free(h);
    json_t *ca = fa8_args();
    fa8_set_copy(ca, "app_id", json_obj_get(args, "app_id"));
    fa8_set_copy(ca, "app_secret", json_obj_get(args, "app_secret"));
    fa8_set_s(ca, "log_level", "INFO");
    fa8_set_copy(ca, "event_handler", json_obj_get(args, "event_handler"));
    fa8_set_copy(ca, "domain", d);
    json_t *tags = json_array(); json_append(tags, json_string("channel"));
    json_set(ca, "extra_ua_tags", tags);
    json_free(d);
    json_t *client = fa8_call(ctx, "new_ws_client", ca);
    if (fa8_is_raise(client)) return client;
    /* self._ws_client = FeishuWSClient(...) */
    json_t *sa1 = fa8_args();
    fa8_set_s(sa1, "name", "_ws_client");
    json_set(sa1, "value", json_copy(client));
    json_t *sr1 = fa8_call(ctx, "self_setattr", sa1);
    if (fa8_is_raise(sr1)) { json_free(client); return sr1; }
    json_free(sr1);
    json_t *ra = fa8_args();
    json_set(ra, "client", client); /* adopted */
    fa8_set_s(ra, "runner", "copy_context.run");
    json_t *r = fa8_call(ctx, "run_in_executor", ra);
    if (fa8_is_raise(r)) return r;
    /* self._ws_future = loop.run_in_executor(...) */
    json_t *sa2 = fa8_args();
    fa8_set_s(sa2, "name", "_ws_future");
    json_set(sa2, "value", json_copy(r));
    json_t *sr2 = fa8_call(ctx, "self_setattr", sa2);
    if (fa8_is_raise(sr2)) { json_free(r); return sr2; }
    json_free(sr2);
    json_free(r);
    return json_null();
}

/* PoP: fa8_connect_webhook @ plugins/platforms/feishu/adapter.py:FeishuAdapter._connect_webhook */
static json_t *fa8_connect_webhook(fa8_ctx_t *ctx, const json_t *args) {
    if (!json_get_bool(args, "webhook_available", false))
        return fa8_err("RuntimeError", "aiohttp not installed; webhook mode unavailable");
    json_t *d = fa8_call(ctx, "prepare_client", fa8_args());
    if (fa8_is_raise(d)) return d;
    json_free(d);
    json_t *h = fa8_call(ctx, "hydrate_bot_identity", fa8_args());
    if (fa8_is_raise(h)) return h;
    json_free(h);
    json_t *aa = fa8_args();
    json_set(aa, "client_max_size", json_number(1048576.0));
    json_t *app = fa8_call(ctx, "web_application", aa);
    if (fa8_is_raise(app)) return app;
    json_free(app);
    json_t *pa = fa8_args();
    fa8_set_copy(pa, "path", json_obj_get(args, "webhook_path"));
    fa8_set_s(pa, "handler", "handler");
    json_t *r = fa8_call(ctx, "add_post", pa);
    if (fa8_is_raise(r)) return r;
    json_free(r);
    json_t *ba = fa8_args();
    fa8_set_copy(ba, "host", json_obj_get(args, "webhook_host"));
    json_set(ba, "port", json_obj_get(args, "webhook_port") ? json_copy(json_obj_get(args, "webhook_port")) : json_null());
    fa8_set_copy(ba, "path", json_obj_get(args, "webhook_path"));
    json_t *runner = fa8_call(ctx, "bind_listener", ba);
    if (fa8_is_raise(runner)) return runner;
    json_t *sa = fa8_args();
    fa8_set_s(sa, "name", "_webhook_runner");
    json_set(sa, "value", runner); /* adopted */
    json_t *sr = fa8_call(ctx, "self_setattr", sa);
    if (fa8_is_raise(sr)) return sr;
    json_free(sr);
    return json_null();
}
