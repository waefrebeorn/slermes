/*
 * feishu_adapter_flow9.c — Feishu/Lark adapter message-extraction, media/queue,
 * settings, webhook-limit and retry/lark-client FLOW cores of
 * plugins/platforms/feishu/adapter.py (hermes-agent).
 *
 * NINTH remainder port beyond feishu_adapter_flow.c (message normalization),
 * feishu_adapter_flow2..flow8 (inbound/card, admission, SDK gates, outbound,
 * media/response, proxy/onboarding/reactions/batch, card/lifecycle/connections).
 * Ports 16 GENUINELY UNCLAIMED defs (dotted-name-aware claim parser confirmed
 * none are claimed by flows 1-8):
 *   _build_lark_client (module), _normalize, _webhook_reject,
 *   _download_remote_image, _apply_settings, _check_webhook_rate_limit,
 *   _download_feishu_message_resources, _download_feishu_image,
 *   _download_feishu_message_resource, _extract_message_content,
 *   _enqueue_text_event, _enqueue_media_event, _enqueue_pending_inbound_event,
 *   _drain_pending_inbound_events, _feishu_send_with_retry,
 *   _download_remote_document.
 *
 * Universal seam: every seamed call enters through ONE callback
 * fa9_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting the
 * identical {"name","args","ret"} event from the SAME scripts. args is ADOPTED
 * into the trace event; the return is a NEW owned json_t (NULL == Python None;
 * {"__raise__":cls,"message":...} == a raised exception). A missing/exhausted
 * script is a fail-closed RuntimeError on both sides.
 *
 * FULL BASEEXCEPTION PASSTHROUGH: fa9_is_base_exc names the family that bypasses
 * `except Exception`; the retry/limiter/download defs propagate BaseException
 * verbatim and swallow only Exception-side raises where the live def does.
 *
 * Representation contract (shared with tests/oracle_feishu_adapter_flow9.py):
 * - NULL json_t* == Python None; JSON_NULL node is a real null value.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__":cls,"message":str(exc)}; a scripted raise is
 *   {"__script__":true,"raise":{"type":cls,"message":msg}}.
 * - Carrier fields are read directly from args doubles (oracle materializes
 *   equivalent attribute doubles). Fixture domain: ASCII strings, scalars,
 *   JSON dict/list values; POSIX paths; integral-int numbers. Out.state is {}.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and fa9_-prefixed.
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
typedef struct fa9_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} fa9_ctx_t;

/* ── basic helpers ── */
static bool fa9_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool fa9_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool fa9_is_base_exc(const char *kind) {
    return kind && (
        strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "asyncio.CancelledError") == 0);
}
static bool fa9_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *fa9_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static double fa9_num(const json_t *v, double d) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : d;
}
static bool fa9_bool(const json_t *v, bool d) {
    return (v && v->type == JSON_BOOL) ? v->bool_val : d;
}
static const char *fa9_str_or(const json_t *v, const char *d) {
    const char *s = fa9_str(v);
    return s ? s : d;
}
static json_t *fa9_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *fa9_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return fa9_err(type, msg);
}
/* The one universal seam. args is ADOPTED; the return is a fresh owned node. */
static json_t *fa9_call(fa9_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = fa9_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = fa9_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = fa9_make_exc(raise);
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
static json_t *fa9_args(void) { return json_object(); }
static void fa9_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void fa9_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
/* Python str(x) for the scalar fixture domain. */
static const char *fa9_pystr(const json_t *v, char *buf, size_t cap) {
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
static bool fa9_json_equal(const json_t *a, const json_t *b) {
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
                if (!fa9_json_equal(json_get(a, i), json_get(b, i))) return false;
            return true;
        case JSON_OBJECT:
            if (json_object_size(a) != json_object_size(b)) return false;
            for (size_t i = 0; i < json_object_size(a); i++) {
                const char *k = json_object_get_key_at(a, i);
                if (!fa9_json_equal(json_object_get_at(a, i), json_obj_get(b, k))) return false;
            }
            return true;
    }
    return false;
}

/* ══ PoP functions ══ */

/* PoP: fa9_build_lark_client @ plugins/platforms/feishu/adapter.py:_build_lark_client */
static json_t *fa9_build_lark_client(fa9_ctx_t *ctx, const json_t *args) {
    json_t *a = fa9_args();
    fa9_set_copy(a, "app_id", json_obj_get(args, "app_id"));
    fa9_set_copy(a, "app_secret", json_obj_get(args, "app_secret"));
    fa9_set_copy(a, "domain", json_obj_get(args, "sdk_domain"));
    return fa9_call(ctx, "lark_build", a);
}

/* PoP: fa9_normalize @ plugins/platforms/feishu/adapter.py:FeishuAdapter._normalize */
static json_t *fa9_normalize(fa9_ctx_t *ctx, const json_t *args) {
    json_t *bot = fa9_call(ctx, "bot_identity", fa9_args());
    if (fa9_is_raise(bot)) return bot;
    json_t *a = fa9_args();
    fa9_set_copy(a, "message_type", json_obj_get(args, "message_type"));
    fa9_set_copy(a, "raw_content", json_obj_get(args, "raw_content"));
    fa9_set_copy(a, "mentions", json_obj_get(args, "mentions"));
    json_set(a, "bot", bot); /* adopted */
    return fa9_call(ctx, "normalize", a);
}

/* PoP: fa9_webhook_reject @ plugins/platforms/feishu/adapter.py:FeishuAdapter._webhook_reject */
static json_t *fa9_webhook_reject(fa9_ctx_t *ctx, const json_t *args) {
    json_t *ra = fa9_args();
    fa9_set_copy(ra, "remote_ip", json_obj_get(args, "remote_ip"));
    fa9_set_copy(ra, "anomaly", json_obj_get(args, "anomaly"));
    json_t *r = fa9_call(ctx, "record_anomaly", ra);
    if (fa9_is_raise(r)) return r;
    json_free(r);
    const json_t *jm = json_obj_get(args, "json_msg");
    const json_t *status = json_obj_get(args, "status");
    if (!fa9_is_null(jm)) {
        json_t *a = fa9_args();
        json_set(a, "code", status ? json_copy(status) : json_null());
        fa9_set_copy(a, "msg", jm);
        json_set(a, "status", status ? json_copy(status) : json_null());
        return fa9_call(ctx, "json_response", a);
    }
    json_t *a = fa9_args();
    json_set(a, "status", status ? json_copy(status) : json_null());
    fa9_set_copy(a, "text", json_obj_get(args, "text"));
    return fa9_call(ctx, "web_response", a);
}

/* PoP: fa9_download_remote_image @ plugins/platforms/feishu/adapter.py:FeishuAdapter._download_remote_image */
static json_t *fa9_download_remote_image(fa9_ctx_t *ctx, const json_t *args) {
    json_t *ga = fa9_args();
    fa9_set_copy(ga, "url", json_obj_get(args, "image_url"));
    fa9_set_s(ga, "default", ".jpg");
    json_t *ext = fa9_call(ctx, "guess_ext", ga);
    if (fa9_is_raise(ext)) return ext;
    json_t *a = fa9_args();
    fa9_set_copy(a, "url", json_obj_get(args, "image_url"));
    json_set(a, "ext", ext); /* adopted */
    return fa9_call(ctx, "cache_img", a);
}

/* FeishuAdapterSettings field order (matches the dataclass __dataclass_fields__). */
static const char *FA9_SETTINGS_FIELDS[] = {
    "app_id", "app_secret", "domain_name", "connection_mode", "encrypt_key",
    "verification_token", "group_policy", "allowed_group_users", "bot_open_id",
    "bot_user_id", "bot_name", "dedup_cache_size", "text_batch_delay_seconds",
    "text_batch_split_delay_seconds", "text_batch_max_messages", "text_batch_max_chars",
    "media_batch_delay_seconds", "webhook_host", "webhook_port", "webhook_path",
    "ws_reconnect_nonce", "ws_reconnect_interval", "ws_ping_interval", "ws_ping_timeout",
    "admins", "default_group_policy", "group_rules", "allow_bots",
    "require_mention", "allow_all_dm",
};
#define FA9_SETTINGS_N (sizeof(FA9_SETTINGS_FIELDS) / sizeof(FA9_SETTINGS_FIELDS[0]))

/* PoP: fa9_apply_settings @ plugins/platforms/feishu/adapter.py:FeishuAdapter._apply_settings */
static json_t *fa9_apply_settings(fa9_ctx_t *ctx, const json_t *args) {
    const json_t *settings = json_obj_get(args, "settings");
    char name[128];
    for (size_t i = 0; i < FA9_SETTINGS_N; i++) {
        json_t *a = fa9_args();
        snprintf(name, sizeof name, "_%s", FA9_SETTINGS_FIELDS[i]);
        fa9_set_s(a, "name", name);
        fa9_set_copy(a, "value", settings ? json_obj_get(settings, FA9_SETTINGS_FIELDS[i]) : NULL);
        json_t *r = fa9_call(ctx, "set_field", a);
        if (fa9_is_raise(r)) return r;
        json_free(r);
    }
    /* _allowed_group_users = set(settings.allowed_group_users) */
    json_t *a = fa9_args();
    fa9_set_s(a, "name", "_allowed_group_users");
    fa9_set_copy(a, "value", settings ? json_obj_get(settings, "allowed_group_users") : NULL);
    json_t *r = fa9_call(ctx, "set_field", a);
    if (fa9_is_raise(r)) return r;
    json_free(r);
    /* _admins = set(settings.admins) */
    a = fa9_args();
    fa9_set_s(a, "name", "_admins");
    fa9_set_copy(a, "value", settings ? json_obj_get(settings, "admins") : NULL);
    r = fa9_call(ctx, "set_field", a);
    if (fa9_is_raise(r)) return r;
    json_free(r);
    /* _default_group_policy = settings.default_group_policy or settings.group_policy */
    a = fa9_args();
    fa9_set_s(a, "name", "_default_group_policy");
    const json_t *dgp = settings ? json_obj_get(settings, "default_group_policy") : NULL;
    const json_t *gp = settings ? json_obj_get(settings, "group_policy") : NULL;
    fa9_set_copy(a, "value", fa9_truthy(dgp) ? dgp : gp);
    r = fa9_call(ctx, "set_field", a);
    if (fa9_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: fa9_check_webhook_rate_limit @ plugins/platforms/feishu/adapter.py:FeishuAdapter._check_webhook_rate_limit */
static json_t *fa9_check_webhook_rate_limit(fa9_ctx_t *ctx, const json_t *args) {
    const json_t *key = json_obj_get(args, "rate_key");
    json_t *now = fa9_call(ctx, "now", fa9_args());
    if (fa9_is_raise(now)) return now;
    double t = fa9_num(now, 0.0);
    json_free(now);
    json_t *ga = fa9_args();
    fa9_set_copy(ga, "key", key);
    json_t *entry = fa9_call(ctx, "counts_get", ga);
    if (fa9_is_raise(entry)) return entry;
    if (entry && entry->type == JSON_ARRAY && json_len(entry) >= 2) {
        double count = fa9_num(json_get(entry, 0), 0.0);
        double ws = fa9_num(json_get(entry, 1), 0.0);
        json_free(entry);
        if (t - ws < 60.0) {
            if (count >= 120.0) return json_bool(false);
            json_t *a = fa9_args();
            fa9_set_copy(a, "key", key);
            json_t *v = json_array();
            json_append(v, json_number(count + 1.0));
            json_append(v, json_number(ws));
            json_set(a, "value", v);
            json_t *r = fa9_call(ctx, "counts_set", a);
            if (fa9_is_raise(r)) return r;
            json_free(r);
            return json_bool(true);
        }
    } else if (!fa9_is_null(entry)) {
        json_free(entry);
        return fa9_err("TypeError", "cannot unpack non-iterable object");
    } else {
        json_free(entry);
    }
    /* new window or new key */
    json_t *ln = fa9_call(ctx, "counts_len", fa9_args());
    if (fa9_is_raise(ln)) return ln;
    double n = fa9_num(ln, 0.0);
    json_free(ln);
    if (n >= 4096.0) {
        json_t *items = fa9_call(ctx, "counts_items", fa9_args());
        if (fa9_is_raise(items)) return items;
        if (items && items->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(items); i++) {
                const json_t *it = json_get(items, i);
                if (it->type == JSON_ARRAY && json_len(it) >= 2) {
                    double ws2 = fa9_num(json_get(json_get(it, 1), 1), 0.0);
                    if (t - ws2 >= 60.0) {
                        json_t *da = fa9_args();
                        fa9_set_copy(da, "key", json_get(it, 0));
                        json_t *r = fa9_call(ctx, "counts_del", da);
                        if (fa9_is_raise(r)) { json_free(items); return r; }
                        json_free(r);
                    }
                }
            }
        }
        json_free(items);
        json_t *ga2 = fa9_args();
        fa9_set_copy(ga2, "key", key);
        json_t *present = fa9_call(ctx, "counts_get", ga2);
        if (fa9_is_raise(present)) return present;
        bool in_map = !fa9_is_null(present);
        json_free(present);
        json_t *ln2 = fa9_call(ctx, "counts_len", fa9_args());
        if (fa9_is_raise(ln2)) return ln2;
        double n2 = fa9_num(ln2, 0.0);
        json_free(ln2);
        if (!in_map && n2 >= 4096.0) {
            json_t *r = fa9_call(ctx, "log_warn", fa9_args());
            if (fa9_is_raise(r)) return r;
            json_free(r);
            return json_bool(false);
        }
    }
    json_t *a = fa9_args();
    fa9_set_copy(a, "key", key);
    json_t *v = json_array();
    json_append(v, json_number(1.0));
    json_append(v, json_number(t));
    json_set(a, "value", v);
    json_t *r = fa9_call(ctx, "counts_set", a);
    if (fa9_is_raise(r)) return r;
    json_free(r);
    return json_bool(true);
}

/* PoP: fa9_download_feishu_message_resources @ plugins/platforms/feishu/adapter.py:FeishuAdapter._download_feishu_message_resources */
static json_t *fa9_download_feishu_message_resources(fa9_ctx_t *ctx, const json_t *args) {
    const json_t *norm = json_obj_get(args, "normalized");
    const json_t *keys = norm ? json_obj_get(norm, "image_keys") : NULL;
    const json_t *refs = norm ? json_obj_get(norm, "media_refs") : NULL;
    const json_t *mid = json_obj_get(args, "message_id");
    json_t *urls = json_array();
    json_t *types = json_array();
    if (keys && keys->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(keys); i++) {
            json_t *a = fa9_args();
            fa9_set_copy(a, "message_id", mid);
            fa9_set_s(a, "image_key", fa9_str_or(json_get(keys, i), ""));
            json_t *r = fa9_call(ctx, "dl_img", a);
            if (fa9_is_raise(r)) { json_free(urls); json_free(types); return r; }
            if (r->type == JSON_ARRAY && json_len(r) >= 2) {
                if (fa9_truthy(json_get(r, 0))) {
                    json_append(urls, json_copy(json_get(r, 0)));
                    json_append(types, json_copy(json_get(r, 1)));
                }
            }
            json_free(r);
        }
    }
    if (refs && refs->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(refs); i++) {
            const json_t *rf = json_get(refs, i);
            json_t *a = fa9_args();
            fa9_set_copy(a, "message_id", mid);
            fa9_set_copy(a, "file_key", rf ? json_obj_get(rf, "file_key") : NULL);
            fa9_set_copy(a, "resource_type", rf ? json_obj_get(rf, "resource_type") : NULL);
            fa9_set_copy(a, "fallback_filename", rf ? json_obj_get(rf, "file_name") : NULL);
            json_t *r = fa9_call(ctx, "dl_res", a);
            if (fa9_is_raise(r)) { json_free(urls); json_free(types); return r; }
            if (r->type == JSON_ARRAY && json_len(r) >= 2) {
                if (fa9_truthy(json_get(r, 0))) {
                    json_append(urls, json_copy(json_get(r, 0)));
                    json_append(types, json_copy(json_get(r, 1)));
                }
            }
            json_free(r);
        }
    }
    json_t *out = json_array();
    json_append(out, urls);
    json_append(out, types);
    return out;
}

