/*
 * feishu_adapter_flow5.c — Feishu/Lark adapter Lark-API request-builder and
 * send-result FLOW cores of plugins/platforms/feishu/adapter.py (hermes-agent).
 * FIFTH remainder port beyond feishu_adapter_flow.c (message normalization),
 * feishu_adapter_flow2.c (inbound-event / card-action),
 * feishu_adapter_flow3.c (inbound admission / mention gating) and
 * feishu_adapter_flow4.c (SDK-build / dependency gates + the two request
 * wrappers _build_get_chat_request / _build_file_upload_body).
 *
 * This file ports 18 GENUINELY UNCLAIMED defs (adapter.py L3720-L3744 and
 * L3936-L4019), a coherent "outbound request/response building" slice:
 *   13 Lark request builders (each one _sdk_build seam):
 *     _build_get_message_request, _build_message_resource_request,
 *     _build_get_application_request, _build_reply_message_body,
 *     _build_reply_message_request, _build_update_message_body,
 *     _build_update_message_request, _build_delete_message_request,
 *     _build_create_message_body, _build_create_message_request,
 *     _build_image_upload_body, _build_image_upload_request,
 *     _build_file_upload_request
 *   4 send-result helpers:
 *     _response_succeeded, _extract_response_field, _response_error_result,
 *     _finalize_send_result
 *   1 pure outbound-routing helper:
 *     _resolve_outbound_file_routing
 * NO def claimed by flows 1-4 is re-ported. Claimed siblings _sdk_build,
 * _response_succeeded, _response_error_result and _extract_response_field are
 * SEAMED when composed (a ported sibling is real only when it is itself the
 * case target).
 *
 * Universal seam: every seamed call enters through ONE callback
 * fa5_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting
 * the identical event shape {"name","args","ret"} from the SAME scripts. args
 * is ADOPTED into the trace event; the return is a NEW owned json_t (NULL ==
 * Python None; {"__raise__":cls,"message":str(exc)} == a raised exception). A
 * missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * FULL BUILTIN EXCEPTION HIERARCHY / BASEEXCEPTION PASSTHROUGH: none of these
 * defs has a try/except, so every scripted raise propagates verbatim (the C
 * returns the raise descriptor; the oracle's BaseException catch round-trips
 * the scripted spec). fa5_is_base_exc() names the family that bypasses an
 * `except Exception`; the oracle's EXC_CLASSES table mirrors the full builtin
 * hierarchy so make_exc() materializes the exact class for every scripted kind.
 *
 * Representation contract (shared with tests/oracle_feishu_adapter_flow5.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__":cls,"message":str(exc)}; a scripted raise is
 *   {"__script__":true,"raise":{"type":cls,"message":msg}}.
 * - Lark request classes are marker strings known to both sides (e.g.
 *   "GetMessageRequest", "ReplyMessageRequestBody"). `fields` travels as an
 *   ordered array of {"name","value"} pairs (libjson preserves parse order;
 *   live Python **fields preserves dict order).
 * - A response is null or an object with scalar attributes success/data/code/
 *   msg. `success` is the RESULT of the live getattr(...success...)() call
 *   (a bool in the fixture domain); `data` is an object or null. Domain:
 *   response is null or a non-empty object; success is a bool; code/msg are
 *   strings when present. raw_response echoes this same object verbatim.
 * - SendResult projects to its dataclasses.asdict() field dict (8 fields:
 *   success, message_id, error, raw_response, retryable, retry_after,
 *   continuation_message_ids=[], error_kind).
 * - A filename for _resolve_outbound_file_routing is a normal single-extension
 *   name (no trailing-dot / leading-dot-only edge cases); suffix is the lower-
 *   cased last extension.
 * - String slots are otherwise str/null/bool/int. State emission (out.state)
 *   is {} for every func (no observable mutation).
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and fa5_-prefixed.
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
typedef struct fa5_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} fa5_ctx_t;

/* ── basic helpers ── */
static bool fa5_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
/* The BaseException family that bypasses `except Exception`. */
static bool fa5_is_base_exc(const char *kind) {
    return kind && (
        strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0);
}
static bool fa5_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *fa5_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static const char *fa5_str_or_empty(const json_t *v) {
    const char *s = fa5_str(v);
    return s ? s : "";
}
static json_t *fa5_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *fa5_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    (void)fa5_is_base_exc(type);
    return fa5_err(type, msg);
}
/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *fa5_call(fa5_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = fa5_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = fa5_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = fa5_make_exc(raise);
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
static json_t *fa5_args(void) { return json_object(); }
static void fa5_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void fa5_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void fa5_add_pair(json_t *pairs, const char *name, const json_t *v) {
    json_t *p = json_object();
    fa5_set_s(p, "name", name);
    fa5_set_copy(p, "value", v);
    json_append(pairs, p);
}
static void fa5_add_pair_s(json_t *pairs, const char *name, const char *v) {
    json_t *p = json_object();
    fa5_set_s(p, "name", name);
    fa5_set_s(p, "value", v);
    json_append(pairs, p);
}
/* Build the _sdk_build seam args and fire the single seam. pairs is adopted. */
static json_t *fa5_build_req(fa5_ctx_t *ctx, const char *cls, json_t *pairs) {
    json_t *a = fa5_args();
    fa5_set_s(a, "cls", cls);
    json_set(a, "fields", pairs);
    return fa5_call(ctx, "_sdk_build", a);
}

/* ── Lark API request builders (13; each = one _sdk_build seam) ── */

/* PoP: fa5__build_get_message_request @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_get_message_request */
static json_t *fa5__build_get_message_request(fa5_ctx_t *ctx, const json_t *args) {
    json_t *pairs = json_array();
    fa5_add_pair_s(pairs, "message_id", fa5_str_or_empty(json_obj_get(args, "message_id")));
    return fa5_build_req(ctx, "GetMessageRequest", pairs);
}

/* PoP: fa5__build_message_resource_request @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_message_resource_request */
static json_t *fa5__build_message_resource_request(fa5_ctx_t *ctx, const json_t *args) {
    json_t *pairs = json_array();
    fa5_add_pair_s(pairs, "message_id", fa5_str_or_empty(json_obj_get(args, "message_id")));
    fa5_add_pair_s(pairs, "file_key", fa5_str_or_empty(json_obj_get(args, "file_key")));
    fa5_add_pair_s(pairs, "type", fa5_str_or_empty(json_obj_get(args, "resource_type")));
    return fa5_build_req(ctx, "GetMessageResourceRequest", pairs);
}

/* PoP: fa5__build_get_application_request @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_get_application_request */
static json_t *fa5__build_get_application_request(fa5_ctx_t *ctx, const json_t *args) {
    json_t *pairs = json_array();
    fa5_add_pair_s(pairs, "app_id", fa5_str_or_empty(json_obj_get(args, "app_id")));
    fa5_add_pair_s(pairs, "lang", fa5_str_or_empty(json_obj_get(args, "lang")));
    return fa5_build_req(ctx, "GetApplicationRequest", pairs);
}

/* PoP: fa5__build_reply_message_body @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_reply_message_body */
static json_t *fa5__build_reply_message_body(fa5_ctx_t *ctx, const json_t *args) {
    json_t *pairs = json_array();
    fa5_add_pair_s(pairs, "content", fa5_str_or_empty(json_obj_get(args, "content")));
    fa5_add_pair_s(pairs, "msg_type", fa5_str_or_empty(json_obj_get(args, "msg_type")));
    fa5_add_pair(pairs, "reply_in_thread", json_obj_get(args, "reply_in_thread"));
    fa5_add_pair_s(pairs, "uuid", fa5_str_or_empty(json_obj_get(args, "uuid_value")));
    return fa5_build_req(ctx, "ReplyMessageRequestBody", pairs);
}

/* PoP: fa5__build_reply_message_request @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_reply_message_request */
static json_t *fa5__build_reply_message_request(fa5_ctx_t *ctx, const json_t *args) {
    json_t *pairs = json_array();
    fa5_add_pair_s(pairs, "message_id", fa5_str_or_empty(json_obj_get(args, "message_id")));
    fa5_add_pair(pairs, "request_body", json_obj_get(args, "request_body"));
    return fa5_build_req(ctx, "ReplyMessageRequest", pairs);
}

/* PoP: fa5__build_update_message_body @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_update_message_body */
static json_t *fa5__build_update_message_body(fa5_ctx_t *ctx, const json_t *args) {
    json_t *pairs = json_array();
    fa5_add_pair_s(pairs, "msg_type", fa5_str_or_empty(json_obj_get(args, "msg_type")));
    fa5_add_pair_s(pairs, "content", fa5_str_or_empty(json_obj_get(args, "content")));
    return fa5_build_req(ctx, "UpdateMessageRequestBody", pairs);
}

/* PoP: fa5__build_update_message_request @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_update_message_request */
static json_t *fa5__build_update_message_request(fa5_ctx_t *ctx, const json_t *args) {
    json_t *pairs = json_array();
    fa5_add_pair_s(pairs, "message_id", fa5_str_or_empty(json_obj_get(args, "message_id")));
    fa5_add_pair(pairs, "request_body", json_obj_get(args, "request_body"));
    return fa5_build_req(ctx, "UpdateMessageRequest", pairs);
}

/* PoP: fa5__build_delete_message_request @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_delete_message_request */
static json_t *fa5__build_delete_message_request(fa5_ctx_t *ctx, const json_t *args) {
    json_t *pairs = json_array();
    fa5_add_pair_s(pairs, "message_id", fa5_str_or_empty(json_obj_get(args, "message_id")));
    return fa5_build_req(ctx, "DeleteMessageRequest", pairs);
}

/* PoP: fa5__build_create_message_body @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_create_message_body */
static json_t *fa5__build_create_message_body(fa5_ctx_t *ctx, const json_t *args) {
    json_t *pairs = json_array();
    fa5_add_pair_s(pairs, "receive_id", fa5_str_or_empty(json_obj_get(args, "receive_id")));
    fa5_add_pair_s(pairs, "msg_type", fa5_str_or_empty(json_obj_get(args, "msg_type")));
    fa5_add_pair_s(pairs, "content", fa5_str_or_empty(json_obj_get(args, "content")));
    fa5_add_pair_s(pairs, "uuid", fa5_str_or_empty(json_obj_get(args, "uuid_value")));
    return fa5_build_req(ctx, "CreateMessageRequestBody", pairs);
}

/* PoP: fa5__build_create_message_request @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_create_message_request */
static json_t *fa5__build_create_message_request(fa5_ctx_t *ctx, const json_t *args) {
    json_t *pairs = json_array();
    fa5_add_pair_s(pairs, "receive_id_type", fa5_str_or_empty(json_obj_get(args, "receive_id_type")));
    fa5_add_pair(pairs, "request_body", json_obj_get(args, "request_body"));
    return fa5_build_req(ctx, "CreateMessageRequest", pairs);
}

/* PoP: fa5__build_image_upload_body @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_image_upload_body */
static json_t *fa5__build_image_upload_body(fa5_ctx_t *ctx, const json_t *args) {
    json_t *pairs = json_array();
    fa5_add_pair_s(pairs, "image_type", fa5_str_or_empty(json_obj_get(args, "image_type")));
    fa5_add_pair(pairs, "image", json_obj_get(args, "image"));
    return fa5_build_req(ctx, "CreateImageRequestBody", pairs);
}

/* PoP: fa5__build_image_upload_request @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_image_upload_request */
static json_t *fa5__build_image_upload_request(fa5_ctx_t *ctx, const json_t *args) {
    json_t *pairs = json_array();
    fa5_add_pair(pairs, "request_body", json_obj_get(args, "request_body"));
    return fa5_build_req(ctx, "CreateImageRequest", pairs);
}

/* PoP: fa5__build_file_upload_request @ plugins/platforms/feishu/adapter.py:FeishuAdapter._build_file_upload_request */
static json_t *fa5__build_file_upload_request(fa5_ctx_t *ctx, const json_t *args) {
    json_t *pairs = json_array();
    fa5_add_pair(pairs, "request_body", json_obj_get(args, "request_body"));
    return fa5_build_req(ctx, "CreateFileRequest", pairs);
}

/* ── send-result helpers ── */

/* PoP: fa5__response_succeeded @ plugins/platforms/feishu/adapter.py:FeishuAdapter._response_succeeded */
static json_t *fa5__response_succeeded(fa5_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *resp = json_obj_get(args, "response");
    if (!resp || resp->type == JSON_NULL) return json_bool(false);
    if (resp->type != JSON_OBJECT) return json_bool(false);
    /* getattr(response, "success", lambda: False)() -> fa5_truthy(result). */
    const json_t *success = json_obj_get(resp, "success");
    return json_bool(success ? fa5_truthy(success) : false);
}

/* PoP: fa5__extract_response_field @ plugins/platforms/feishu/adapter.py:FeishuAdapter._extract_response_field */
static json_t *fa5__extract_response_field(fa5_ctx_t *ctx, const json_t *args) {
    const json_t *resp = json_obj_get(args, "response");
    json_t *sa = fa5_args();
    fa5_set_copy(sa, "response", resp);
    json_t *ok = fa5_call(ctx, "_response_succeeded", sa);
    if (fa5_is_raise(ok)) return ok;
    bool succeeded = fa5_truthy(ok);
    json_free(ok);
    if (!succeeded) return json_null();
    const json_t *data = (resp && resp->type == JSON_OBJECT)
                         ? json_obj_get(resp, "data") : NULL;
    if (!data || data->type == JSON_NULL) return json_null();
    if (data->type != JSON_OBJECT) return json_null();
    const char *field = fa5_str_or_empty(json_obj_get(args, "field_name"));
    json_t *value = json_obj_get(data, field);
    return value ? json_copy(value) : json_null();
}

/* getattr-style string read: absent -> default; present null -> "None";
 * present string -> value; present bool/number -> py-style scalar. */
static const char *fa5_getattr_str(const json_t *obj, const char *key,
                                   const char *fallback, char *buf, size_t cap) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (!v) return fallback;
    if (v->type == JSON_NULL) { snprintf(buf, cap, "None"); return buf; }
    if (v->type == JSON_STRING) return v->str_val;
    if (v->type == JSON_BOOL) { snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return buf; }
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, cap, "%.0f", v->num_val);
        else
            snprintf(buf, cap, "%g", v->num_val);
        return buf;
    }
    return fallback;
}

