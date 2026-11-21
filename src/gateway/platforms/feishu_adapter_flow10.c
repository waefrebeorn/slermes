/*
 * feishu_adapter_flow10.c — Feishu/Lark adapter "remainders" FLOW cores of
 * plugins/platforms/feishu/adapter.py (hermes-agent).
 *
 * TENTH remainder port beyond feishu_adapter_flow.c .. flow9. Ports 16
 * genuinely unclaimed defs (dotted-name-aware claim parser confirmed none are
 * claimed by flows 1-9):
 *   _fetch_bot_names, _fetch_last_message_in_thread, _fetch_message_resource,
 *   _fetch_message_text, _flush_media_batch, _flush_media_batch_now,
 *   _get_audio_duration_ms, _get_chat_lock, _get_sdk_executor,
 *   _handle_message_event_data, _handle_message_with_guards,
 *   _handle_webhook_request, _clear_webhook_anomaly, _ws_link_up,
 *   _ws_link_retrying, _submit_if_ready.
 *
 * Universal seam: every seamed call enters through ONE callback
 * fa10_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting the
 * identical {"name","args","ret"} event from the SAME scripts. args is ADOPTED
 * into the trace event; the return is a NEW owned json_t (NULL == Python None;
 * {"__raise__":cls,"message":...} == a raised exception). A missing/exhausted
 * script is a fail-closed RuntimeError on both sides.
 *
 * FULL BASEEXCEPTION PASSTHROUGH: fa10_is_base_exc + fa10_raise_is_base name the
 * family that bypasses `except Exception`; those raises propagate verbatim.
 *
 * Representation contract (shared with tests/oracle_feishu_adapter_flow10.py):
 * - NULL json_t* == Python None; JSON_NULL node is a real null value.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__":cls,"message":str(exc)}; a scripted raise is
 *   {"__script__":true,"raise":{"type":cls,"message":msg}}.
 * - Carrier fields are read directly from args doubles (oracle materializes
 *   equivalent attribute doubles). Fixture domain: ASCII strings, scalars,
 *   JSON dict/list values, POSIX paths, integral-int numbers, byte lists.
 *   Out.state is {}.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and fa10_-prefixed.
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
typedef struct fa10_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} fa10_ctx_t;

/* ── basic helpers ── */
static bool fa10_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool fa10_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool fa10_is_base_exc(const char *kind) {
    return kind && (
        strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "asyncio.CancelledError") == 0);
}
static bool fa10_raise_is_base(const json_t *r) {
    return fa10_is_raise(r) && fa10_is_base_exc(json_get_str(r, "__raise__", ""));
}
static double fa10_as_num(const json_t *v, double d) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : d;
}
static bool fa10_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *fa10_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static double fa10_num(const json_t *v, double d) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : d;
}
static bool fa10_bool(const json_t *v, bool d) {
    return (v && v->type == JSON_BOOL) ? v->bool_val : d;
}
static const char *fa10_str_or(const json_t *v, const char *d) {
    const char *s = fa10_str(v);
    return s ? s : d;
}
static json_t *fa10_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *fa10_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return fa10_err(type, msg);
}
/* The one universal seam. args is ADOPTED; the return is a fresh owned node. */
static json_t *fa10_call(fa10_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = fa10_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = fa10_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = fa10_make_exc(raise);
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
static json_t *fa10_args(void) { return json_object(); }
static void fa10_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void fa10_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
/* Python str.strip() for the ASCII fixture domain. */
static char *fa10_strip_inplace(const char *s) {
    static char b[4096];
    size_t n = 0;
    const char *p = s ? s : "";
    while (*p && n + 1 < sizeof b) { b[n++] = *p++; }
    b[n] = '\0';
    size_t start = 0, end = n;
    while (start < end && (b[start] == ' ' || b[start] == '\t' || b[start] == '\n' ||
                           b[start] == '\r' || b[start] == '\v' || b[start] == '\f')) start++;
    while (end > start && (b[end-1] == ' ' || b[end-1] == '\t' || b[end-1] == '\n' ||
                           b[end-1] == '\r' || b[end-1] == '\v' || b[end-1] == '\f')) end--;
    b[end] = '\0';
    return b + start;
}
/* Lowercase ASCII in place. */
static char *fa10_lower_inplace(const char *s) {
    static char b[4096];
    size_t n = 0;
    const char *p = s ? s : "";
    while (*p && n + 1 < sizeof b) { char c = *p++; b[n++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
    b[n] = '\0';
    return b;
}
/* Python str(x) for the scalar fixture domain. */
static const char *fa10_pystr(const json_t *v, char *buf, size_t cap) {
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
/* Deep equality of JSON values (Python == for the scalar/container fixture domain). */
static bool fa10_json_equal(const json_t *a, const json_t *b) {
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
                if (!fa10_json_equal(json_get(a, i), json_get(b, i))) return false;
            return true;
        case JSON_OBJECT:
            if (json_object_size(a) != json_object_size(b)) return false;
            for (size_t i = 0; i < json_object_size(a); i++) {
                const char *k = json_object_get_key_at(a, i);
                if (!fa10_json_equal(json_object_get_at(a, i), json_obj_get(b, k))) return false;
            }
            return true;
    }
    return false;
}

/* ══ PoP functions ══ */

/* PoP: fa10_fetch_bot_names @ plugins/platforms/feishu/adapter.py:FeishuAdapter._fetch_bot_names */
static json_t *fa10_fetch_bot_names(fa10_ctx_t *ctx, const json_t *args) {
    bool has_client = fa10_truthy(json_obj_get(args, "client"));
    const json_t *ids = json_obj_get(args, "bot_ids");
    if (!has_client || !ids || ids->type != JSON_ARRAY || json_len(ids) == 0) return json_null();
    json_t *qa = fa10_args();
    fa10_set_s(qa, "path", "/open-apis/bot/v3/bots/basic_batch");
    json_t *queries = json_array();
    for (size_t i = 0; i < json_len(ids); i++) {
        json_t *pair = json_array();
        json_append(pair, json_string("bot_ids"));
        json_append(pair, json_copy(json_get(ids, i)));
        json_append(queries, pair);
    }
    json_set(qa, "queries", queries);
    json_t *content = fa10_call(ctx, "tenant_get_raw", qa);
    if (fa10_is_raise(content)) {
        if (fa10_raise_is_base(content)) return content;
        json_free(content); return json_null();
    }
    const char *cs = fa10_str(content);
    json_t *payload = cs ? json_parse(cs, NULL) : NULL;
    json_free(content);
    if (!payload || payload->type != JSON_OBJECT) { if (payload) json_free(payload); return json_null(); }
    double code = json_get_num(payload, "code", -1);
    if (code != 0) { json_free(payload); return json_null(); }
    const json_t *data = json_obj_get(payload, "data");
    const json_t *bots = (data && data->type == JSON_OBJECT) ? json_obj_get(data, "bots") : NULL;
    if (!bots || bots->type != JSON_OBJECT) { json_free(payload); return json_null(); }
    json_t *out = json_object();
    for (size_t i = 0; i < json_object_size(bots); i++) {
        const char *oid = json_object_get_key_at(bots, i);
        if (!oid || !oid[0]) continue;
        const json_t *info = json_object_get_at(bots, i);
        const char *nm = (info && info->type == JSON_OBJECT) ? json_get_str(info, "name", "") : "";
        const char *stripped = fa10_strip_inplace(nm);
        json_set_hdr(out, oid, stripped);
    }
    json_free(payload);
    return out;
}

/* PoP: fa10_fetch_last_message_in_thread @ plugins/platforms/feishu/adapter.py:FeishuAdapter._fetch_last_message_in_thread */
static json_t *fa10_fetch_last_message_in_thread(fa10_ctx_t *ctx, const json_t *args) {
    bool has_client = fa10_truthy(json_obj_get(args, "client"));
    const char *tid = fa10_str_or(json_obj_get(args, "thread_id"), "");
    if (!has_client || !tid[0]) return json_null();
    json_t *qa = fa10_args(); fa10_set_s(qa, "thread_id", tid);
    json_t *resp = fa10_call(ctx, "list_messages", qa);
    if (fa10_is_raise(resp)) {
        if (fa10_raise_is_base(resp)) return resp;
        json_free(resp); return json_null();
    }
    json_t *sa = fa10_args();
    json_t *ok = fa10_call(ctx, "response_succeeded", sa);
    if (fa10_is_raise(ok)) {
        if (fa10_raise_is_base(ok)) { json_free(resp); return ok; }
        json_free(ok); json_free(resp); return json_null();
    }
    bool succeeded = fa10_truthy(ok);
    json_free(ok);
    json_t *result = json_null();
    if (succeeded) {
        const json_t *data = (resp && resp->type == JSON_OBJECT) ? json_obj_get(resp, "data") : NULL;
        const json_t *items = (data && data->type == JSON_OBJECT) ? json_obj_get(data, "items") : NULL;
        if (items && items->type == JSON_ARRAY && json_len(items) > 0) {
            const json_t *first = json_get(items, 0);
            const json_t *mid = (first && first->type == JSON_OBJECT) ? json_obj_get(first, "message_id") : NULL;
            result = mid ? json_copy(mid) : json_null();
        }
    }
    json_free(resp);
    return result;
}

/* PoP: fa10_fetch_message_resource @ plugins/platforms/feishu/adapter.py:FeishuAdapter._fetch_message_resource */
static json_t *fa10_fetch_message_resource(fa10_ctx_t *ctx, const json_t *args) {
    const json_t *mid = json_obj_get(args, "message_id");
    const json_t *fk = json_obj_get(args, "file_key");
    const json_t *rt = json_obj_get(args, "resource_type");
    json_t *ba = fa10_args();
    fa10_set_copy(ba, "message_id", mid); fa10_set_copy(ba, "file_key", fk); fa10_set_copy(ba, "resource_type", rt);
    json_t *req = fa10_call(ctx, "build_resource_request", ba);
    if (fa10_is_raise(req)) return req; /* no try/except: propagate all */
    json_free(req);
    return fa10_call(ctx, "fetch_resource", fa10_args());
}

/* PoP: fa10_fetch_message_text @ plugins/platforms/feishu/adapter.py:FeishuAdapter._fetch_message_text */
static json_t *fa10_fetch_message_text(fa10_ctx_t *ctx, const json_t *args) {
    bool has_client = fa10_truthy(json_obj_get(args, "client"));
    const json_t *midv = json_obj_get(args, "message_id");
    const char *mid = fa10_str_or(midv, "");
    if (!has_client || !mid[0]) return json_null();
    /* cache membership probe */
    json_t *ca = fa10_args(); fa10_set_s(ca, "message_id", mid);
    json_t *hit = fa10_call(ctx, "cache_contains", ca);
    if (fa10_is_raise(hit)) return hit; /* cache probe is outside Python's try */
    bool in_cache = fa10_truthy(hit);
    json_free(hit);
    if (in_cache) {
        json_t *ma = fa10_args(); fa10_set_s(ma, "message_id", mid);
        json_t *mv = fa10_call(ctx, "cache_move_end", ma);
        if (fa10_is_raise(mv)) return mv;
        json_free(mv);
        json_t *ga = fa10_args(); fa10_set_s(ga, "message_id", mid);
        json_t *val = fa10_call(ctx, "cache_get", ga);
        if (fa10_is_raise(val)) return val;
        return val;
    }
    /* request build + blocking get */
    json_t *ba = fa10_args(); fa10_set_s(ba, "message_id", mid);
    json_t *req = fa10_call(ctx, "build_get_message_request", ba);
    if (fa10_is_raise(req)) {
        if (fa10_raise_is_base(req)) return req;
        json_free(req); return json_null();
    }
    json_free(req);
    json_t *resp = fa10_call(ctx, "fetch_message", fa10_args());
    if (fa10_is_raise(resp)) {
        if (fa10_raise_is_base(resp)) return resp;
        json_free(resp); return json_null();
    }
    json_t *sa = fa10_args();
    json_t *ok = fa10_call(ctx, "response_succeeded", sa);
    if (fa10_is_raise(ok)) {
        if (fa10_raise_is_base(ok)) { json_free(resp); return ok; }
        json_free(ok); json_free(resp); return json_null();
    }
    bool succeeded = fa10_truthy(ok);
    json_free(ok);
    if (!succeeded) { json_free(resp); return json_null(); }
    const json_t *data = (resp && resp->type == JSON_OBJECT) ? json_obj_get(resp, "data") : NULL;
    const json_t *items = (data && data->type == JSON_OBJECT) ? json_obj_get(data, "items") : NULL;
    const json_t *parent = (items && items->type == JSON_ARRAY && json_len(items) > 0) ? json_get(items, 0) : NULL;
    const json_t *body = (parent && parent->type == JSON_OBJECT) ? json_obj_get(parent, "body") : NULL;
    const char *msg_type = parent ? fa10_str_or(json_obj_get(parent, "msg_type"), "") : "";
    const char *raw = body ? fa10_str_or(json_obj_get(body, "content"), "") : "";
    const json_t *mentions = parent ? json_obj_get(parent, "mentions") : NULL;
    json_t *ea = fa10_args();
    fa10_set_s(ea, "msg_type", msg_type); fa10_set_s(ea, "raw_content", raw); fa10_set_copy(ea, "mentions", mentions);
    json_t *text = fa10_call(ctx, "extract_text", ea);
    if (fa10_is_raise(text)) {
        if (fa10_raise_is_base(text)) { json_free(resp); return text; }
        json_free(text); json_free(resp); return json_null();
    }
    json_free(resp);
    /* cache store */
    json_t *sa2 = fa10_args(); fa10_set_s(sa2, "message_id", mid); fa10_set_copy(sa2, "value", text);
    json_t *st = fa10_call(ctx, "cache_set", sa2);
    if (fa10_is_raise(st)) {
        if (fa10_raise_is_base(st)) { json_free(text); return st; }
        json_free(st);
    } else json_free(st);
    /* trim to max size */
    int iter = 0;
    while (iter++ < 4096) {
        json_t *la = fa10_args();
        json_t *len = fa10_call(ctx, "cache_len", la);
        if (fa10_is_raise(len)) {
            if (fa10_raise_is_base(len)) { json_free(text); return len; }
            json_free(len); break;
        }
        double n = fa10_as_num(len, 0);
        json_free(len);
        double cap = fa10_num(json_obj_get(args, "cache_size"), 512);
        if (!(n > cap)) break;
        json_t *pa = fa10_args();
        json_t *popped = fa10_call(ctx, "cache_popitem", pa);
        if (fa10_is_raise(popped)) {
            if (fa10_raise_is_base(popped)) { json_free(text); return popped; }
            json_free(popped); break;
        }
        json_free(popped);
    }
    return text;
}

/* PoP: fa10_flush_media_batch @ plugins/platforms/feishu/adapter.py:FeishuAdapter._flush_media_batch */
static json_t *fa10_flush_media_batch(fa10_ctx_t *ctx, const json_t *args) {
    json_t *da = fa10_args();
    fa10_set_copy(da, "key", json_obj_get(args, "key"));
    json_set(da, "delay", json_number(fa10_num(json_obj_get(args, "delay"), 0.0)));
    json_t *r = fa10_call(ctx, "delayed_flush", da);
    if (fa10_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: fa10_flush_media_batch_now @ plugins/platforms/feishu/adapter.py:FeishuAdapter._flush_media_batch_now */
static json_t *fa10_flush_media_batch_now(fa10_ctx_t *ctx, const json_t *args) {
    json_t *pa = fa10_args(); fa10_set_copy(pa, "key", json_obj_get(args, "key"));
    json_t *event = fa10_call(ctx, "batch_pop", pa);
    if (fa10_is_raise(event)) return event;
    if (!fa10_truthy(event)) { json_free(event); return json_null(); }
    json_t *ha = fa10_args(); json_set(ha, "event", event); /* adopted */
    json_t *r = fa10_call(ctx, "handle_guards", ha);
    if (fa10_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: fa10_get_audio_duration_ms @ plugins/platforms/feishu/adapter.py:FeishuAdapter._get_audio_duration_ms */
static json_t *fa10_get_audio_duration_ms(fa10_ctx_t *ctx, const json_t *args) {
    json_t *ra = fa10_args(); fa10_set_copy(ra, "path", json_obj_get(args, "path"));
    json_t *dataj = fa10_call(ctx, "read_audio", ra);
    if (fa10_is_raise(dataj)) {
        if (fa10_raise_is_base(dataj)) return dataj;
        json_free(dataj); return json_number(0);
    }
    size_t n = (dataj && dataj->type == JSON_ARRAY) ? json_len(dataj) : 0;
    unsigned char *data = n ? (unsigned char *)malloc(n) : NULL;
    if (n) for (size_t i = 0; i < n; i++) data[i] = (unsigned char)fa10_num(json_get(dataj, i), 0);
    json_free(dataj);
    long long pos = 0, last_granule = 0;
    while (pos < (long long)n - 27) {
        long long found = -1;
        for (long long i = pos; i + 4 <= (long long)n; i++) {
            if (data[i] == 'O' && data[i+1] == 'g' && data[i+2] == 'g' && data[i+3] == 'S') { found = i; break; }
        }
        pos = found;
        if (pos == -1 || pos + 27 > (long long)n) break;
        long long granule = 0;
        for (int b = 0; b < 8; b++) granule |= ((long long)data[pos + 6 + b]) << (8 * b);
        int num_segments = data[pos + 26];
        if (granule > 0) last_granule = granule;
        if (pos + 27 + num_segments > (long long)n) break;
        long long segsum = 0;
        for (int b = 0; b < num_segments; b++) segsum += data[pos + 27 + b];
        pos += num_segments + segsum;
    }
    if (data) free(data);
    if (last_granule > 0) return json_number((double)(long long)((double)last_granule / 48000.0 * 1000.0));
    return json_number(0);
}

/* PoP: fa10_get_chat_lock @ plugins/platforms/feishu/adapter.py:FeishuAdapter._get_chat_lock */
static json_t *fa10_get_chat_lock(fa10_ctx_t *ctx, const json_t *args) {
    const char *cid = fa10_str_or(json_obj_get(args, "chat_id"), "");
    json_t *ga = fa10_args(); fa10_set_s(ga, "chat_id", cid);
    json_t *lock = fa10_call(ctx, "locks_get", ga);
    if (fa10_is_raise(lock)) return lock;
    if (!fa10_is_null(lock)) {
        json_t *ma = fa10_args(); fa10_set_s(ma, "chat_id", cid);
        json_t *r = fa10_call(ctx, "locks_move_end", ma);
        if (fa10_is_raise(r)) { json_free(lock); return r; }
        json_free(r);
        return lock;
    }
    json_free(lock);
    json_t *la = fa10_args();
    json_t *len = fa10_call(ctx, "locks_len", la);
    if (fa10_is_raise(len)) return len;
    double cnt = fa10_as_num(len, 0);
    json_free(len);
    double cap = fa10_num(json_obj_get(args, "max_size"), 2);
    if (cnt >= cap) {
        json_t *ia = fa10_args();
        json_t *items = fa10_call(ctx, "locks_items", ia);
        if (fa10_is_raise(items)) return items;
        const char *victim = NULL;
        const char *fallback = NULL;
        if (items && items->type == JSON_ARRAY && json_len(items) > 0) {
            for (size_t i = 0; i < json_len(items); i++) {
                json_t *pair = json_get(items, i);
                if (!pair || json_len(pair) < 2) continue;
                const char *k = fa10_str_or(json_get(pair, 0), "");
                if (!fallback) fallback = k;
                bool locked = fa10_bool(json_get(pair, 1), false);
                if (!locked) { victim = k; break; }
            }
            if (!victim) victim = fallback;
        }
        json_free(items);
        if (victim) {
            json_t *pa = fa10_args(); fa10_set_s(pa, "chat_id", victim);
            json_t *r = fa10_call(ctx, "locks_pop", pa);
            if (fa10_is_raise(r)) return r;
            json_free(r);
        }
    }
    json_t *na = fa10_args();
    json_t *nl = fa10_call(ctx, "new_lock", na);
    if (fa10_is_raise(nl)) return nl;
    json_t *sa = fa10_args(); fa10_set_s(sa, "chat_id", cid); json_set(sa, "value", json_copy(nl));
    json_t *r = fa10_call(ctx, "locks_set", sa);
    if (fa10_is_raise(r)) { json_free(nl); return r; }
    json_free(r);
    return nl;
}

/* PoP: fa10_get_sdk_executor @ plugins/platforms/feishu/adapter.py:FeishuAdapter._get_sdk_executor */
static json_t *fa10_get_sdk_executor(fa10_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    bool closing = fa10_bool(json_obj_get(args, "closing"), false);
    if (closing) return fa10_err("RuntimeError", "Feishu adapter is shutting down; SDK executor unavailable");
    const json_t *executor = json_obj_get(args, "executor");
    if (!fa10_is_null(executor)) {
        bool shutdown = false;
        if (executor && executor->type == JSON_OBJECT) shutdown = fa10_bool(json_obj_get(executor, "_shutdown"), false);
        if (!shutdown) return json_copy(executor);
    }
    json_t *ea = fa10_args();
    json_set(ea, "max_workers", json_number(10));
    fa10_set_s(ea, "thread_name_prefix", "hermes-feishu-sdk");
    return fa10_call(ctx, "new_executor", ea);
}

/* PoP: fa10_handle_message_event_data @ plugins/platforms/feishu/adapter.py:FeishuAdapter._handle_message_event_data */
static json_t *fa10_handle_message_event_data(fa10_ctx_t *ctx, const json_t *args) {
    const json_t *data = json_obj_get(args, "data");
    const json_t *event = (data && data->type == JSON_OBJECT) ? json_obj_get(data, "event") : NULL;
    const json_t *message = (event && event->type == JSON_OBJECT) ? json_obj_get(event, "message") : NULL;
    const json_t *sender = (event && event->type == JSON_OBJECT) ? json_obj_get(event, "sender") : NULL;
    if (!message || message->type == JSON_NULL || !sender || sender->type == JSON_NULL) return json_null();
    const char *sid = fa10_str_or(json_obj_get(sender, "sender_id"), "");
    if (!sid[0]) return json_null();
    const char *mid = fa10_str_or(json_obj_get(message, "message_id"), "");
    if (!mid[0]) return json_null();
    json_t *da = fa10_args(); fa10_set_s(da, "message_id", mid);
    json_t *dup = fa10_call(ctx, "is_duplicate", da);
    if (fa10_is_raise(dup)) return dup;
    bool is_dup = fa10_truthy(dup);
    json_free(dup);
    if (is_dup) return json_null();
    json_t *aa = fa10_args();
    json_t *reason = fa10_call(ctx, "admit", aa);
    if (fa10_is_raise(reason)) return reason;
    if (!fa10_is_null(reason)) {
        const char *rs = fa10_str_or(reason, "");
        if (strcmp(rs, "group_policy_rejected") == 0) {
            json_t *wa = fa10_args(); fa10_set_s(wa, "chat_id", fa10_str_or(json_obj_get(message, "chat_id"), ""));
            json_t *w = fa10_call(ctx, "warn_allowlist", wa);
            if (fa10_is_raise(w)) { json_free(reason); return w; }
            json_free(w);
        }
        json_free(reason);
        return json_null();
    }
    json_free(reason);
    json_t *ba = fa10_args(); fa10_set_s(ba, "sender_id", sid);
    json_t *bot = fa10_call(ctx, "is_bot_sender", ba);
    if (fa10_is_raise(bot)) return bot;
    bool is_bot = fa10_truthy(bot);
    json_free(bot);
    const char *chat_type = fa10_str_or(json_obj_get(message, "chat_type"), "p2p");
    json_t *pa = fa10_args();
    fa10_set_s(pa, "message_id", mid); fa10_set_s(pa, "sender_id", sid);
    fa10_set_s(pa, "chat_type", chat_type);
    json_set(pa, "is_bot", json_bool(is_bot));
    json_t *r = fa10_call(ctx, "process_inbound", pa);
    if (fa10_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: fa10_handle_message_with_guards @ plugins/platforms/feishu/adapter.py:FeishuAdapter._handle_message_with_guards */
static json_t *fa10_handle_message_with_guards(fa10_ctx_t *ctx, const json_t *args) {
    const json_t *ev = json_obj_get(args, "event");
    const char *cid = "";
    if (ev && ev->type == JSON_OBJECT) {
        const json_t *src = json_obj_get(ev, "source");
        if (src && src->type == JSON_OBJECT) cid = fa10_str_or(json_obj_get(src, "chat_id"), "");
    }
    json_t *la = fa10_args(); fa10_set_s(la, "chat_id", cid);
    json_t *lock = fa10_call(ctx, "get_chat_lock", la);
    if (fa10_is_raise(lock)) return lock;
    json_free(lock);
    json_t *ha = fa10_args(); fa10_set_copy(ha, "event", ev);
    json_t *handled = fa10_call(ctx, "handle_message", ha);
    if (fa10_is_raise(handled)) return handled;
    json_free(handled);
    return json_null(); /* the Python def awaits but does not return */
}

/* PoP: fa10_handle_webhook_request @ plugins/platforms/feishu/adapter.py:FeishuAdapter._handle_webhook_request */
static json_t *fa10_reject(fa10_ctx_t *ctx, const char *ip, const char *anomaly,
                           double status, const char *text, const char *json_msg) {
    json_t *a = fa10_args();
    fa10_set_s(a, "remote_ip", ip); fa10_set_s(a, "anomaly", anomaly);
    json_set(a, "status", json_number(status));
    if (text) fa10_set_s(a, "text", text); else json_set(a, "text", json_null());
    if (json_msg) fa10_set_s(a, "json_msg", json_msg); else json_set(a, "json_msg", json_null());
    return fa10_call(ctx, "webhook_reject", a);
}
static json_t *fa10_handle_webhook_request(fa10_ctx_t *ctx, const json_t *args) {
    const json_t *req = json_obj_get(args, "request");
    const char *ip = fa10_str_or(req ? json_obj_get(req, "remote") : NULL, "unknown");
    if (!ip[0]) ip = "unknown";
    const json_t *headers = req ? json_obj_get(req, "headers") : NULL;
    const char *ct = fa10_str_or(headers ? json_obj_get(headers, "Content-Type") : NULL, "");
    /* content_type = str(...).split(";")[0].strip().lower() */
    char ctbuf[512]; size_t k = 0;
    for (const char *p = ct; *p && *p != ';' && k + 1 < sizeof ctbuf; p++) ctbuf[k++] = *p;
    ctbuf[k] = '\0';
    const char *cttrim = fa10_strip_inplace(ctbuf);
    const char *ctlower = fa10_lower_inplace(cttrim);
    if (ctlower[0] && strcmp(ctlower, "application/json") != 0)
        return fa10_reject(ctx, ip, "415", 415, "Unsupported Media Type", NULL);
    const json_t *clv = req ? json_obj_get(req, "content_length") : NULL;
    if (!fa10_is_null(clv)) {
        double maxb = fa10_num(json_obj_get(args, "max_body"), 1048576);
        if (fa10_num(clv, 0) > maxb) return fa10_reject(ctx, ip, "413", 413, "Request body too large", NULL);
    }
    json_t *body = fa10_call(ctx, "read_body", fa10_args());
    if (fa10_is_raise(body)) {
        if (fa10_raise_is_base(body)) return body;
        const char *kind = json_get_str(body, "__raise__", "");
        json_free(body);
        if (strcmp(kind, "ValueError") == 0) return fa10_reject(ctx, ip, "413", 413, "Request body too large", NULL);
        if (strcmp(kind, "TimeoutError") == 0) return fa10_reject(ctx, ip, "408", 408, "Request Timeout", NULL);
        return fa10_reject(ctx, ip, "400", 400, NULL, "failed to read body");
    }
    size_t bn = (body && body->type == JSON_ARRAY) ? json_len(body) : 0;
    char *bstr = (char *)malloc(bn + 1);
    if (bn) for (size_t i = 0; i < bn; i++) bstr[i] = (char)fa10_num(json_get(body, i), 0);
    bstr[bn] = '\0';
    json_free(body);
    json_t *payload = json_parse(bstr, NULL);
    free(bstr);
    if (!payload || payload->type != JSON_OBJECT) {
        if (payload) json_free(payload);
        return fa10_reject(ctx, ip, "400", 400, NULL, "invalid json");
    }
    bool vtok = fa10_truthy(json_obj_get(args, "verification_token"));
    const char *vtoks = fa10_str_or(json_obj_get(args, "verification_token"), "");
    if (vtok) {
        const json_t *hdr = json_obj_get(payload, "header");
        const char *incoming = "";
        if (hdr && hdr->type == JSON_OBJECT)
            incoming = fa10_str_or(json_obj_get(hdr, "token"), fa10_str_or(json_obj_get(payload, "token"), ""));
        else
            incoming = fa10_str_or(json_obj_get(payload, "token"), "");
        json_t *ma = fa10_args(); fa10_set_s(ma, "incoming", incoming); fa10_set_s(ma, "expected", vtoks);
        json_t *matched = fa10_call(ctx, "secrets_match", ma);
        if (fa10_is_raise(matched)) { json_free(payload); return matched; }
        bool ok = fa10_truthy(matched);
        json_free(matched);
        if (!ok) { json_free(payload); return fa10_reject(ctx, ip, "401-token", 401, "Invalid verification token", NULL); }
    }
    const char *ptype = fa10_str_or(json_obj_get(payload, "type"), "");
    bool is_url_verification = (strcmp(ptype, "url_verification") == 0);
    bool encrypt_key = fa10_truthy(json_obj_get(args, "encrypt_key"));
    if (encrypt_key && !is_url_verification) {
        json_t *sig = fa10_call(ctx, "sig_valid", fa10_args());
        if (fa10_is_raise(sig)) { json_free(payload); return sig; }
        bool valid = fa10_truthy(sig);
        json_free(sig);
        if (!valid) { json_free(payload); return fa10_reject(ctx, ip, "401-sig", 401, "Invalid signature", NULL); }
    }
    if (vtok || !is_url_verification) {
        char key[1024];
        snprintf(key, sizeof key, "%s:%s:%s", fa10_str_or(json_obj_get(args, "app_id"), ""),
                 fa10_str_or(json_obj_get(args, "webhook_path"), ""), ip);
        json_t *ka = fa10_args(); fa10_set_s(ka, "key", key);
        json_t *allowed = fa10_call(ctx, "rate_limit", ka);
        if (fa10_is_raise(allowed)) { json_free(payload); return allowed; }
        bool ok = fa10_truthy(allowed);
        json_free(allowed);
        if (!ok) { json_free(payload); return fa10_reject(ctx, ip, "429", 429, "Too Many Requests", NULL); }
    }
    if (is_url_verification) {
        const char *challenge = fa10_str_or(json_obj_get(payload, "challenge"), "");
        json_t *wr = json_object(); fa10_set_s(wr, "challenge", challenge);
        json_t *wa = fa10_args(); json_set(wa, "value", wr);
        json_free(payload);
        return fa10_call(ctx, "web_response", wa);
    }
    if (!fa10_is_null(json_obj_get(payload, "encrypt")) && fa10_truthy(json_obj_get(payload, "encrypt"))) {
        json_free(payload);
        return fa10_reject(ctx, ip, "400-encrypted", 400, NULL, "encrypted webhook payloads are not supported");
    }
    json_t *ca = fa10_args(); fa10_set_s(ca, "remote_ip", ip);
    json_t *cleared = fa10_call(ctx, "clear_anomaly", ca);
    if (fa10_is_raise(cleared)) { json_free(payload); return cleared; }
    json_free(cleared);
    const json_t *hdr = json_obj_get(payload, "header");
    const char *event_type = (hdr && hdr->type == JSON_OBJECT) ? fa10_str_or(json_obj_get(hdr, "event_type"), "") : "";
    if (strcmp(event_type, "card.action.trigger") == 0) {
        json_t *card = fa10_call(ctx, "card_action", fa10_args());
        if (fa10_is_raise(card)) { json_free(payload); return card; }
        json_t *wa = fa10_args(); json_set(wa, "value", card);
        json_free(payload);
        return fa10_call(ctx, "web_response", wa);
    }
    json_t *na = fa10_args(); json_set(na, "payload", json_copy(payload));
    json_t *data = fa10_call(ctx, "namespace", na);
    if (fa10_is_raise(data)) { json_free(payload); return data; }
    json_free(data);
    if (strcmp(event_type, "im.message.reaction.created_v1") == 0 ||
        strcmp(event_type, "im.message.reaction.deleted_v1") == 0) {
        json_t *ra = fa10_args(); fa10_set_s(ra, "event_type", event_type);
        json_t *r = fa10_call(ctx, "reaction_event", ra);
        if (fa10_is_raise(r)) { json_free(payload); return r; }
        json_free(r);
    } else {
        const json_t *handlers = json_obj_get(args, "handlers");
        const char *handler = (handlers && handlers->type == JSON_OBJECT) ? fa10_str_or(json_obj_get(handlers, event_type), "") : "";
        if (handler[0]) {
            json_t *r = fa10_call(ctx, "dispatch_handler", fa10_args());
            if (fa10_is_raise(r)) { json_free(payload); return r; }
            json_free(r);
        }
    }
    json_free(payload);
    json_t *okv = json_object(); json_set(okv, "code", json_number(0)); fa10_set_s(okv, "msg", "ok");
    json_t *oa = fa10_args(); json_set(oa, "value", okv);
    return fa10_call(ctx, "web_response", oa);
}

/* PoP: fa10_clear_webhook_anomaly @ plugins/platforms/feishu/adapter.py:FeishuAdapter._clear_webhook_anomaly */
static json_t *fa10_clear_webhook_anomaly(fa10_ctx_t *ctx, const json_t *args) {
    json_t *pa = fa10_args(); fa10_set_copy(pa, "remote_ip", json_obj_get(args, "remote_ip"));
    json_t *popped = fa10_call(ctx, "anomaly_pop", pa);
    if (fa10_is_raise(popped)) return popped;
    json_free(popped);
    return json_null();
}

/* PoP: fa10_ws_link_up @ plugins/platforms/feishu/adapter.py:FeishuAdapter._ws_link_up */
static json_t *fa10_ws_link_up(fa10_ctx_t *ctx, const json_t *args) {
    bool running = fa10_bool(json_obj_get(args, "running"), false);
    const json_t *b = json_obj_get(args, "client");
    const json_t *a = json_obj_get(args, "ws_client");
    if (!a) a = b;  /* absent ws_client means self._ws_client is the client */
    if (!running || !fa10_json_equal(a, b)) return json_null();
    json_t *r = fa10_call(ctx, "mark_connected", fa10_args());
    if (fa10_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: fa10_ws_link_retrying @ plugins/platforms/feishu/adapter.py:FeishuAdapter._ws_link_retrying */
static json_t *fa10_ws_link_retrying(fa10_ctx_t *ctx, const json_t *args) {
    bool running = fa10_bool(json_obj_get(args, "running"), false);
    const json_t *b = json_obj_get(args, "client");
    const json_t *a = json_obj_get(args, "ws_client");
    if (!a) a = b;
    if (!running || !fa10_json_equal(a, b)) return json_null();
    json_t *wa = fa10_args();
    fa10_set_s(wa, "platform_state", "retrying");
    json_set(wa, "error_code", json_null());
    fa10_set_s(wa, "error_message", "Feishu websocket link lost; reconnecting");
    json_t *r = fa10_call(ctx, "write_status", wa);
    if (fa10_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: fa10_submit_if_ready @ plugins/platforms/feishu/adapter.py:FeishuAdapter._submit_if_ready */
static json_t *fa10_submit_if_ready(fa10_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *ok = fa10_call(ctx, "loop_accepts", fa10_args());
    if (fa10_is_raise(ok)) return ok;
    bool accepts = fa10_truthy(ok);
    json_free(ok);
    if (!accepts) return json_null();
    json_t *si = fa10_args();
    json_t *r = fa10_call(ctx, "submit_on_loop", si);
    if (fa10_is_raise(r)) return r;
    json_free(r);
    return json_null();
}