/* PoP: fa9_enqueue_pending_inbound_event @ plugins/platforms/feishu/adapter.py:FeishuAdapter._enqueue_pending_inbound_event */
static json_t *fa9_enqueue_pending_inbound_event(fa9_ctx_t *ctx, const json_t *args) {
    json_t *r = fa9_call(ctx, "lock_enter", fa9_args());
    if (fa9_is_raise(r)) return r;
    json_free(r);
    json_t *pending = NULL;
    int should_start = 0;
    /* body */
    json_t *ln = fa9_call(ctx, "queued_len", fa9_args());
    if (fa9_is_raise(ln)) { pending = ln; goto after_body; }
    double qlen = fa9_num(ln, 0.0);
    json_free(ln);
    double maxd = fa9_num(json_obj_get(args, "max_depth"), 0.0);
    if (qlen >= maxd) {
        json_t *pop = fa9_call(ctx, "queued_pop", fa9_args());
        if (fa9_is_raise(pop)) { pending = pop; goto after_body; }
        char mib[512];
        const char *mid = "";
        const json_t *ev = pop ? json_obj_get(pop, "event") : NULL;
        const json_t *msg = ev ? json_obj_get(ev, "message") : NULL;
        const json_t *idn = msg ? json_obj_get(msg, "message_id") : NULL;
        if (!idn || fa9_is_null(idn) || !fa9_truthy(idn)) snprintf(mib, sizeof mib, "unknown");
        else { const char *s = fa9_str(idn); snprintf(mib, sizeof mib, "%s", s ? s : "unknown"); }
        mid = mib;
        json_free(pop);
        json_t *la = fa9_args();
        fa9_set_s(la, "message_id", mid);
        json_t *lg = fa9_call(ctx, "log_error", la);
        if (fa9_is_raise(lg)) { pending = lg; goto after_body; }
        json_free(lg);
    }
    json_t *ap = fa9_call(ctx, "queued_append", fa9_args());
    if (fa9_is_raise(ap)) { pending = ap; goto after_body; }
    json_free(ap);
    json_t *ln2 = fa9_call(ctx, "queued_len", fa9_args());
    if (fa9_is_raise(ln2)) { pending = ln2; goto after_body; }
    json_free(ln2); /* depth unused in trace beyond the seam */
    should_start = !fa9_bool(json_obj_get(args, "pending_drain_scheduled"), false);
after_body:
    json_t *lx = fa9_call(ctx, "lock_exit", fa9_args());
    if (fa9_is_raise(lx)) { if (pending) json_free(pending); return lx; }
    json_free(lx);
    if (pending) return pending;
    json_t *lw = fa9_call(ctx, "log_warning", fa9_args());
    if (fa9_is_raise(lw)) return lw;
    json_free(lw);
    return json_bool(should_start);
}