/* Build the full SendResult dataclass projection (asdict field order). */
static json_t *fa5_send_result(bool success, const json_t *message_id,
                               const json_t *error, const json_t *raw_response) {
    json_t *r = json_object();
    json_set(r, "success", json_bool(success));
    json_set(r, "message_id", message_id ? json_copy(message_id) : json_null());
    json_set(r, "error", error ? json_copy(error) : json_null());
    json_set(r, "raw_response", raw_response ? json_copy(raw_response) : json_null());
    json_set(r, "retryable", json_bool(false));
    json_set(r, "retry_after", json_null());
    json_set(r, "continuation_message_ids", json_array());
    json_set(r, "error_kind", json_null());
    return r;
}

/* PoP: fa5__response_error_result @ plugins/platforms/feishu/adapter.py:FeishuAdapter._response_error_result */
static json_t *fa5__response_error_result(fa5_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *resp = json_obj_get(args, "response");
    const json_t *override = json_obj_get(args, "override_error");
    const char *dm = fa5_str_or_empty(json_obj_get(args, "default_message"));
    json_t *err;
    if (fa5_truthy(override)) {
        err = json_copy(override);
    } else {
        char cb[64], mb[512], fb[512];
        const char *code = fa5_getattr_str(resp, "code", "unknown", cb, sizeof cb);
        const char *msg = fa5_getattr_str(resp, "msg", dm, mb, sizeof mb);
        snprintf(fb, sizeof fb, "[%s] %s", code, msg);
        err = json_string(fb);
    }
    return fa5_send_result(false, NULL, err, resp);
}

