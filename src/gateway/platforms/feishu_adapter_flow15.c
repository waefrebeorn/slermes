/*
 * feishu_adapter_flow15.c — Feishu/Lark adapter "remainders" FLOW cores of
 * plugins/platforms/feishu/adapter.py (hermes-agent).
 *
 * FIFTEENTH remainder port beyond feishu_adapter_flow.c .. flow14. Ports 14
 * unclaimed defs (none is a TARGET of flows 1-14):
 *   _EA_ACTION_LABELS, _target, _teardown_ws_thread, _tenant_get_raw,
 *   _tenant_get_request, _write_update_prompt_response, delete_message,
 *   edit_message, get_chat_info, on_processing_complete, on_processing_start,
 *   qr_register, send_animation, send_document.
 *
 * Universal seam: every seamed call enters through ONE callback
 * fa15_call(ctx, name, args). args is ADOPTED into the trace event; the return
 * is a NEW owned json_t (NULL == Python None;
 * {"__raise__":cls,"message":...} == a raised exception). A missing/exhausted
 * script is a fail-closed RuntimeError on both sides.
 *
 * FULL BASEEXCEPTION PASSTHROUGH: fa15_is_base_exc + fa15_raise_is_base name
 * the family that bypasses `except Exception`; those raises propagate verbatim.
 *
 * Representation contract (shared with tests/oracle_feishu_adapter_flow15.py):
 * - NULL json_t* == Python None; JSON_NULL node is a real null value.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__":cls,"message":str(exc)}; a scripted raise is
 *   {"__script__":true,"raise":{"type":cls,"message":msg}}.
 * - SendResult is projected to {success,message_id,error}.
 * - Logger calls are modeled SILENT (no seam) on both sides; only observable
 *   decision/collaborator calls are seamed.
 * - Fixture domain: ASCII scalars + JSON dict/list values; emoji prefixes on
 *   _EA_ACTION_LABELS are UTF-8 literals. State is always {}.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and fa15_-prefixed.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct fa15_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} fa15_ctx_t;

/* ── basic helpers ── */
static bool fa15_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool fa15_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool fa15_is_base_exc(const char *kind) {
    return kind && (
        strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "asyncio.CancelledError") == 0);
}
static bool fa15_raise_is_base(const json_t *r) {
    return fa15_is_raise(r) && fa15_is_base_exc(json_get_str(r, "__raise__", ""));
}
static bool fa15_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *fa15_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static const char *fa15_str_or(const json_t *v, const char *d) {
    const char *s = fa15_str(v);
    return s ? s : d;
}
static bool fa15_bool(const json_t *v, bool d) {
    return (v && v->type == JSON_BOOL) ? v->bool_val : d;
}
static json_t *fa15_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *fa15_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return fa15_err(type, msg);
}
/* The one universal seam. args is ADOPTED; the return is a fresh owned node. */
static json_t *fa15_call(fa15_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = fa15_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = fa15_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = fa15_make_exc(raise);
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
static json_t *fa15_args(void) { return json_object(); }
static void fa15_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void fa15_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void fa15_set_n(json_t *a, const char *k, double n) { json_set(a, k, json_number(n)); }
static void fa15_set_b(json_t *a, const char *k, bool b) { json_set(a, k, json_bool(b)); }
static json_t *fa15_send_result(bool success, const char *mid, const char *err) {
    json_t *r = json_object();
    fa15_set_b(r, "success", success);
    fa15_set_copy(r, "message_id", mid ? json_string(mid) : json_null());
    fa15_set_copy(r, "error", err ? json_string(err) : json_null());
    return r;
}
/* Python str.strip() + optional lower, ASCII fixture domain. */
static const char *fa15_snorm(const char *s, char *out, size_t cap, bool lower) {
    size_t n = 0;
    const char *p = s ? s : "";
    while (*p && n + 1 < cap) {
        char c = *p++;
        out[n++] = (lower && c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    out[n] = '\0';
    size_t st = 0, en = n;
    while (st < en && (out[st]==' '||out[st]=='\t'||out[st]=='\n'||out[st]=='\r'||out[st]=='\v'||out[st]=='\f')) st++;
    while (en > st && (out[en-1]==' '||out[en-1]=='\t'||out[en-1]=='\n'||out[en-1]=='\r'||out[en-1]=='\v'||out[en-1]=='\f')) en--;
    out[en] = '\0';
    if (st) memmove(out, out + st, en - st + 1);
    return out;
}
/* qr_register's caught exception family: (RuntimeError, URLError, OSError,
 * json.JSONDecodeError). URLError subclasses OSError in the oracle. */
static bool fa15_qr_catch(const char *kind) {
    return kind && (
        strcmp(kind, "RuntimeError") == 0 || strcmp(kind, "OSError") == 0
        || strcmp(kind, "URLError") == 0 || strcmp(kind, "IOError") == 0
        || strcmp(kind, "FileNotFoundError") == 0 || strcmp(kind, "ConnectionError") == 0
        || strcmp(kind, "TimeoutError") == 0
        || strcmp(kind, "JSONDecodeError") == 0 || strcmp(kind, "json.JSONDecodeError") == 0);
}
/* teardown wait_ws_future swallow family: TimeoutError, CancelledError, the
 * whole Exception family; BaseException trio still propagates. */
static bool fa15_teardown_wait_catch(const char *kind) {
    if (!kind) return false;
    /* live has `except asyncio.CancelledError` in the ws-future wait. */
    if (strcmp(kind, "CancelledError") == 0 || strcmp(kind, "asyncio.CancelledError") == 0)
        return true;
    if (fa15_is_base_exc(kind)) return false;
    return true; /* all other non-BaseException raise types are swallowed */
}

/* ══ PoP functions ══ */

/* PoP: fa15__target @ plugins/platforms/feishu/adapter.py:_ThreadLocalLoopProxy._target */
static json_t *fa15__target(fa15_ctx_t *ctx, const json_t *args) {
    json_t *loop = fa15_call(ctx, "ws_loop", fa15_args());
    if (fa15_is_raise(loop)) return loop;
    if (fa15_truthy(loop)) return loop;
    json_free(loop);
    const json_t *f = json_obj_get(args, "_fallback");
    return f ? json_copy(f) : json_null();
}

/* PoP: fa15__EA_ACTION_LABELS @ plugins/platforms/feishu/adapter.py:FeishuAdapter._EA_ACTION_LABELS */
static json_t *fa15__EA_ACTION_LABELS(fa15_ctx_t *ctx, const json_t *args) {
    (void)args;
    static const char *names[4] = {"once", "session", "always", "deny"};
    static const char *keys[4] = {
        "gateway.exec_approval.action_once",
        "platform.feishu.approval.action_session",
        "platform.feishu.approval.action_always",
        "gateway.exec_approval.action_deny"};
    static const char *pre[4] = {"\xe2\x9c\x85 ", "\xe2\x9c\x85 ", "\xe2\x9c\x85 ", "\xe2\x9d\x8c "};
    json_t *out = json_object();
    for (int i = 0; i < 4; i++) {
        json_t *a = fa15_args();
        fa15_set_s(a, "key", keys[i]);
        json_t *r = fa15_call(ctx, "t", a);
        if (fa15_is_raise(r)) { json_free(out); return r; }
        char buf[1024];
        snprintf(buf, sizeof buf, "%s%s", pre[i], fa15_str_or(r, ""));
        json_set_hdr(out, names[i], buf);
        json_free(r);
    }
    return out;
}

/* PoP: fa15__tenant_get_request @ plugins/platforms/feishu/adapter.py:_tenant_get_request */
static json_t *fa15__tenant_get_request(fa15_ctx_t *ctx, const json_t *args) {
    json_t *b = fa15_call(ctx, "base_builder", fa15_args());
    if (fa15_is_raise(b)) return b;
    json_t *a = fa15_args(); fa15_set_s(a, "method", "GET");
    json_t *r = fa15_call(ctx, "http_method", a);
    if (fa15_is_raise(r)) { json_free(b); return r; }
    json_free(b); b = r;
    a = fa15_args(); fa15_set_copy(a, "uri", json_obj_get(args, "uri"));
    r = fa15_call(ctx, "uri", a);
    if (fa15_is_raise(r)) { json_free(b); return r; }
    json_free(b); b = r;
    const json_t *q = json_obj_get(args, "queries");
    if (q && q->type != JSON_NULL) {
        a = fa15_args(); fa15_set_copy(a, "queries", q);
        r = fa15_call(ctx, "queries", a);
        if (fa15_is_raise(r)) { json_free(b); return r; }
        json_free(b); b = r;
    }
    a = fa15_args();
    json_t *tt = json_array(); json_append(tt, json_string("TENANT"));
    json_set(a, "token_types", tt);
    r = fa15_call(ctx, "token_types", a);
    if (fa15_is_raise(r)) { json_free(b); return r; }
    json_free(b); b = r;
    r = fa15_call(ctx, "build", fa15_args());
    json_free(b);
    return r;
}

/* PoP: fa15__tenant_get_raw @ plugins/platforms/feishu/adapter.py:FeishuAdapter._tenant_get_raw */
static json_t *fa15__tenant_get_raw(fa15_ctx_t *ctx, const json_t *args) {
    json_t *a = fa15_args();
    fa15_set_copy(a, "uri", json_obj_get(args, "uri"));
    fa15_set_copy(a, "queries", json_obj_get(args, "queries"));
    json_t *req = fa15_call(ctx, "tenant_get_request", a);
    if (fa15_is_raise(req)) return req;
    a = fa15_args(); fa15_set_s(a, "method", "request"); fa15_set_copy(a, "request", req);
    json_free(req);
    json_t *resp = fa15_call(ctx, "run_blocking", a);
    if (fa15_is_raise(resp)) return resp;
    json_t *content = json_null();
    const json_t *raw = json_obj_get(resp, "raw");
    if (raw && raw->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(raw, "content");
        content = c ? json_copy(c) : json_null();
    }
    json_free(resp);
    return content;
}

/* PoP: fa15__write_update_prompt_response @ plugins/platforms/feishu/adapter.py:FeishuAdapter._write_update_prompt_response */
static json_t *fa15__write_update_prompt_response(fa15_ctx_t *ctx, const json_t *args) {
    json_t *home = fa15_call(ctx, "get_hermes_home", fa15_args());
    if (fa15_is_raise(home)) return home;
    json_t *a = fa15_args(); fa15_set_copy(a, "base", home); fa15_set_s(a, "part", ".update_response");
    json_t *rp = fa15_call(ctx, "path_join", a);
    json_free(home);
    if (fa15_is_raise(rp)) return rp;
    a = fa15_args(); fa15_set_copy(a, "path", rp); fa15_set_s(a, "suffix", ".tmp");
    json_t *tp = fa15_call(ctx, "path_with_suffix", a);
    if (fa15_is_raise(tp)) { json_free(rp); return tp; }
    a = fa15_args(); fa15_set_copy(a, "path", tp);
    fa15_set_copy(a, "answer", json_obj_get(args, "answer")); fa15_set_s(a, "encoding", "utf-8");
    json_t *r = fa15_call(ctx, "write_text", a);
    if (fa15_is_raise(r)) { json_free(tp); json_free(rp); return r; }
    json_free(r);
    a = fa15_args(); fa15_set_copy(a, "src", tp); fa15_set_copy(a, "dst", rp);
    json_free(tp); json_free(rp);
    return fa15_call(ctx, "path_replace", a);
}

/* PoP: fa15_delete_message @ plugins/platforms/feishu/adapter.py:FeishuAdapter.delete_message */
static json_t *fa15_delete_message(fa15_ctx_t *ctx, const json_t *args) {
    if (!fa15_truthy(json_obj_get(args, "has_client")) ||
        !fa15_truthy(json_obj_get(args, "message_id"))) return json_bool(false);
    json_t *a = fa15_args(); fa15_set_copy(a, "message_id", json_obj_get(args, "message_id"));
    json_t *r = fa15_call(ctx, "build_delete_request", a);
    if (fa15_is_raise(r)) goto fail;
    a = fa15_args(); fa15_set_s(a, "method", "delete"); fa15_set_copy(a, "request", r); json_free(r);
    json_t *resp = fa15_call(ctx, "run_blocking", a);
    if (fa15_is_raise(resp)) { r = resp; goto fail; }
    a = fa15_args(); fa15_set_copy(a, "response", resp);
    json_t *ok = fa15_call(ctx, "response_succeeded", a);
    json_free(resp);
    if (fa15_is_raise(ok)) { r = ok; goto fail; }
    bool succ = fa15_truthy(ok); json_free(ok);
    return json_bool(succ);
fail:
    if (fa15_raise_is_base(r)) return r;
    json_free(r);
    return json_bool(false);
}

/* helper for edit_message's inner _update */
static json_t *fa15_edit_update(fa15_ctx_t *ctx, const char *msg_type,
                                const json_t *payload, const char *mid) {
    json_t *a = fa15_args();
    fa15_set_s(a, "msg_type", msg_type);
    fa15_set_copy(a, "content", payload);
    json_t *body = fa15_call(ctx, "build_update_body", a);
    if (fa15_is_raise(body)) return body;
    a = fa15_args(); fa15_set_s(a, "message_id", mid); fa15_set_copy(a, "request_body", body);
    json_free(body);
    json_t *req = fa15_call(ctx, "build_update_request", a);
    if (fa15_is_raise(req)) return req;
    a = fa15_args(); fa15_set_s(a, "method", "update"); fa15_set_copy(a, "request", req);
    json_free(req);
    json_t *resp = fa15_call(ctx, "run_blocking", a);
    if (fa15_is_raise(resp)) return resp;
    a = fa15_args(); fa15_set_copy(a, "response", resp); fa15_set_s(a, "failure_message", "update failed");
    json_free(resp);
    return fa15_call(ctx, "finalize_send_result", a);
}

/* PoP: fa15_edit_message @ plugins/platforms/feishu/adapter.py:FeishuAdapter.edit_message */
static json_t *fa15_edit_message(fa15_ctx_t *ctx, const json_t *args) {
    if (!fa15_truthy(json_obj_get(args, "has_client")))
        return fa15_send_result(false, NULL, "Not connected");
    const char *mid = fa15_str_or(json_obj_get(args, "message_id"), "");
    /* format_message runs BEFORE the try. */
    json_t *a = fa15_args(); fa15_set_copy(a, "content", json_obj_get(args, "content"));
    json_t *content = fa15_call(ctx, "format_message", a);
    if (fa15_is_raise(content)) return content;
    json_t *r;
    a = fa15_args(); fa15_set_copy(a, "content", content);
    json_t *payload = fa15_call(ctx, "build_outbound_payload", a);
    if (fa15_is_raise(payload)) { r = payload; goto fail; }
    const char *msgtype = fa15_str_or(json_get(payload, 0), "");
    const json_t *pl = json_get(payload, 1);
    json_t *result = fa15_edit_update(ctx, msgtype, pl, mid);
    json_free(payload);
    if (fa15_is_raise(result)) { r = result; goto fail; }
    bool success = fa15_bool(json_obj_get(result, "success"), false);
    if (!success && strcmp(msgtype, "post") == 0) {
        const json_t *err = json_obj_get(result, "error");
        a = fa15_args();
        fa15_set_copy(a, "error", (err && err->type == JSON_STRING) ? err : json_string(""));
        json_t *iv = fa15_call(ctx, "post_invalid_search", a);
        if (fa15_is_raise(iv)) { r = iv; json_free(result); goto fail; }
        bool invalid = fa15_truthy(iv); json_free(iv);
        if (invalid) {
            a = fa15_args(); fa15_set_copy(a, "content", content);
            json_t *plain = fa15_call(ctx, "strip_markdown", a);
            if (fa15_is_raise(plain)) { r = plain; json_free(result); goto fail; }
            json_t *dobj = json_object(); fa15_set_copy(dobj, "text", plain); json_free(plain);
            a = fa15_args(); fa15_set_copy(a, "value", dobj); json_free(dobj);
            json_t *jp = fa15_call(ctx, "json_dumps", a);
            if (fa15_is_raise(jp)) { r = jp; json_free(result); goto fail; }
            json_t *nr = fa15_edit_update(ctx, "text", jp, mid);
            json_free(jp); json_free(result);
            if (fa15_is_raise(nr)) { r = nr; goto fail; }
            result = nr;
        }
    }
    success = fa15_bool(json_obj_get(result, "success"), false);
    if (success) json_set_hdr(result, "message_id", mid);
    json_free(content);
    return result;
fail:
    if (fa15_raise_is_base(r)) { json_free(content); return r; }
    /* except Exception: error envelope (logger silent) */
    {
        const char *rmsg = json_get_str(r, "message", "");
        json_t *pe = fa15_args(); fa15_set_s(pe, "error", rmsg);
        json_t *per = fa15_call(ctx, "proxy_error", pe);
        const char *errs = rmsg;
        if (fa15_truthy(per)) errs = fa15_str_or(per, rmsg);
        if (fa15_is_raise(per)) { json_free(r); json_free(content); return per; }
        /* errs borrows from per: build the envelope BEFORE freeing per. */
        json_t *out = fa15_send_result(false, NULL, errs);
        json_free(per);
        json_free(r); json_free(content);
        return out;
    }
}

static json_t *fa15_chat_fallback(const char *cid) {
    json_t *f = json_object();
    fa15_set_s(f, "chat_id", cid); fa15_set_s(f, "name", cid); fa15_set_s(f, "type", "dm");
    return f;
}

/* PoP: fa15_get_chat_info @ plugins/platforms/feishu/adapter.py:FeishuAdapter.get_chat_info */
static json_t *fa15_get_chat_info(fa15_ctx_t *ctx, const json_t *args) {
    const char *cid = fa15_str_or(json_obj_get(args, "chat_id"), "");
    if (!fa15_truthy(json_obj_get(args, "has_client"))) return fa15_chat_fallback(cid);
    json_t *a = fa15_args(); fa15_set_s(a, "chat_id", cid);
    json_t *cached = fa15_call(ctx, "cache_get", a);
    if (fa15_is_raise(cached)) return cached;
    if (!fa15_is_null(cached)) return cached;
    json_free(cached);
    a = fa15_args(); fa15_set_s(a, "chat_id", cid);
    json_t *req = fa15_call(ctx, "build_chat_request", a);
    json_t *r;
    if (fa15_is_raise(req)) { r = req; goto fail; }
    a = fa15_args(); fa15_set_s(a, "method", "chat.get"); fa15_set_copy(a, "request", req);
    json_free(req);
    json_t *resp = fa15_call(ctx, "run_blocking", a);
    if (fa15_is_raise(resp)) { r = resp; goto fail; }
    bool fail_resp = false;
    if (!fa15_truthy(resp)) fail_resp = true;
    else {
        const json_t *succ = json_obj_get(resp, "success");
        if (!succ || !fa15_bool(succ, true)) fail_resp = true;
    }
    if (fail_resp) { json_free(resp); return fa15_chat_fallback(cid); }
    const json_t *data = json_obj_get(resp, "data");
    char rt[512], rm[512], name[512];
    fa15_snorm(fa15_str_or(json_obj_get(data, "chat_type"), ""), rt, sizeof rt, true);
    fa15_snorm(fa15_str_or(json_obj_get(data, "chat_mode"), ""), rm, sizeof rm, true);
    const char *dn = fa15_str_or(json_obj_get(data, "name"), "");
    snprintf(name, sizeof name, "%s", dn[0] ? dn : cid);
    a = fa15_args(); fa15_set_s(a, "raw_type", rt);
    json_t *mapped = fa15_call(ctx, "map_chat_type", a);
    if (fa15_is_raise(mapped)) { r = mapped; json_free(resp); goto fail; }
    json_t *info = json_object();
    fa15_set_s(info, "chat_id", cid);
    fa15_set_s(info, "name", name);
    fa15_set_copy(info, "type", mapped);
    json_free(mapped);
    if (rt[0]) fa15_set_s(info, "raw_type", rt); else json_set(info, "raw_type", json_null());
    if (rm[0]) fa15_set_s(info, "raw_mode", rm); else json_set(info, "raw_mode", json_null());
    json_t *ret = json_copy(info);
    a = fa15_args(); fa15_set_s(a, "chat_id", cid); fa15_set_copy(a, "info", info);
    json_free(info);
    json_t *cs = fa15_call(ctx, "cache_set", a);
    if (fa15_is_raise(cs)) { r = cs; json_free(resp); json_free(ret); goto fail; }
    json_free(cs);
    json_free(resp);
    return ret;
fail:
    if (fa15_raise_is_base(r)) return r;
    json_free(r);
    return fa15_chat_fallback(cid);
}

/* PoP: fa15_on_processing_start @ plugins/platforms/feishu/adapter.py:FeishuAdapter.on_processing_start */
static json_t *fa15_on_processing_start(fa15_ctx_t *ctx, const json_t *args) {
    const char *mid = fa15_str_or(json_obj_get(args, "message_id"), "");
    json_t *en = fa15_call(ctx, "reactions_enabled", fa15_args());
    if (fa15_is_raise(en)) return en;
    if (!fa15_truthy(en)) { json_free(en); return json_null(); }
    json_free(en);
    if (!mid[0]) return json_null();
    json_t *a = fa15_args(); fa15_set_s(a, "message_id", mid);
    json_t *cc = fa15_call(ctx, "cache_contains", a);
    if (fa15_is_raise(cc)) return cc;
    bool present = fa15_truthy(cc); json_free(cc);
    if (present) return json_null();
    a = fa15_args(); fa15_set_s(a, "message_id", mid); fa15_set_s(a, "kind", "in_progress");
    json_t *rid = fa15_call(ctx, "add_reaction", a);
    if (fa15_is_raise(rid)) return rid;
    if (fa15_truthy(rid)) {
        a = fa15_args(); fa15_set_s(a, "message_id", mid); fa15_set_copy(a, "reaction_id", rid);
        json_t *st = fa15_call(ctx, "cache_store", a);
        if (fa15_is_raise(st)) { json_free(rid); return st; }
        json_free(st);
    }
    json_free(rid);
    return json_null();
}

/* PoP: fa15_on_processing_complete @ plugins/platforms/feishu/adapter.py:FeishuAdapter.on_processing_complete */
static json_t *fa15_on_processing_complete(fa15_ctx_t *ctx, const json_t *args) {
    const char *mid = fa15_str_or(json_obj_get(args, "message_id"), "");
    json_t *en = fa15_call(ctx, "reactions_enabled", fa15_args());
    if (fa15_is_raise(en)) return en;
    if (!fa15_truthy(en) || !mid[0]) { json_free(en); return json_null(); }
    json_free(en);
    json_t *a = fa15_args(); fa15_set_s(a, "message_id", mid);
    json_t *rid = fa15_call(ctx, "proc_cache_get", a);
    if (fa15_is_raise(rid)) return rid;
    if (fa15_truthy(rid)) {
        a = fa15_args(); fa15_set_s(a, "message_id", mid); fa15_set_copy(a, "reaction_id", rid);
        json_t *ok = fa15_call(ctx, "remove_reaction", a);
        json_free(rid);
        if (fa15_is_raise(ok)) return ok;
        if (!fa15_truthy(ok)) { json_free(ok); return json_null(); }
        json_free(ok);
        a = fa15_args(); fa15_set_s(a, "message_id", mid);
        json_t *pp = fa15_call(ctx, "cache_pop", a);
        if (fa15_is_raise(pp)) return pp;
        json_free(pp);
    } else json_free(rid);
    if (strcmp(fa15_str_or(json_obj_get(args, "outcome"), ""), "FAILURE") == 0) {
        a = fa15_args(); fa15_set_s(a, "message_id", mid); fa15_set_s(a, "kind", "failure");
        json_t *r = fa15_call(ctx, "add_reaction", a);
        if (fa15_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* PoP: fa15_send_document @ plugins/platforms/feishu/adapter.py:FeishuAdapter.send_document */
static json_t *fa15_send_document(fa15_ctx_t *ctx, const json_t *args) {
    static const char *keys[6] = {"chat_id", "file_path", "reply_to", "metadata", "caption", "file_name"};
    json_t *a = fa15_args();
    for (int i = 0; i < 6; i++) fa15_set_copy(a, keys[i], json_obj_get(args, keys[i]));
    return fa15_call(ctx, "send_uploaded_file_message", a);
}

/* PoP: fa15_send_animation @ plugins/platforms/feishu/adapter.py:FeishuAdapter.send_animation */
static json_t *fa15_send_animation(fa15_ctx_t *ctx, const json_t *args) {
    json_t *a = fa15_args();
    fa15_set_copy(a, "url", json_obj_get(args, "animation_url"));
    fa15_set_s(a, "default_ext", ".gif"); fa15_set_s(a, "preferred_name", "animation.gif");
    json_t *dl = fa15_call(ctx, "download_remote_document", a);
    json_t *r;
    if (fa15_is_raise(dl)) {
        if (fa15_raise_is_base(dl)) return dl;
        json_free(dl);
        a = fa15_args();
        static const char *keys[5] = {"chat_id", "animation_url", "caption", "reply_to", "metadata"};
        for (int i = 0; i < 5; i++) fa15_set_copy(a, keys[i], json_obj_get(args, keys[i]));
        return fa15_call(ctx, "super_send_animation", a);
    }
    const char *fp = fa15_str_or(json_get(dl, 0), "");
    const char *fn = fa15_str_or(json_get(dl, 1), "");
    bool hascap = fa15_truthy(json_obj_get(args, "caption"));
    json_t *ta = fa15_args();
    fa15_set_s(ta, "key", hascap ? "platform.feishu.media.gif_downgraded_caption"
                                  : "platform.feishu.media.gif_downgraded");
    fa15_set_copy(ta, "caption", json_obj_get(args, "caption"));
    json_t *txt = fa15_call(ctx, "t_anim", ta);
    if (fa15_is_raise(txt)) { json_free(dl); return txt; }
    a = fa15_args(); fa15_set_copy(a, "text", txt);
    fa15_set_copy(a, "caption", json_obj_get(args, "caption")); json_free(txt);
    json_t *cap = fa15_call(ctx, "warning_text", a);
    if (fa15_is_raise(cap)) { json_free(dl); return cap; }
    a = fa15_args();
    fa15_set_copy(a, "chat_id", json_obj_get(args, "chat_id"));
    fa15_set_s(a, "file_path", fp); fa15_set_s(a, "file_name", fn);
    fa15_set_copy(a, "caption", cap); json_free(cap);
    fa15_set_copy(a, "reply_to", json_obj_get(args, "reply_to"));
    fa15_set_copy(a, "metadata", json_obj_get(args, "metadata"));
    json_free(dl);
    return fa15_call(ctx, "send_document", a);
}

/* PoP: fa15_qr_register @ plugins/platforms/feishu/adapter.py:qr_register */
static json_t *fa15_qr_register(fa15_ctx_t *ctx, const json_t *args) {
    json_t *a = fa15_args();
    fa15_set_s(a, "initial_domain", fa15_str_or(json_obj_get(args, "initial_domain"), "feishu"));
    const json_t *ts = json_obj_get(args, "timeout_seconds");
    fa15_set_n(a, "timeout_seconds", (ts && ts->type == JSON_NUMBER) ? ts->num_val : 600);
    json_t *r = fa15_call(ctx, "qr_inner", a);
    if (!fa15_is_raise(r)) return r;
    if (fa15_qr_catch(json_get_str(r, "__raise__", ""))) { json_free(r); return json_null(); }
    return r;
}

/* PoP: fa15__teardown_ws_thread @ plugins/platforms/feishu/adapter.py:FeishuAdapter._teardown_ws_thread */
static json_t *fa15__teardown_ws_thread(fa15_ctx_t *ctx, const json_t *args) {
    bool loop_present = fa15_truthy(json_obj_get(args, "loop_present"));
    bool loop_alive = false;
    if (loop_present) {
        json_t *lc = fa15_call(ctx, "loop_is_closed", fa15_args());
        if (fa15_is_raise(lc)) return lc;
        loop_alive = !fa15_truthy(lc); json_free(lc);
    }
    bool ws_present = fa15_truthy(json_obj_get(args, "ws_client_present"));
    bool has_disc = fa15_truthy(json_obj_get(args, "ws_has_disconnect"));
    if (ws_present && loop_alive && has_disc) {
        json_t *d = fa15_call(ctx, "ws_disconnect", fa15_args());
        json_t *r;
        if (fa15_is_raise(d)) {
            if (fa15_raise_is_base(d)) return d;
            json_free(d);
        } else {
            json_free(d);
            r = fa15_call(ctx, "close_frame", fa15_args());
            if (fa15_is_raise(r)) {
                if (fa15_raise_is_base(r)) return r;
                json_free(r);
            } else json_free(r);
        }
    }
    if (loop_alive) {
        json_t *r = fa15_call(ctx, "call_soon_threadsafe", fa15_args());
        if (fa15_is_raise(r)) return r;
        json_free(r);
        r = fa15_call(ctx, "all_tasks", fa15_args());
        if (fa15_is_raise(r)) return r;
        json_t *pending = json_array();
        if (!fa15_is_raise(r) && r->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(r); i++) {
                const json_t *t = json_get(r, i);
                json_t *a = fa15_args(); fa15_set_copy(a, "task", t);
                json_t *d = fa15_call(ctx, "task_done", a);
                if (fa15_is_raise(d)) { json_free(pending); json_free(r); return d; }
                bool done = fa15_truthy(d); json_free(d);
                if (!done) json_append(pending, json_copy(t));
            }
        }
        json_free(r);
        for (size_t i = 0; i < json_len(pending); i++) {
            json_t *a = fa15_args(); fa15_set_copy(a, "task", json_get(pending, i));
            json_t *c = fa15_call(ctx, "task_cancel", a);
            if (fa15_is_raise(c)) { json_free(pending); return c; }
            json_free(c);
        }
        json_free(pending);
        json_t *clargs = fa15_args(); fa15_set_n(clargs, "delay", 0.1);
        json_t *cl = fa15_call(ctx, "call_later", clargs);
        if (fa15_is_raise(cl)) return cl;
        json_free(cl);
    }
    if (fa15_truthy(json_obj_get(args, "ws_future_present"))) {
        json_t *r = fa15_call(ctx, "wait_ws_future", fa15_args());
        if (fa15_is_raise(r)) {
            const char *kind = json_get_str(r, "__raise__", "");
            if (!fa15_teardown_wait_catch(kind)) return r;
            json_free(r);
        } else json_free(r);
    }
    return json_null();
}