/* swallow an Exception-family raise (return swallowed=true), propagate BaseException */
static bool fa9_swallowed(json_t *r) {
    if (!fa9_is_raise(r)) return false;
    const char *k = json_get_str(r, "__raise__", "Exception");
    if (fa9_is_base_exc(k)) return false;
    json_free(r);
    return true;
}
static json_t *fa9_pair(const char *a, const char *b) {
    json_t *o = json_array();
    json_append(o, json_string(a));
    json_append(o, json_string(b));
    return o;
}

/* PoP: fa9_download_feishu_image @ plugins/platforms/feishu/adapter.py:FeishuAdapter._download_feishu_image */
static json_t *fa9_download_feishu_image(fa9_ctx_t *ctx, const json_t *args) {
    bool client = fa9_bool(json_obj_get(args, "client_present"), false);
    const json_t *mid = json_obj_get(args, "message_id");
    const char *key = fa9_str_or(json_obj_get(args, "image_key"), "");
    if (!client || !fa9_truthy(mid)) return fa9_pair("", "");
    json_t *a = fa9_args();
    fa9_set_copy(a, "message_id", mid);
    fa9_set_s(a, "file_key", key);
    fa9_set_s(a, "resource_type", "image");
    json_t *resp = fa9_call(ctx, "fetch_resource", a);
    if (fa9_is_raise(resp)) {
        const char *k = json_get_str(resp, "__raise__", "Exception");
        if (fa9_is_base_exc(k)) return resp;
        json_free(resp); return fa9_pair("", "");
    }
    if (!fa9_truthy(resp)) { json_free(resp); return fa9_pair("", ""); }
    json_t *succ = fa9_call(ctx, "resp_success", fa9_args());
    if (fa9_is_raise(succ)) {
        const char *k = json_get_str(succ, "__raise__", "Exception");
        json_free(resp);
        if (fa9_is_base_exc(k)) return succ;
        json_free(succ); return fa9_pair("", "");
    }
    bool ok = fa9_truthy(succ);
    json_free(succ);
    if (!ok) { json_free(resp); return fa9_pair("", ""); }
    json_t *raw = fa9_call(ctx, "read_binary", fa9_args());
    if (fa9_is_raise(raw)) {
        const char *k = json_get_str(raw, "__raise__", "Exception");
        json_free(resp);
        if (fa9_is_base_exc(k)) return raw;
        json_free(raw); return fa9_pair("", "");
    }
    if (!fa9_truthy(raw)) { json_free(raw); json_free(resp); return fa9_pair("", ""); }
    json_t *ha = fa9_args();
    fa9_set_s(ha, "name", "Content-Type");
    json_t *ct = fa9_call(ctx, "get_header", ha);
    if (fa9_is_raise(ct)) {
        const char *k = json_get_str(ct, "__raise__", "Exception");
        json_free(raw); json_free(resp);
        if (fa9_is_base_exc(k)) return ct;
        json_free(ct); return fa9_pair("", "");
    }
    const char *cts = fa9_str_or(ct, "");
    /* filename = getattr(response,"file_name",None) or f"{image_key}.jpg" */
    char fn[1024];
    const char *fnv = fa9_str(json_obj_get(resp, "file_name"));
    if (fnv && fnv[0]) snprintf(fn, sizeof fn, "%s", fnv);
    else snprintf(fn, sizeof fn, "%s.jpg", key);
    json_t *ga = fa9_args();
    fa9_set_s(ga, "filename", fn);
    fa9_set_s(ga, "content_type", cts);
    fa9_set_s(ga, "default", ".jpg");
    json_t *ext = fa9_call(ctx, "guess_file_ext", ga);
    if (fa9_is_raise(ext)) {
        const char *k = json_get_str(ext, "__raise__", "Exception");
        json_free(ct); json_free(raw); json_free(resp);
        if (fa9_is_base_exc(k)) return ext;
        json_free(ext); return fa9_pair("", "");
    }
    const char *exts = fa9_str_or(ext, ".jpg");
    json_t *ca = fa9_args();
    fa9_set_s(ca, "ext", exts);
    json_t *cached = fa9_call(ctx, "cache_img_bytes", ca);
    if (fa9_is_raise(cached)) {
        const char *k = json_get_str(cached, "__raise__", "Exception");
        json_free(ext); json_free(ct); json_free(raw); json_free(resp);
        if (fa9_is_base_exc(k)) return cached;
        json_free(cached); return fa9_pair("", "");
    }
    /* default=self._default_image_media_type(ext) evaluated before norm_media */
    json_t *da = fa9_args();
    fa9_set_s(da, "ext", exts);
    json_t *dflt = fa9_call(ctx, "default_img_media", da);
    json_t *na = fa9_args();
    fa9_set_s(na, "content_type", cts);
    json_set(na, "default", dflt ? dflt : json_null()); /* adopted */
    json_t *mt = fa9_call(ctx, "norm_media", na);
    json_free(ext); json_free(ct); json_free(raw); json_free(resp);
    if (fa9_is_raise(mt)) {
        const char *k = json_get_str(mt, "__raise__", "Exception");
        if (fa9_is_base_exc(k)) { json_free(cached); return mt; }
        json_free(mt); json_free(cached); return fa9_pair("", "");
    }
    json_t *out = json_array();
    json_append(out, cached);
    json_append(out, mt);
    return out;
}