/* PoP: fa5__finalize_send_result @ plugins/platforms/feishu/adapter.py:FeishuAdapter._finalize_send_result */
static json_t *fa5__finalize_send_result(fa5_ctx_t *ctx, const json_t *args) {
    const json_t *resp = json_obj_get(args, "response");
    const char *dm = fa5_str_or_empty(json_obj_get(args, "default_message"));
    json_t *sa = fa5_args();
    fa5_set_copy(sa, "response", resp);
    json_t *ok = fa5_call(ctx, "_response_succeeded", sa);
    if (fa5_is_raise(ok)) return ok;
    bool succeeded = fa5_truthy(ok);
    json_free(ok);
    if (succeeded) {
        json_t *ea = fa5_args();
        fa5_set_copy(ea, "response", resp);
        fa5_set_s(ea, "field_name", "message_id");
        json_t *mid = fa5_call(ctx, "_extract_response_field", ea);
        if (fa5_is_raise(mid)) return mid;
        json_t *r = fa5_send_result(true, mid, NULL, resp);
        json_free(mid);
        return r;
    }
    json_t *ea = fa5_args();
    fa5_set_copy(ea, "response", resp);
    fa5_set_s(ea, "default_message", dm);
    return fa5_call(ctx, "_response_error_result", ea);
}