/* PoP: fa9_download_feishu_message_resource @ plugins/platforms/feishu/adapter.py:FeishuAdapter._download_feishu_message_resource */
static json_t *fa9_download_feishu_message_resource(fa9_ctx_t *ctx, const json_t *args) {
    bool client = fa9_bool(json_obj_get(args, "client_present"), false);
    const json_t *mid = json_obj_get(args, "message_id");
    if (!client || !fa9_truthy(mid)) return fa9_pair("", "");
    const char *rt = fa9_str_or(json_obj_get(args, "resource_type"), "");
    const char *fk = fa9_str_or(json_obj_get(args, "file_key"), "");
    const char *fb = fa9_str_or(json_obj_get(args, "fallback_filename"), "");
    const char *types[2];
    int ntypes = 0;
    types[ntypes++] = rt;
    if (strcmp(rt, "audio") == 0 || strcmp(rt, "media") == 0) types[ntypes++] = "file";
    for (int i = 0; i < ntypes; i++) {
        const char *request_type = types[i];
        json_t *a = fa9_args();
        fa9_set_copy(a, "message_id", mid);
        fa9_set_s(a, "file_key", fk);
        fa9_set_s(a, "resource_type", request_type);
        json_t *resp = fa9_call(ctx, "fetch_resource", a);
        if (fa9_is_raise(resp)) {
            const char *k = json_get_str(resp, "__raise__", "Exception");
            if (fa9_is_base_exc(k)) return resp;
            json_free(resp); continue;
        }
        if (!fa9_truthy(resp)) { json_free(resp); continue; }
        json_t *succ = fa9_call(ctx, "resp_success", fa9_args());
        if (fa9_is_raise(succ)) {
            const char *k = json_get_str(succ, "__raise__", "Exception");
            json_free(resp);
            if (fa9_is_base_exc(k)) return succ;
            json_free(succ); continue;
        }
        bool ok = fa9_truthy(succ); json_free(succ);
        if (!ok) { json_free(resp); continue; }
        json_t *raw = fa9_call(ctx, "read_binary", fa9_args());
        if (fa9_is_raise(raw)) {
            const char *k = json_get_str(raw, "__raise__", "Exception");
            json_free(resp);
            if (fa9_is_base_exc(k)) return raw;
            json_free(raw); continue;
        }
        if (!fa9_truthy(raw)) { json_free(raw); json_free(resp); continue; }
        json_t *ha = fa9_args(); fa9_set_s(ha, "name", "Content-Type");
        json_t *ct = fa9_call(ctx, "get_header", ha);
        if (fa9_is_raise(ct)) {
            const char *k = json_get_str(ct, "__raise__", "Exception");
            json_free(raw); json_free(resp);
            if (fa9_is_base_exc(k)) return ct;
            json_free(ct); continue;
        }
        const char *cts = fa9_str_or(ct, "");
        char fn[1024];
        const char *fnv = fa9_str(json_obj_get(resp, "file_name"));
        if (fnv && fnv[0]) snprintf(fn, sizeof fn, "%s", fnv);
        else if (fb[0]) snprintf(fn, sizeof fn, "%s", fb);
        else snprintf(fn, sizeof fn, "%s_%s", request_type, fk);
        /* default=self._guess_media_type_from_filename(filename) */
        json_t *ma = fa9_args(); fa9_set_s(ma, "filename", fn);
        json_t *dflt = fa9_call(ctx, "guess_media_name", ma);
        if (fa9_is_raise(dflt)) {
            const char *k = json_get_str(dflt, "__raise__", "Exception");
            json_free(ct); json_free(raw); json_free(resp);
            if (fa9_is_base_exc(k)) return dflt;
            json_free(dflt); continue;
        }
        json_t *na = fa9_args();
        fa9_set_s(na, "content_type", cts);
        json_set(na, "default", dflt); /* adopted */
        json_t *mt = fa9_call(ctx, "norm_media", na);
        if (fa9_is_raise(mt)) {
            const char *k = json_get_str(mt, "__raise__", "Exception");
            json_free(ct); json_free(raw); json_free(resp);
            if (fa9_is_base_exc(k)) return mt;
            json_free(mt); continue;
        }
        char mtbuf[512];
        const char *mts = fa9_str_or(mt, "");
        snprintf(mtbuf, sizeof mtbuf, "%s", mts);
        const char *final_mt = mtbuf;
        json_t *cached = NULL;
        if (strncmp(mts, "image/", 6) == 0) {
            json_t *ga = fa9_args(); fa9_set_s(ga, "filename", fn);
            fa9_set_s(ga, "content_type", cts); fa9_set_s(ga, "default", ".jpg");
            json_t *ext = fa9_call(ctx, "guess_file_ext", ga);
            const char *exts = fa9_str_or(ext, ".jpg");
            json_t *ca = fa9_args(); fa9_set_s(ca, "ext", exts);
            cached = fa9_call(ctx, "cache_img_bytes", ca);
            if (fa9_is_raise(cached)) {
                const char *k = json_get_str(cached, "__raise__", "Exception");
                json_free(ext); json_free(mt); json_free(ct); json_free(raw); json_free(resp);
                if (fa9_is_base_exc(k)) return cached;
                json_free(cached); continue;
            }
            if (!fa9_truthy(mt)) {
                json_t *da = fa9_args(); fa9_set_s(da, "ext", exts);
                json_t *dm = fa9_call(ctx, "default_img_media", da);
                snprintf(mtbuf, sizeof mtbuf, "%s", fa9_str_or(dm, ""));
                json_free(dm);
            }
            json_free(ext);
        } else if (strcmp(request_type, "audio") == 0 || strncmp(mts, "audio/", 6) == 0) {
            json_t *ga = fa9_args(); fa9_set_s(ga, "filename", fn);
            fa9_set_s(ga, "content_type", cts); fa9_set_s(ga, "default", ".ogg");
            json_t *ext = fa9_call(ctx, "guess_file_ext", ga);
            const char *exts = fa9_str_or(ext, ".ogg");
            json_t *ca = fa9_args(); fa9_set_s(ca, "ext", exts);
            cached = fa9_call(ctx, "cache_audio_bytes", ca);
            if (fa9_is_raise(cached)) {
                const char *k = json_get_str(cached, "__raise__", "Exception");
                json_free(ext); json_free(mt); json_free(ct); json_free(raw); json_free(resp);
                if (fa9_is_base_exc(k)) return cached;
                json_free(cached); continue;
            }
            if (!fa9_truthy(mt)) {
                snprintf(mtbuf, sizeof mtbuf, "audio/%s",
                    (exts[0] == '.') ? (exts + 1) : exts);
            }
            json_free(ext);
        } else if (strncmp(mts, "video/", 6) == 0) {
            char dn[1024]; snprintf(dn, sizeof dn, "%s", fn);
            if (!strchr(fn, '.')) { snprintf(dn, sizeof dn, "%s.mp4", fn); }
            json_t *ca = fa9_args(); fa9_set_s(ca, "filename", dn);
            cached = fa9_call(ctx, "cache_doc_bytes", ca);
            if (fa9_is_raise(cached)) {
                const char *k = json_get_str(cached, "__raise__", "Exception");
                json_free(mt); json_free(ct); json_free(raw); json_free(resp);
                if (fa9_is_base_exc(k)) return cached;
                json_free(cached); continue;
            }
        } else {
            char dn[1024]; snprintf(dn, sizeof dn, "%s", fn);
            json_t *ca = fa9_args(); fa9_set_s(ca, "filename", dn);
            cached = fa9_call(ctx, "cache_doc_bytes", ca);
            if (fa9_is_raise(cached)) {
                const char *k = json_get_str(cached, "__raise__", "Exception");
                json_free(mt); json_free(ct); json_free(raw); json_free(resp);
                if (fa9_is_base_exc(k)) return cached;
                json_free(cached); continue;
            }
            if (!fa9_truthy(mt)) {
                json_t *da = fa9_args(); fa9_set_s(da, "filename", dn);
                json_t *dm = fa9_call(ctx, "guess_doc_media", da);
                snprintf(mtbuf, sizeof mtbuf, "%s", fa9_str_or(dm, ""));
                json_free(dm);
            }
        }
        json_free(mt);
        json_free(ct); json_free(raw); json_free(resp);
        json_t *out = json_array();
        json_append(out, cached); /* adopted */
        json_append(out, json_string(final_mt));
        return out;
    }
    return fa9_pair("", "");
}

/* PoP: fa9_extract_message_content @ plugins/platforms/feishu/adapter.py:FeishuAdapter._extract_message_content */
static json_t *fa9_extract_message_content(fa9_ctx_t *ctx, const json_t *args) {
    const json_t *m = json_obj_get(args, "message");
    const char *content = fa9_str_or(m ? json_obj_get(m, "content") : NULL, "");
    const char *rt = fa9_str_or(m ? json_obj_get(m, "message_type") : NULL, "");
    char midb[512];
    const json_t *midv = m ? json_obj_get(m, "message_id") : NULL;
    const char *mid = fa9_pystr(midv, midb, sizeof midb);
    if (midv && fa9_is_null(midv)) mid = "";
    const json_t *mentions = m ? json_obj_get(m, "mentions") : NULL;
    /* normalized = self._normalize(raw_type, raw_content, mentions) */
    json_t *na = fa9_args();
    fa9_set_s(na, "message_type", rt);
    fa9_set_s(na, "raw_content", content);
    fa9_set_copy(na, "mentions", mentions);
    json_t *norm = fa9_call(ctx, "normalize_msg", na);
    if (fa9_is_raise(norm)) return norm;
    /* media_urls, media_types = await self._download_feishu_message_resources(...) */
    json_t *ra = fa9_args();
    fa9_set_s(ra, "message_id", mid);
    json_set(ra, "normalized", json_copy(norm));
    json_t *res = fa9_call(ctx, "fetch_resources", ra);
    if (fa9_is_raise(res)) { json_free(norm); return res; }
    json_t *urls = (res->type == JSON_ARRAY && json_len(res) >= 1) ? json_copy(json_get(res, 0)) : json_array();
    json_t *types = (res->type == JSON_ARRAY && json_len(res) >= 2) ? json_copy(json_get(res, 1)) : json_array();
    json_free(res);
    /* inbound_type = self._resolve_normalized_message_type(normalized, media_types) */
    json_t *ta = fa9_args();
    json_set(ta, "normalized", json_copy(norm));
    json_set(ta, "media_types", json_copy(types));
    json_t *itype = fa9_call(ctx, "resolve_type", ta);
    if (fa9_is_raise(itype)) { json_free(norm); json_free(urls); json_free(types); return itype; }
    /* text = normalized.text_content */
    const json_t *textv = json_obj_get(norm, "text_content");
    char textb[8192];
    const char *text = fa9_pystr(textv, textb, sizeof textb);
    if (!textv || fa9_is_null(textv)) text = "";
    const json_t *nmentions = json_obj_get(norm, "mentions");
    /* is_command = caption_is_command(_strip_edge_self_mentions(text, normalized.mentions)) */
    json_t *sa = fa9_args();
    fa9_set_s(sa, "text", text);
    fa9_set_copy(sa, "mentions", nmentions);
    json_t *stripped = fa9_call(ctx, "strip_mentions", sa);
    if (fa9_is_raise(stripped)) { json_free(norm); json_free(urls); json_free(types); json_free(itype); return stripped; }
    char stripb[8192];
    const char *ss = fa9_str(stripped) ? fa9_str(stripped) : fa9_pystr(stripped, stripb, sizeof stripb);
    json_t *ca = fa9_args();
    fa9_set_s(ca, "text", ss);
    json_t *isc = fa9_call(ctx, "caption_is_command", ca);
    if (fa9_is_raise(isc)) { json_free(norm); json_free(urls); json_free(types); json_free(itype); json_free(stripped); return isc; }
    bool is_command = fa9_truthy(isc);
    json_free(isc); json_free(stripped);
    json_t *inlined = json_array();
    json_t *parts = json_array();
    size_t n = json_len(urls) < json_len(types) ? json_len(urls) : json_len(types);
    for (size_t i = 0; i < n; i++) {
        const char *extracted = "";
        char extb[8192];
        if (!is_command) {
            json_t *ea = fa9_args();
            fa9_set_copy(ea, "url", json_get(urls, i));
            fa9_set_copy(ea, "media_type", json_get(types, i));
            json_t *ex = fa9_call(ctx, "extract_doc", ea);
            if (fa9_is_raise(ex)) { json_free(norm); json_free(urls); json_free(types); json_free(itype); json_free(inlined); json_free(parts); return ex; }
            extracted = fa9_str(ex) ? fa9_str(ex) : fa9_pystr(ex, extb, sizeof extb);
            /* keep ex alive until end via parts? copy into parts below */
            if (fa9_truthy(ex)) json_append(parts, json_copy(ex));
            json_free(ex);
        }
        json_append(inlined, json_bool(extracted[0] != '\0'));
    }
    char final_text[16384];
    if (json_len(parts) > 0) {
        char joined[16384]; joined[0] = '\0';
        for (size_t i = 0; i < json_len(parts); i++) {
            const char *p = fa9_str(json_get(parts, i)) ? fa9_str(json_get(parts, i)) : "";
            if (i) strncat(joined, "\n\n", sizeof joined - strlen(joined) - 1);
            strncat(joined, p, sizeof joined - strlen(joined) - 1);
        }
        if (text[0]) snprintf(final_text, sizeof final_text, "%s\n\n%s", text, joined);
        else snprintf(final_text, sizeof final_text, "%s", joined);
    } else {
        snprintf(final_text, sizeof final_text, "%s", text);
    }
    json_free(parts);
    json_t *out = json_array();
    json_append(out, json_string(final_text));
    json_append(out, itype); /* adopted */
    json_append(out, urls); /* adopted */
    json_append(out, types); /* adopted */
    json_append(out, inlined); /* adopted */
    json_append(out, nmentions ? json_copy(nmentions) : json_array());
    json_free(norm);
    return out;
}