/* PoP: fa5__resolve_outbound_file_routing @ plugins/platforms/feishu/adapter.py:FeishuAdapter._resolve_outbound_file_routing */
static json_t *fa5__resolve_outbound_file_routing(fa5_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *path = fa5_str_or_empty(json_obj_get(args, "file_path"));
    const char *req = fa5_str_or_empty(json_obj_get(args, "requested_message_type"));
    /* pathlib suffix: lower-cased last extension of the basename (leading dot
     * does not count; trailing dot -> empty). Fixture domain is a normal name. */
    const char *base = path;
    for (const char *p = path; *p; p++) if (*p == '/') base = p + 1;
    char suffix[128] = "";
    const char *dot = NULL;
    for (const char *p = base; *p; p++) if (*p == '.') dot = p;
    if (dot && dot > base && dot[1] != '\0') {
        size_t n = strlen(dot);
        if (n >= sizeof suffix) n = sizeof suffix - 1;
        for (size_t i = 0; i < n; i++) {
            char c = dot[i];
            suffix[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
        }
        suffix[n] = '\0';
    }
    json_t *r = json_array();
    if ((strcmp(suffix, ".ogg") == 0 || strcmp(suffix, ".opus") == 0)
            && strcmp(req, "audio") == 0) {
        json_append(r, json_string("opus"));
        json_append(r, json_string("audio"));
        return r;
    }
    if (strcmp(suffix, ".mp4") == 0 || strcmp(suffix, ".mov") == 0
            || strcmp(suffix, ".avi") == 0 || strcmp(suffix, ".m4v") == 0) {
        json_append(r, json_string("mp4"));
        json_append(r, json_string("media"));
        return r;
    }
    const char *doc = NULL;
    if (strcmp(suffix, ".pdf") == 0) doc = "pdf";
    else if (strcmp(suffix, ".doc") == 0 || strcmp(suffix, ".docx") == 0) doc = "doc";
    else if (strcmp(suffix, ".xls") == 0 || strcmp(suffix, ".xlsx") == 0) doc = "xls";
    else if (strcmp(suffix, ".ppt") == 0 || strcmp(suffix, ".pptx") == 0) doc = "ppt";
    json_append(r, json_string(doc ? doc : "stream"));
    json_append(r, json_string("file"));
    return r;
}