/* PoP: fa9_enqueue_text_event @ plugins/platforms/feishu/adapter.py:FeishuAdapter._enqueue_text_event */
static json_t *fa9_enqueue_text_event(fa9_ctx_t *ctx, const json_t *args) {
    const json_t *ev = json_obj_get(args, "event");
    /* key = self._text_batch_key(event) */
    json_t *ka = fa9_args();
    json_t *key = fa9_call(ctx, "text_key", ka);
    if (fa9_is_raise(key)) return key;
    /* chunk_len = len(event.text or "") */
    const json_t *et = ev ? json_obj_get(ev, "text") : NULL;
    const char *ets = fa9_str(et) ? fa9_str(et) : "";
    size_t chunk = strlen(ets);
    /* existing = self._pending_text_batches.get(key) */
    json_t *ga = fa9_args(); json_set(ga, "key", json_copy(key));
    json_t *existing = fa9_call(ctx, "batches_get", ga);
    if (fa9_is_raise(existing)) { json_free(key); return existing; }
    bool has = !fa9_is_null(existing);
    if (has) {
        json_t *ca = fa9_args();
        json_t *comp = fa9_call(ctx, "batch_compatible", ca);
        if (fa9_is_raise(comp)) { json_free(key); json_free(existing); return comp; }
        bool compat = fa9_truthy(comp);
        json_free(comp);
        if (!compat) {
            json_t *fa = fa9_args(); json_set(fa, "key", json_copy(key));
            json_t *fr = fa9_call(ctx, "flush_now", fa);
            if (fa9_is_raise(fr)) { json_free(key); json_free(existing); return fr; }
            json_free(fr);
        }
        if (!compat) {
            /* _start_batch */
            json_t *sa = fa9_args(); json_set(sa, "key", json_copy(key));
            json_t *r = fa9_call(ctx, "batches_set", sa);
            if (fa9_is_raise(r)) { json_free(key); json_free(existing); return r; }
            json_free(r);
            json_t *ca2 = fa9_args(); json_set(ca2, "key", json_copy(key));
            json_set(ca2, "value", json_number(1));
            r = fa9_call(ctx, "counts_set", ca2);
            if (fa9_is_raise(r)) { json_free(key); json_free(existing); return r; }
            json_free(r);
            json_t *fa2 = fa9_args(); json_set(fa2, "key", json_copy(key));
            r = fa9_call(ctx, "schedule_flush", fa2);
            if (fa9_is_raise(r)) { json_free(key); json_free(existing); return r; }
            json_free(r);
            json_free(key); json_free(existing);
            return json_null();
        }
        /* merge path */
        json_t *cga = fa9_args(); json_set(cga, "key", json_copy(key));
        json_t *cnt = fa9_call(ctx, "counts_get", cga);
        if (fa9_is_raise(cnt)) { json_free(key); json_free(existing); return cnt; }
        double next_count = fa9_num(cnt, 1.0) + 1.0;
        json_free(cnt);
        const char *xtext = fa9_str(json_obj_get(existing, "text")) ? fa9_str(json_obj_get(existing, "text")) : "";
        const char *append = (ets && ets[0]) ? ets : "";
        char next_text[8192];
        if (xtext[0] && append[0]) snprintf(next_text, sizeof next_text, "%s\n%s", xtext, append);
        else snprintf(next_text, sizeof next_text, "%s", xtext[0] ? xtext : append);
        double maxmsg = fa9_num(json_obj_get(args, "max_messages"), 0.0);
        double maxchars = fa9_num(json_obj_get(args, "max_chars"), 0.0);
        if (next_count > maxmsg || strlen(next_text) > (size_t)maxchars) {
            json_t *fa = fa9_args(); json_set(fa, "key", json_copy(key));
            json_t *fr = fa9_call(ctx, "flush_now", fa);
            if (fa9_is_raise(fr)) { json_free(key); json_free(existing); return fr; }
            json_free(fr);
            json_t *sa = fa9_args(); json_set(sa, "key", json_copy(key));
            json_t *r = fa9_call(ctx, "batches_set", sa);
            if (fa9_is_raise(r)) { json_free(key); json_free(existing); return r; }
            json_free(r);
            json_t *ca2 = fa9_args(); json_set(ca2, "key", json_copy(key));
            json_set(ca2, "value", json_number(1));
            r = fa9_call(ctx, "counts_set", ca2);
            if (fa9_is_raise(r)) { json_free(key); json_free(existing); return r; }
            json_free(r);
            json_t *fa2 = fa9_args(); json_set(fa2, "key", json_copy(key));
            r = fa9_call(ctx, "schedule_flush", fa2);
            if (fa9_is_raise(r)) { json_free(key); json_free(existing); return r; }
            json_free(r);
            json_free(key); json_free(existing);
            return json_null();
        }
        json_t *ca3 = fa9_args(); json_set(ca3, "key", json_copy(key));
        json_set(ca3, "value", json_number(next_count));
        json_t *r = fa9_call(ctx, "counts_set", ca3);
        if (fa9_is_raise(r)) { json_free(key); json_free(existing); return r; }
        json_free(r);
        json_t *fa3 = fa9_args(); json_set(fa3, "key", json_copy(key));
        r = fa9_call(ctx, "schedule_flush", fa3);
        if (fa9_is_raise(r)) { json_free(key); json_free(existing); return r; }
        json_free(r);
        (void)chunk;
        json_free(key); json_free(existing);
        return json_null();
    }
    /* existing is None: _start_batch */
    json_free(existing);
    json_t *sa = fa9_args(); json_set(sa, "key", json_copy(key));
    json_t *r = fa9_call(ctx, "batches_set", sa);
    if (fa9_is_raise(r)) { json_free(key); return r; }
    json_free(r);
    json_t *ca2 = fa9_args(); json_set(ca2, "key", json_copy(key));
    json_set(ca2, "value", json_number(1));
    r = fa9_call(ctx, "counts_set", ca2);
    if (fa9_is_raise(r)) { json_free(key); return r; }
    json_free(r);
    json_t *fa2 = fa9_args(); json_set(fa2, "key", json_copy(key));
    r = fa9_call(ctx, "schedule_flush", fa2);
    if (fa9_is_raise(r)) { json_free(key); return r; }
    json_free(r);
    json_free(key);
    return json_null();
}

/* PoP: fa9_enqueue_media_event @ plugins/platforms/feishu/adapter.py:FeishuAdapter._enqueue_media_event */
static json_t *fa9_enqueue_media_event(fa9_ctx_t *ctx, const json_t *args) {
    const json_t *ev = json_obj_get(args, "event");
    json_t *ka = fa9_args();
    json_t *key = fa9_call(ctx, "media_key", ka);
    if (fa9_is_raise(key)) return key;
    json_t *ga = fa9_args(); json_set(ga, "key", json_copy(key));
    json_t *existing = fa9_call(ctx, "mbatches_get", ga);
    if (fa9_is_raise(existing)) { json_free(key); return existing; }
    bool has = !fa9_is_null(existing);
    if (!has) {
        if (has) {
            json_t *fa = fa9_args(); json_set(fa, "key", json_copy(key));
            json_t *fr = fa9_call(ctx, "mflush_now", fa);
            if (fa9_is_raise(fr)) { json_free(key); json_free(existing); return fr; }
            json_free(fr);
        }
        json_t *sa = fa9_args(); json_set(sa, "key", json_copy(key));
        json_t *r = fa9_call(ctx, "mbatches_set", sa);
        if (fa9_is_raise(r)) { json_free(key); json_free(existing); return r; }
        json_free(r);
        json_t *fa2 = fa9_args(); json_set(fa2, "key", json_copy(key));
        r = fa9_call(ctx, "schedule_mflush", fa2);
        if (fa9_is_raise(r)) { json_free(key); json_free(existing); return r; }
        json_free(r);
        json_free(key); json_free(existing);
        return json_null();
    }
    json_t *ca = fa9_args();
    json_t *comp = fa9_call(ctx, "media_compatible", ca);
    if (fa9_is_raise(comp)) { json_free(key); json_free(existing); return comp; }
    bool compat = fa9_truthy(comp);
    json_free(comp);
    if (!compat) {
        json_t *fa = fa9_args(); json_set(fa, "key", json_copy(key));
        json_t *fr = fa9_call(ctx, "mflush_now", fa);
        if (fa9_is_raise(fr)) { json_free(key); json_free(existing); return fr; }
        json_free(fr);
        json_t *sa = fa9_args(); json_set(sa, "key", json_copy(key));
        json_t *r = fa9_call(ctx, "mbatches_set", sa);
        if (fa9_is_raise(r)) { json_free(key); json_free(existing); return r; }
        json_free(r);
        json_t *fa2 = fa9_args(); json_set(fa2, "key", json_copy(key));
        r = fa9_call(ctx, "schedule_mflush", fa2);
        if (fa9_is_raise(r)) { json_free(key); json_free(existing); return r; }
        json_free(r);
        json_free(key); json_free(existing);
        return json_null();
    }
    /* merge: optional _merge_caption, then schedule */
    const json_t *et = ev ? json_obj_get(ev, "text") : NULL;
    if (fa9_truthy(et)) {
        json_t *ma = fa9_args();
        const json_t *xt = json_obj_get(existing, "text");
        fa9_set_s(ma, "existing_text", fa9_str_or(xt, ""));
        fa9_set_s(ma, "event_text", fa9_str_or(et, ""));
        json_t *mr = fa9_call(ctx, "merge_caption", ma);
        if (fa9_is_raise(mr)) { json_free(key); json_free(existing); return mr; }
        json_free(mr);
    }
    json_t *fa3 = fa9_args(); json_set(fa3, "key", json_copy(key));
    json_t *r = fa9_call(ctx, "schedule_mflush", fa3);
    if (fa9_is_raise(r)) { json_free(key); json_free(existing); return r; }
    json_free(r);
    json_free(key); json_free(existing);
    return json_null();
}

/* PoP: fa9_feishu_send_with_retry @ plugins/platforms/feishu/adapter.py:FeishuAdapter._feishu_send_with_retry */
static bool fa9_post_invalid(const char *msg) {
    static const char *needle = "content format of the post type is incorrect";
    size_t nl = strlen(needle);
    if (!msg) return false;
    size_t ml = strlen(msg);
    if (ml < nl) return false;
    for (size_t i = 0; i + nl <= ml; i++) {
        size_t j = 0;
        for (; j < nl; j++) {
            char a = msg[i + j], b = needle[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (a != b) break;
        }
        if (j == nl) return true;
    }
    return false;
}
static json_t *fa9_feishu_send_with_retry(fa9_ctx_t *ctx, const json_t *args) {
    const json_t *chat = json_obj_get(args, "chat_id");
    const json_t *mtype = json_obj_get(args, "msg_type");
    const json_t *payload = json_obj_get(args, "payload");
    const json_t *meta = json_obj_get(args, "metadata");
    const char *mt = fa9_str_or(mtype, "");
    const json_t *reply = json_obj_get(args, "reply_to");
    bool active_reply = !fa9_is_null(reply) && fa9_truthy(reply);
    bool is_post = strcmp(mt, "post") == 0;
    for (int attempt = 0; attempt < 3; attempt++) {
        json_t *a = fa9_args();
        fa9_set_copy(a, "chat_id", chat);
        fa9_set_copy(a, "msg_type", mtype);
        fa9_set_copy(a, "payload", payload);
        if (active_reply && reply) json_set(a, "reply_to", json_copy(reply));
        else json_set(a, "reply_to", json_null());
        fa9_set_copy(a, "metadata", meta);
        json_t *resp = fa9_call(ctx, "send_raw", a);
        if (fa9_is_raise(resp)) {
            const char *k = json_get_str(resp, "__raise__", "Exception");
            if (fa9_is_base_exc(k)) return resp; /* except Exception does not catch */
            const char *msg = json_get_str(resp, "message", "");
            if (is_post && fa9_post_invalid(msg)) return resp;
            if (attempt >= 2) return resp;
            json_t *lg = fa9_call(ctx, "log_warn", fa9_args());
            if (fa9_is_raise(lg)) { json_free(resp); return lg; }
            json_free(lg);
            json_t *sa = fa9_args();
            json_set(sa, "delay", json_number((double)(1 << attempt)));
            json_t *sl = fa9_call(ctx, "sleep", sa);
            if (fa9_is_raise(sl)) { json_free(resp); return sl; }
            json_free(sl); json_free(resp);
            continue;
        }
        /* if active_reply_to and not _response_succeeded(response) */
        if (active_reply) {
            json_t *sa = fa9_args();
            json_t *ok = fa9_call(ctx, "response_succeeded", sa);
            if (fa9_is_raise(ok)) { json_free(resp); return ok; }
            bool succ = fa9_truthy(ok);
            json_free(ok);
            if (!succ) {
                double code = fa9_num(json_obj_get(resp, "code"), 0.0);
                bool fallback = (code == 230011.0 || code == 231003.0);
                if (fallback) {
                    const json_t *tid = meta ? json_obj_get(meta, "thread_id") : NULL;
                    json_t *lg = fa9_call(ctx, "log_warn", fa9_args());
                    if (fa9_is_raise(lg)) { json_free(resp); return lg; }
                    json_free(lg);
                    if (fa9_truthy(tid)) return resp;
                    /* active_reply_to = None; response = await _raw(None) */
                    active_reply = false;
                    json_t *a2 = fa9_args();
                    fa9_set_copy(a2, "chat_id", chat);
                    fa9_set_copy(a2, "msg_type", mtype);
                    fa9_set_copy(a2, "payload", payload);
                    json_set(a2, "reply_to", json_null());
                    fa9_set_copy(a2, "metadata", meta);
                    json_t *resp2 = fa9_call(ctx, "send_raw", a2);
                    if (fa9_is_raise(resp2)) {
                        const char *k2 = json_get_str(resp2, "__raise__", "Exception");
                        json_free(resp);
                        if (fa9_is_base_exc(k2)) return resp2;
                        const char *msg2 = json_get_str(resp2, "message", "");
                        if (is_post && fa9_post_invalid(msg2)) return resp2;
                        if (attempt >= 2) return resp2;
                        json_t *lg2 = fa9_call(ctx, "log_warn", fa9_args());
                        if (fa9_is_raise(lg2)) { json_free(resp2); return lg2; }
                        json_free(lg2);
                        json_t *sl2 = fa9_args();
                        json_set(sl2, "delay", json_number((double)(1 << attempt)));
                        json_t *ss = fa9_call(ctx, "sleep", sl2);
                        if (fa9_is_raise(ss)) { json_free(resp2); return ss; }
                        json_free(ss); json_free(resp2);
                        continue;
                    }
                    json_free(resp);
                    return resp2;
                }
            }
        }
        return resp; /* adopted */
    }
    return fa9_err("RuntimeError", "Feishu send failed");
}

/* PoP: fa9_download_remote_document @ plugins/platforms/feishu/adapter.py:FeishuAdapter._download_remote_document */
static json_t *fa9_download_remote_document(fa9_ctx_t *ctx, const json_t *args) {
    const json_t *url = json_obj_get(args, "file_url");
    const char *us = fa9_str_or(url, "");
    json_t *sa = fa9_args(); fa9_set_s(sa, "url", us);
    json_t *safe = fa9_call(ctx, "is_safe", sa);
    if (fa9_is_raise(safe)) return safe;
    if (!fa9_truthy(safe)) {
        json_free(safe);
        char msg[256];
        char shorturl[81];
        snprintf(shorturl, sizeof shorturl, "%s", us);
        snprintf(msg, sizeof msg, "Blocked unsafe URL (SSRF protection): %s", shorturl);
        return fa9_err("ValueError", msg);
    }
    json_free(safe);
    json_t *ce = fa9_call(ctx, "client_enter", fa9_args());
    if (fa9_is_raise(ce)) return ce;
    json_free(ce);
    json_t *se = fa9_call(ctx, "stream_enter", fa9_args());
    if (fa9_is_raise(se)) {
        json_t *cx = fa9_call(ctx, "client_exit", fa9_args());
        if (fa9_is_raise(cx)) return cx;
        json_free(cx);
        return se;
    }
    json_free(se);
    json_t *pending = NULL;
    const char *cts = "";
    char ctb[512];
    json_t *body = NULL;
    json_t *rs = fa9_call(ctx, "raise_status", fa9_args());
    if (fa9_is_raise(rs)) pending = rs; else json_free(rs);
    if (!pending) {
        json_t *hga = fa9_args();
        fa9_set_s(hga, "name", "Content-Type");
        json_t *hg = fa9_call(ctx, "headers_get", hga);
        if (fa9_is_raise(hg)) pending = hg;
        else { cts = fa9_str_or(hg, ""); snprintf(ctb, sizeof ctb, "%s", cts); cts = ctb; json_free(hg); }
    }
    if (!pending) {
        body = fa9_call(ctx, "read_body", fa9_args());
        if (fa9_is_raise(body)) { pending = body; body = NULL; }
    }
    /* finally: stream exit then client exit */
    json_t *sx = fa9_call(ctx, "stream_exit", fa9_args());
    if (fa9_is_raise(sx)) {
        if (pending) json_free(pending);
        if (body) json_free(body);
        json_t *cx = fa9_call(ctx, "client_exit", fa9_args());
        if (fa9_is_raise(cx)) return cx;
        json_free(cx);
        return sx;
    }
    json_free(sx);
    json_t *cx = fa9_call(ctx, "client_exit", fa9_args());
    if (fa9_is_raise(cx)) {
        if (pending) json_free(pending);
        if (body) json_free(body);
        return cx;
    }
    json_free(cx);
    if (pending) return pending;
    json_t *da = fa9_args();
    fa9_set_s(da, "url", us);
    fa9_set_s(da, "content_type", cts);
    fa9_set_copy(da, "default_name", json_obj_get(args, "preferred_name"));
    fa9_set_copy(da, "default_ext", json_obj_get(args, "default_ext"));
    json_t *fn = fa9_call(ctx, "derive_name", da);
    if (fa9_is_raise(fn)) { if (body) json_free(body); return fn; }
    json_t *ca = fa9_args();
    const char *fns = fa9_str_or(fn, "");
    fa9_set_s(ca, "filename", fns);
    json_t *cached = fa9_call(ctx, "cache_doc", ca);
    if (fa9_is_raise(cached)) { json_free(fn); if (body) json_free(body); return cached; }
    json_t *out = json_array();
    json_append(out, cached);
    json_append(out, json_copy(fn));
    json_free(fn);
    if (body) json_free(body);
    return out;
}

/* PoP: fa9_drain_pending_inbound_events @ plugins/platforms/feishu/adapter.py:FeishuAdapter._drain_pending_inbound_events */
static json_t *fa9_drain_pending_inbound_events(fa9_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *pending = NULL;
    for (int iter = 0; iter < 6; iter++) {
        json_t *r = fa9_call(ctx, "running", fa9_args());
        if (fa9_is_raise(r)) { pending = r; goto drain_finally; }
        bool running = fa9_truthy(r);
        json_free(r);
        if (!running) {
            /* dropped = len(_take_all()) */
            json_t *le = fa9_call(ctx, "lock_enter", fa9_args());
            if (fa9_is_raise(le)) { pending = le; goto drain_finally; }
            json_free(le);
            json_t *batch = fa9_call(ctx, "take_batch", fa9_args());
            if (fa9_is_raise(batch)) { pending = batch; goto drain_finally; }
            size_t dropped = json_len(batch);
            json_free(batch);
            json_t *lx = fa9_call(ctx, "lock_exit", fa9_args());
            if (fa9_is_raise(lx)) { pending = lx; goto drain_finally; }
            json_free(lx);
            if (dropped > 0) {
                json_t *lg = fa9_call(ctx, "log_warn", fa9_args());
                if (fa9_is_raise(lg)) { pending = lg; goto drain_finally; }
                json_free(lg);
            }
            goto drain_finally;
        }
        json_t *lc = fa9_call(ctx, "loop_accepts", fa9_args());
        if (fa9_is_raise(lc)) { pending = lc; goto drain_finally; }
        bool accepts = fa9_truthy(lc);
        json_free(lc);
        if (accepts) {
            json_t *le = fa9_call(ctx, "lock_enter", fa9_args());
            if (fa9_is_raise(le)) { pending = le; goto drain_finally; }
            json_free(le);
            json_t *batch = fa9_call(ctx, "take_batch", fa9_args());
            if (fa9_is_raise(batch)) { pending = batch; goto drain_finally; }
            json_t *lx = fa9_call(ctx, "lock_exit", fa9_args());
            if (fa9_is_raise(lx)) { json_free(batch); pending = lx; goto drain_finally; }
            json_free(lx);
            if (json_len(batch) == 0) {
                json_free(batch);
                json_t *le2 = fa9_call(ctx, "lock_enter", fa9_args());
                if (fa9_is_raise(le2)) { pending = le2; goto drain_finally; }
                json_free(le2);
                json_t *ie = fa9_call(ctx, "is_empty", fa9_args());
                if (fa9_is_raise(ie)) { pending = ie; goto drain_finally; }
                bool empty = fa9_truthy(ie);
                json_free(ie);
                json_t *lx2 = fa9_call(ctx, "lock_exit", fa9_args());
                if (fa9_is_raise(lx2)) { pending = lx2; goto drain_finally; }
                json_free(lx2);
                if (empty) goto drain_finally;
                continue;
            }
            json_t *requeue = json_array();
            for (size_t i = 0; i < json_len(batch); i++) {
                json_t *ea = fa9_args();
                json_set(ea, "event", json_copy(json_get(batch, i)));
                json_t *he = fa9_call(ctx, "handle_event", ea);
                if (fa9_is_raise(he)) { json_free(batch); json_free(requeue); pending = he; goto drain_finally; }
                json_free(he);
                json_t *sb = fa9_call(ctx, "submit_on_loop", fa9_args());
                if (fa9_is_raise(sb)) { json_free(batch); json_free(requeue); pending = sb; goto drain_finally; }
                bool ok = fa9_truthy(sb);
                json_free(sb);
                if (!ok) json_append(requeue, json_copy(json_get(batch, i)));
            }
            size_t submitted = json_len(batch) - json_len(requeue);
            json_free(batch);
            size_t nre = json_len(requeue);
            if (nre > 0) {
                json_t *le3 = fa9_call(ctx, "lock_enter", fa9_args());
                if (fa9_is_raise(le3)) { json_free(requeue); pending = le3; goto drain_finally; }
                json_free(le3);
                json_t *qa = fa9_args(); json_set(qa, "items", requeue); /* adopted */
                json_t *qr = fa9_call(ctx, "requeue", qa);
                if (fa9_is_raise(qr)) { pending = qr; goto drain_finally; }
                json_free(qr);
                json_t *lx3 = fa9_call(ctx, "lock_exit", fa9_args());
                if (fa9_is_raise(lx3)) { pending = lx3; goto drain_finally; }
                json_free(lx3);
            } else {
                json_free(requeue);
            }
            if (submitted > 0) {
                json_t *lg = fa9_call(ctx, "log_info", fa9_args());
                if (fa9_is_raise(lg)) { pending = lg; goto drain_finally; }
                json_free(lg);
            }
            if (nre == 0) {
                json_t *le4 = fa9_call(ctx, "lock_enter", fa9_args());
                if (fa9_is_raise(le4)) { pending = le4; goto drain_finally; }
                json_free(le4);
                json_t *ie2 = fa9_call(ctx, "is_empty", fa9_args());
                if (fa9_is_raise(ie2)) { pending = ie2; goto drain_finally; }
                bool empty2 = fa9_truthy(ie2);
                json_free(ie2);
                json_t *lx4 = fa9_call(ctx, "lock_exit", fa9_args());
                if (fa9_is_raise(lx4)) { pending = lx4; goto drain_finally; }
                json_free(lx4);
                if (empty2) goto drain_finally;
            }
            continue;
        }
        /* loop not accepting: sleep then poll again */
        json_t *sl = fa9_args(); json_set(sl, "delay", json_number(0.25));
        json_t *sr = fa9_call(ctx, "poll_sleep", sl);
        if (fa9_is_raise(sr)) { pending = sr; goto drain_finally; }
        json_free(sr);
    }
drain_finally:
    {
        json_t *le = fa9_call(ctx, "lock_enter", fa9_args());
        if (fa9_is_raise(le)) { if (pending) json_free(pending); return le; }
        json_free(le);
        json_t *lx = fa9_call(ctx, "lock_exit", fa9_args());
        if (fa9_is_raise(lx)) { if (pending) json_free(pending); return lx; }
        json_free(lx);
    }
    if (pending) return pending;
    return json_null();
}
