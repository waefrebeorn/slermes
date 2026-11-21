/*
 * feishu_adapter_flow12.c — Feishu/Lark adapter "remainders" FLOW cores of
 * plugins/platforms/feishu/adapter.py (hermes-agent).
 *
 * TWELFTH remainder port beyond feishu_adapter_flow.c .. flow11. Ports 14
 * genuinely unclaimed defs (dotted-name-aware claim parser confirmed none are
 * claimed by flows 1-11):
 *   _poll_registration, _pop_text_batch, _post_registration, _prepare_client,
 *   _probe_bot_http, _process_inbound_message, _qr_register_inner,
 *   _reaction_call, _read_binary_response, _read_limited_feishu_webhook_body
 *   + _should_batch_media_event, _resolve_media_message_type, _queue_empty,
 *   _render_qr (4 extra unclaimed helpers).
 *
 * Universal seam: every seamed call enters through ONE callback
 * fa12_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting the
 * identical {"name","args","ret"} event from the SAME scripts. args is ADOPTED
 * into the trace event; the return is a NEW owned json_t (NULL == Python None;
 * {"__raise__":cls,"message":...} == a raised exception). A missing/exhausted
 * script is a fail-closed RuntimeError on both sides.
 *
 * FULL BASEEXCEPTION PASSTHROUGH: fa12_is_base_exc + fa12_raise_is_base name the
 * family that bypasses `except Exception`; those raises propagate verbatim.
 *
 * Representation contract (shared with tests/oracle_feishu_adapter_flow12.py):
 * - NULL json_t* == Python None; JSON_NULL node is a real null value.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__":cls,"message":str(exc)}; a scripted raise is
 *   {"__script__":true,"raise":{"type":cls,"message":msg}}.
 *   A partial aiohttp read is modelled as {"__partial__":"..."}.
 * - Byte payloads are represented by ASCII JSON strings (oracle canon decodes
 *   bytes to str). Fixture domain: ASCII scalars, JSON dict/list values, POSIX
 *   paths, integral-int numbers. State is {}.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and fa12_-prefixed.
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
typedef struct fa12_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} fa12_ctx_t;

/* ── basic helpers ── */
static bool fa12_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool fa12_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool fa12_is_base_exc(const char *kind) {
    return kind && (
        strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "asyncio.CancelledError") == 0);
}
static bool fa12_raise_is_base(const json_t *r) {
    return fa12_is_raise(r) && fa12_is_base_exc(json_get_str(r, "__raise__", ""));
}
/* OSError / UnicodeDecodeError / JSONDecodeError family. */
static bool fa12_is_osfamily(const char *kind) {
    return kind && (
        strcmp(kind, "OSError") == 0 || strcmp(kind, "FileNotFoundError") == 0
        || strcmp(kind, "IsADirectoryError") == 0 || strcmp(kind, "PermissionError") == 0
        || strcmp(kind, "NotADirectoryError") == 0 || strcmp(kind, "TimeoutError") == 0
        || strcmp(kind, "ConnectionError") == 0 || strcmp(kind, "URLError") == 0
        || strcmp(kind, "UnicodeDecodeError") == 0 || strcmp(kind, "UnicodeError") == 0
        || strcmp(kind, "JSONDecodeError") == 0);
}
/* _probe_bot_http catches (URLError, OSError, KeyError, json.JSONDecodeError). */
static bool fa12_probe_handled(const char *kind) {
    return fa12_is_osfamily(kind) || (kind && strcmp(kind, "KeyError") == 0);
}
static double fa12_as_num(const json_t *v, double d) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : d;
}
static bool fa12_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *fa12_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static double fa12_num(const json_t *v, double d) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : d;
}
static int fa12_to_int(const json_t *v, int d) {
    if (!v) return d;
    if (v->type == JSON_NUMBER) return (int)(long long)v->num_val;
    if (v->type == JSON_STRING) return (int)strtol(v->str_val, NULL, 10);
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return d;
}
static bool fa12_bool(const json_t *v, bool d) {
    return (v && v->type == JSON_BOOL) ? v->bool_val : d;
}
static const char *fa12_str_or(const json_t *v, const char *d) {
    const char *s = fa12_str(v);
    return s ? s : d;
}
static json_t *fa12_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *fa12_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *e = fa12_err(type, msg);
    const char *partial = json_get_str(spec, "partial", NULL);
    if (partial) json_set_hdr(e, "partial", partial);
    return e;
}
/* The one universal seam. args is ADOPTED; the return is a fresh owned node. */
static json_t *fa12_call(fa12_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = fa12_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = fa12_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = fa12_make_exc(raise);
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
static json_t *fa12_args(void) { return json_object(); }
static void fa12_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void fa12_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
/* Python str.strip() + optional lower, ASCII fixture domain. */
static const char *fa12_snorm(const char *s, char *out, size_t cap, bool lower) {
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
/* Python str.lower(), ASCII fixture domain (no strip). */
static const char *fa12_lower(const char *s, char *out, size_t cap) {
    size_t n = 0;
    const char *p = s ? s : "";
    while (*p && n + 1 < cap) {
        char c = *p++;
        out[n++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    out[n] = '\0';
    return out;
}
/* Python str(x) for the scalar fixture domain. */
static const char *fa12_pystr(const json_t *v, char *buf, size_t cap) {
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
static bool fa12_starts(const char *s, const char *p) {
    return s && p && strncmp(s, p, strlen(p)) == 0;
}

/* ══ PoP functions ══ */

/* PoP: fa12__poll_registration @ plugins/platforms/feishu/adapter.py:_poll_registration */
static json_t *fa12__poll_registration(fa12_ctx_t *ctx, const json_t *args) {
    const char *device_code = fa12_str_or(json_obj_get(args, "device_code"), "");
    double interval = fa12_num(json_obj_get(args, "interval"), 0);
    double expire_in = fa12_num(json_obj_get(args, "expire_in"), 0);
    char cur_domain[256];
    fa12_snorm(fa12_str_or(json_obj_get(args, "domain"), "feishu"), cur_domain, sizeof cur_domain, false);
    bool domain_switched = false;
    int poll_count = 0;

    /* deadline = time.monotonic() + expire_in; then each loop check calls
     * time.monotonic() again (Python evaluates it at the `while`). */
    json_t *ma = fa12_args();
    json_t *t0 = fa12_call(ctx, "monotonic", ma);
    if (fa12_is_raise(t0)) return t0;
    double deadline = fa12_as_num(t0, 0) + expire_in; json_free(t0);

    for (;;) {
        json_t *tc = fa12_call(ctx, "monotonic", fa12_args());
        if (fa12_is_raise(tc)) return tc;
        double now = fa12_as_num(tc, 0); json_free(tc);
        if (!(now < deadline)) break;
        /* base_url = _accounts_base_url(current_domain) */
        json_t *ba = fa12_args(); fa12_set_s(ba, "domain", cur_domain);
        json_t *base = fa12_call(ctx, "accounts_base_url", ba);
        const char *base_s = fa12_str_or(base, "");
        if (fa12_is_raise(base)) {
            if (fa12_raise_is_base(base)) return base;
            const char *k = json_get_str(base, "__raise__", "");
            bool handled = fa12_is_osfamily(k);
            json_free(base);
            if (handled) {
                json_t *sa = fa12_args(); json_set(sa, "interval", json_number(interval));
                json_t *sl = fa12_call(ctx, "sleep", sa);
                if (fa12_is_raise(sl)) return sl; json_free(sl);
                continue;
            }
            /* other Exception propagates verbatim */
            return json_null(); /* unreachable: raises returned above */
        }
        char url[4096];
        snprintf(url, sizeof url, "%s%s", base_s, "/oauth/v1/app/registration");

        /* body for this poll */
        json_t *body = json_object();
        json_set_hdr(body, "action", "poll");
        json_set_hdr(body, "device_code", device_code);
        json_set_hdr(body, "tp", "ob_app");
        json_t *pa = fa12_args();
        fa12_set_s(pa, "base_url", base_s);
        json_set(pa, "body", body);
        json_t *res = fa12_call(ctx, "post_registration", pa);
        json_free(base);
        if (fa12_is_raise(res)) {
            if (fa12_raise_is_base(res)) return res;
            const char *k = json_get_str(res, "__raise__", "");
            bool handled = fa12_is_osfamily(k);
            json_free(res);
            if (handled) {
                json_t *sa = fa12_args(); json_set(sa, "interval", json_number(interval));
                json_t *sl = fa12_call(ctx, "sleep", sa);
                if (fa12_is_raise(sl)) return sl; json_free(sl);
                continue;
            }
            return res; /* non-handled Exception propagates */
        }
        poll_count++;

        /* user_info = res.get("user_info") or {} */
        const json_t *user_info = json_obj_get(res, "user_info");
        if (!fa12_truthy(user_info)) user_info = NULL;
        const char *brand = user_info ? fa12_str_or(json_obj_get(user_info, "tenant_brand"), "") : "";
        if (strcmp(brand, "lark") == 0 && !domain_switched) {
            snprintf(cur_domain, sizeof cur_domain, "lark");
            domain_switched = true;
        }
        const json_t *cid = json_obj_get(res, "client_id");
        const json_t *csec = json_obj_get(res, "client_secret");
        if (fa12_truthy(cid) && fa12_truthy(csec)) {
            json_t *out = json_object();
            fa12_set_copy(out, "app_id", cid);
            fa12_set_copy(out, "app_secret", csec);
            fa12_set_s(out, "domain", cur_domain);
            json_t *oid = user_info ? json_obj_get(user_info, "open_id") : NULL;
            json_set(out, "open_id", oid ? json_copy(oid) : json_null());
            json_free(res);
            return out;
        }
        const char *error = fa12_str_or(json_obj_get(res, "error"), "");
        json_free(res);
        if (strcmp(error, "access_denied") == 0 || strcmp(error, "expired_token") == 0) {
            return json_null();
        }
        /* authorization_pending / unknown: time.sleep(interval) then next check */
        json_t *sa = fa12_args(); json_set(sa, "interval", json_number(interval));
        json_t *sl = fa12_call(ctx, "sleep", sa);
        if (fa12_is_raise(sl)) return sl; json_free(sl);
    }
    return json_null();
}

/* PoP: fa12__pop_text_batch @ plugins/platforms/feishu/adapter.py:FeishuAdapter._pop_text_batch */
static json_t *fa12__pop_text_batch(fa12_ctx_t *ctx, const json_t *args) {
    const char *key = fa12_str_or(json_obj_get(args, "key"), "");
    json_t *ca = fa12_args(); fa12_set_s(ca, "key", key);
    json_t *cnt = fa12_call(ctx, "pop_count", ca);
    if (fa12_is_raise(cnt)) return cnt; json_free(cnt);
    json_t *ba = fa12_args(); fa12_set_s(ba, "key", key);
    return fa12_call(ctx, "pop_batch", ba);
}

/* PoP: fa12__post_registration @ plugins/platforms/feishu/adapter.py:_post_registration */
static json_t *fa12__post_registration(fa12_ctx_t *ctx, const json_t *args) {
    const char *base_url = fa12_str_or(json_obj_get(args, "base_url"), "");
    const json_t *body = json_obj_get(args, "body");
    char url[4096];
    snprintf(url, sizeof url, "%s%s", base_url, "/oauth/v1/app/registration");
    json_t *a = fa12_args();
    fa12_set_s(a, "url", url);
    json_set(a, "body", body ? json_copy(body) : json_object());
    return fa12_call(ctx, "post_form", a);
}

/* PoP: fa12__prepare_client @ plugins/platforms/feishu/adapter.py:FeishuAdapter._prepare_client */
static json_t *fa12__prepare_client(fa12_ctx_t *ctx, const json_t *args) {
    json_t *da = fa12_args();
    json_set(da, "domain_name", json_copy(json_obj_get(args, "domain_name")));
    json_t *dom = fa12_call(ctx, "sdk_domain", da);
    if (fa12_is_raise(dom)) return dom;
    json_t *ca = fa12_args();
    json_set(ca, "domain", json_copy(dom));
    json_t *client = fa12_call(ctx, "build_lark_client", ca);
    if (fa12_is_raise(client)) { json_free(dom); return client; }
    json_free(client);
    json_t *handler = fa12_call(ctx, "build_event_handler", fa12_args());
    if (fa12_is_raise(handler)) { json_free(dom); return handler; }
    if (!fa12_truthy(handler)) {
        json_free(dom); json_free(handler);
        return fa12_err("RuntimeError", "failed to build Feishu event handler");
    }
    json_free(handler);
    return dom;
}

/* PoP: fa12__probe_bot_http @ plugins/platforms/feishu/adapter.py:_probe_bot_http */
static json_t *fa12__probe_bot_http(fa12_ctx_t *ctx, const json_t *args) {
    const char *app_id = fa12_str_or(json_obj_get(args, "app_id"), "");
    const char *app_secret = fa12_str_or(json_obj_get(args, "app_secret"), "");
    const char *domain = fa12_str_or(json_obj_get(args, "domain"), "");
    json_t *ba = fa12_args(); fa12_set_s(ba, "domain", domain);
    json_t *base = fa12_call(ctx, "onboard_open_base_url", ba);
    if (fa12_is_raise(base)) return base;
    json_free(base);

    /* token request */
    json_t *data = json_object();
    json_set_hdr(data, "app_id", app_id);
    json_set_hdr(data, "app_secret", app_secret);
    json_t *ta = fa12_args();
    fa12_set_s(ta, "path", "/open-apis/auth/v3/tenant_access_token/internal");
    json_set(ta, "data", data);
    json_set(ta, "extra_headers", json_object());
    json_t *token_res = fa12_call(ctx, "probe_get_json", ta);
    if (fa12_is_raise(token_res)) {
        if (fa12_raise_is_base(token_res)) return token_res;
        const char *k = json_get_str(token_res, "__raise__", "");
        bool handled = fa12_probe_handled(k); json_free(token_res);
        return handled ? json_null() : fa12_err(k, "unhandled");
    }
    const char *access_token = fa12_str_or(json_obj_get(token_res, "tenant_access_token"), "");
    json_free(token_res);
    if (!access_token[0]) return json_null();

    /* bot-info request */
    char bearer[8192];
    snprintf(bearer, sizeof bearer, "Bearer %s", access_token);
    json_t *ha = fa12_args(); fa12_set_s(ha, "path", "/open-apis/bot/v3/info");
    json_set(ha, "data", json_null());
    json_t *hdrs = json_object(); json_set_hdr(hdrs, "Authorization", bearer);
    json_set(ha, "extra_headers", hdrs);
    json_t *bot_res = fa12_call(ctx, "probe_get_json", ha);
    if (fa12_is_raise(bot_res)) {
        if (fa12_raise_is_base(bot_res)) return bot_res;
        const char *k = json_get_str(bot_res, "__raise__", "");
        bool handled = fa12_probe_handled(k); json_free(bot_res);
        return handled ? json_null() : fa12_err(k, "unhandled");
    }
    json_t *pa = fa12_args(); json_set(pa, "data", bot_res);
    json_t *parsed = fa12_call(ctx, "parse_bot_response", pa);
    if (fa12_is_raise(parsed)) {
        if (fa12_raise_is_base(parsed)) return parsed;
        const char *k = json_get_str(parsed, "__raise__", "");
        bool handled = fa12_probe_handled(k); json_free(parsed);
        return handled ? json_null() : fa12_err(k, "unhandled");
    }
    return parsed;
}

/* PoP: fa12__process_inbound_message @ plugins/platforms/feishu/adapter.py:FeishuAdapter._process_inbound_message */
static json_t *fa12__process_inbound_message(fa12_ctx_t *ctx, const json_t *args) {
    const json_t *data = json_obj_get(args, "data");
    const json_t *message = json_obj_get(args, "message");
    const json_t *sender_id = json_obj_get(args, "sender_id");
    const char *chat_type = fa12_str_or(json_obj_get(args, "chat_type"), "");
    const char *message_id = fa12_str_or(json_obj_get(args, "message_id"), "");
    bool is_bot = fa12_bool(json_obj_get(args, "is_bot"), false);

    /* text, inbound_type, media_urls, media_types, media_text_inlined, mentions =
     *   await self._extract_message_content(message) */
    json_t *ea = fa12_args(); fa12_set_copy(ea, "message", message);
    json_t *ex = fa12_call(ctx, "extract_content", ea);
    if (fa12_is_raise(ex)) return ex;
    const char *text = fa12_str_or(json_get(ex, 0), "");
    const char *inbound_type = fa12_str_or(json_get(ex, 1), "");
    const json_t *media_urls = json_get(ex, 2);
    const json_t *media_types = json_get(ex, 3);
    const json_t *media_text_inlined = json_get(ex, 4);
    const json_t *mentions = json_get(ex, 5);

    char textbuf[16384];
    snprintf(textbuf, sizeof textbuf, "%s", text);
    text = textbuf;

    if (strcmp(inbound_type, "text") == 0) {
        json_t *sa = fa12_args();
        fa12_set_s(sa, "text", text);
        fa12_set_copy(sa, "mentions", mentions);
        json_t *stripped = fa12_call(ctx, "strip_edge_mentions", sa);
        if (fa12_is_raise(stripped)) {  return stripped; }
        snprintf(textbuf, sizeof textbuf, "%s", fa12_str_or(stripped, ""));
        
        text = textbuf;
        if (fa12_starts(text, "/")) inbound_type = "command";
    }
    /* post-strip guard */
    if (strcmp(inbound_type, "text") == 0 && !text[0] && !fa12_truthy(media_urls)) {
         return json_null();
    }
    if (strcmp(inbound_type, "command") != 0) {
        json_t *ha = fa12_args(); fa12_set_copy(ha, "mentions", mentions);
        json_t *hint = fa12_call(ctx, "build_mention_hint", ha);
        if (fa12_is_raise(hint)) {  return hint; }
        const char *hv = fa12_str_or(hint, "");
        if (hv[0]) {
            char hintbuf[16384];
            if (text[0]) snprintf(hintbuf, sizeof hintbuf, "%s\n\n%s", hv, text);
            else snprintf(hintbuf, sizeof hintbuf, "%s", hv);
            snprintf(textbuf, sizeof textbuf, "%s", hintbuf);
            text = textbuf;
        }
        
    }

    /* thread_id = getattr(message,"thread_id",None) or None */
    const json_t *thread_id = json_obj_get(message, "thread_id");
    if (!fa12_truthy(thread_id)) thread_id = NULL;
    /* reply_to_message_id = parent_id or upper_message_id or root_id or None */
    const json_t *reply_to = json_obj_get(message, "parent_id");
    if (!fa12_truthy(reply_to)) reply_to = json_obj_get(message, "upper_message_id");
    if (!fa12_truthy(reply_to)) reply_to = json_obj_get(message, "root_id");
    if (!fa12_truthy(reply_to)) reply_to = NULL;

    json_t *reply_text = NULL;
    if (reply_to) {
        json_t *fa = fa12_args(); fa12_set_copy(fa, "message_id", reply_to);
        json_t *rt = fa12_call(ctx, "fetch_message_text", fa);
        if (fa12_is_raise(rt)) {  return rt; }
        reply_text = rt; /* owned */
    }
    const char *chat_id = fa12_str_or(json_obj_get(message, "chat_id"), "");

    /* chat_info = await self.get_chat_info(chat_id) */
    json_t *ia = fa12_args(); fa12_set_s(ia, "chat_id", chat_id);
    json_t *chat_info = fa12_call(ctx, "get_chat_info", ia);
    if (fa12_is_raise(chat_info)) { if (reply_text)   return chat_info; }
    /* sender_profile = await self._resolve_sender_profile(sender_id, is_bot=is_bot) */
    json_t *pa = fa12_args(); fa12_set_copy(pa, "sender_id", sender_id);
    json_set(pa, "is_bot", json_bool(is_bot));
    json_t *profile = fa12_call(ctx, "resolve_sender_profile", pa);
    if (fa12_is_raise(profile)) { if (reply_text)    return profile; }
    /* source chat type */
    json_t *ta = fa12_args(); fa12_set_copy(ta, "chat_info", chat_info);
    fa12_set_s(ta, "event_chat_type", chat_type);
    json_t *sct = fa12_call(ctx, "resolve_source_chat_type", ta);
    if (fa12_is_raise(sct)) { if (reply_text)     return sct; }
    /* channel prompt seam fires AFTER build_source (it is a MessageEvent kwarg) */
    json_t *prompt = NULL;

    /* build_source(...) */
    const char *cname = fa12_str_or(json_obj_get(chat_info, "name"), "");
    char chat_name[4096];
    if (cname[0]) snprintf(chat_name, sizeof chat_name, "%s", cname);
    else if (chat_id[0]) snprintf(chat_name, sizeof chat_name, "%s", chat_id);
    else snprintf(chat_name, sizeof chat_name, "Feishu Chat");
    json_t *sa = fa12_args();
    fa12_set_s(sa, "chat_id", chat_id);
    fa12_set_s(sa, "chat_name", chat_name);
    fa12_set_copy(sa, "chat_type", sct);
    fa12_set_copy(sa, "user_id", json_obj_get(profile, "user_id"));
    fa12_set_copy(sa, "user_name", json_obj_get(profile, "user_name"));
    json_set(sa, "thread_id", thread_id ? json_copy(thread_id) : json_null());
    fa12_set_copy(sa, "user_id_alt", json_obj_get(profile, "user_id_alt"));
    json_set(sa, "is_bot", json_bool(is_bot));
    fa12_set_s(sa, "message_id", message_id);
    json_t *source = fa12_call(ctx, "build_source", sa);
    if (fa12_is_raise(source)) { if (reply_text)      return source; }
    /* channel prompt (MessageEvent kwarg, evaluated after build_source) */
    json_t *ca = fa12_args(); fa12_set_s(ca, "chat_id", chat_id);
    json_set(ca, "thread_id", thread_id ? json_copy(thread_id) : json_null());
    prompt = fa12_call(ctx, "resolve_channel_prompt", ca);
    if (fa12_is_raise(prompt)) { if (reply_text)       return prompt; }

    /* MessageEvent(...) */
    json_t *na = fa12_args();
    fa12_set_s(na, "text", text);
    fa12_set_s(na, "message_type", inbound_type);
    json_set(na, "source", source);
    json_set(na, "raw_message", data ? json_copy(data) : json_null());
    fa12_set_s(na, "message_id", message_id);
    json_set(na, "media_urls", media_urls ? json_copy(media_urls) : json_array());
    json_set(na, "media_types", media_types ? json_copy(media_types) : json_array());
    json_set(na, "media_text_inlined", media_text_inlined ? json_copy(media_text_inlined) : json_null());
    json_set(na, "reply_to_message_id", reply_to ? json_copy(reply_to) : json_null());
    json_set(na, "reply_to_text", reply_text ? json_copy(reply_text) : json_null());
    fa12_set_copy(na, "channel_prompt", prompt);
    fa12_set_s(na, "timestamp", "TS");
    json_t *normalized = fa12_call(ctx, "make_message_event", na);
    if (fa12_is_raise(normalized)) { if (reply_text)       return normalized; }

    /* await self._dispatch_inbound_event(normalized) */
    json_t *da = fa12_args(); json_set(da, "event", normalized);
    json_t *dr = fa12_call(ctx, "dispatch_inbound", da);
    if (fa12_is_raise(dr)) { if (reply_text)       return dr; }
    
    if (reply_text) 
         
    return json_null();
}

/* PoP: fa12__qr_register_inner @ plugins/platforms/feishu/adapter.py:_qr_register_inner */
static json_t *fa12__qr_register_inner(fa12_ctx_t *ctx, const json_t *args) {
    const char *initial_domain = fa12_str_or(json_obj_get(args, "initial_domain"), "feishu");
    double timeout = fa12_num(json_obj_get(args, "timeout_seconds"), 0);

    json_t *ia = fa12_args(); fa12_set_s(ia, "domain", initial_domain);
    json_t *r = fa12_call(ctx, "init_registration", ia);
    if (fa12_is_raise(r)) return r; json_free(r);
    json_t *ba = fa12_args(); fa12_set_s(ba, "domain", initial_domain);
    json_t *begin = fa12_call(ctx, "begin_registration", ba);
    if (fa12_is_raise(begin)) return begin;
    const char *qr_url = fa12_str_or(json_obj_get(begin, "qr_url"), "");
    const char *device_code = fa12_str_or(json_obj_get(begin, "device_code"), "");
    double interval = fa12_num(json_obj_get(begin, "interval"), 0);
    double expire_in = fa12_num(json_obj_get(begin, "expire_in"), 0);
    double poll_expire = expire_in < timeout ? expire_in : timeout;

    json_t *qa = fa12_args(); fa12_set_s(qa, "url", qr_url);
    json_t *rendered = fa12_call(ctx, "render_qr", qa);
    if (fa12_is_raise(rendered)) { json_free(begin); return rendered; }
    bool ok = fa12_truthy(rendered); json_free(rendered);
    if (!ok) {
        json_t *ha = fa12_args(); fa12_set_s(ha, "kind", "messaging");
        json_t *hint = fa12_call(ctx, "install_hint", ha);
        if (fa12_is_raise(hint)) { json_free(begin); return hint; }
        json_free(hint);
    }

    json_t *pa = fa12_args();
    fa12_set_s(pa, "device_code", device_code);
    json_set(pa, "interval", json_number(interval));
    json_set(pa, "expire_in", json_number(poll_expire));
    fa12_set_s(pa, "domain", initial_domain);
    json_t *result = fa12_call(ctx, "poll_registration", pa);
    json_free(begin);
    if (fa12_is_raise(result)) return result;
    if (!fa12_truthy(result)) { json_free(result); return json_null(); }

    const char *app_id = fa12_str_or(json_obj_get(result, "app_id"), "");
    const char *app_secret = fa12_str_or(json_obj_get(result, "app_secret"), "");
    const char *domain = fa12_str_or(json_obj_get(result, "domain"), initial_domain);
    json_t *pra = fa12_args();
    fa12_set_s(pra, "app_id", app_id);
    fa12_set_s(pra, "app_secret", app_secret);
    fa12_set_s(pra, "domain", domain);
    json_t *bot_info = fa12_call(ctx, "probe_bot", pra);
    if (fa12_is_raise(bot_info)) { json_free(result); return bot_info; }
    json_t *bi = (bot_info && bot_info->type == JSON_OBJECT) ? bot_info : NULL;
    json_set(result, "bot_name", bi ? json_copy(json_obj_get(bi, "bot_name")) : json_null());
    json_set(result, "bot_open_id", bi ? json_copy(json_obj_get(bi, "bot_open_id")) : json_null());
    if (bot_info) json_free(bot_info);
    return result;
}

/* PoP: fa12__reaction_call @ plugins/platforms/feishu/adapter.py:FeishuAdapter._reaction_call */
static json_t *fa12__reaction_call(fa12_ctx_t *ctx, const json_t *args) {
    const json_t *api = json_obj_get(args, "api");
    (void)json_obj_get(args, "verb");
    (void)json_obj_get(args, "message_id");
    (void)json_obj_get(args, "ident");
    /* try: */
    json_t *req = fa12_call(ctx, "build_request", fa12_args());
    if (fa12_is_raise(req)) {
        if (fa12_raise_is_base(req)) return req;
        json_free(req); return json_null();
    }
    json_t *ra = fa12_args();
    fa12_set_copy(ra, "api", api);
    json_set(ra, "request", req);
    json_t *resp = fa12_call(ctx, "run_blocking", ra);
    if (fa12_is_raise(resp)) {
        if (fa12_raise_is_base(resp)) return resp;
        json_free(resp); return json_null();
    }
    json_t *sa = fa12_args(); fa12_set_copy(sa, "response", resp);
    json_t *ok = fa12_call(ctx, "response_succeeded", sa);
    if (fa12_is_raise(ok)) {
        if (fa12_raise_is_base(ok)) { json_free(resp); return ok; }
        json_free(ok); json_free(resp); return json_null();
    }
    bool succeeded = fa12_truthy(ok); json_free(ok);
    if (succeeded) {
        const json_t *data = (resp && resp->type == JSON_OBJECT) ? json_obj_get(resp, "data") : NULL;
        json_t *out = fa12_truthy(data) ? json_copy(data) : json_bool(true);
        json_free(resp);
        return out;
    }
    json_free(resp);
    return json_null();
}

/* PoP: fa12__read_binary_response @ plugins/platforms/feishu/adapter.py:FeishuAdapter._read_binary_response */
static json_t *fa12__read_binary_response(fa12_ctx_t *ctx, const json_t *args) {
    const json_t *resp = json_obj_get(args, "response");
    const json_t *file_obj = (resp && resp->type == JSON_OBJECT) ? json_obj_get(resp, "file") : NULL;
    if (!fa12_truthy(file_obj)) return json_string("");
    if (fa12_bool(json_obj_get(file_obj, "has_getvalue"), false)) {
        json_t *r = fa12_call(ctx, "getvalue", fa12_args());
        if (fa12_is_raise(r)) return r;
        const char *s = fa12_str_or(r, "");
        json_t *out = json_string(s); json_free(r); return out;
    }
    json_t *r = fa12_call(ctx, "read", fa12_args());
    if (fa12_is_raise(r)) return r;
    const char *s = fa12_str_or(r, "");
    json_t *out = json_string(s); json_free(r); return out;
}

/* PoP: fa12__read_limited_feishu_webhook_body @ plugins/platforms/feishu/adapter.py:_read_limited_feishu_webhook_body */
static json_t *fa12__read_limited_feishu_webhook_body(fa12_ctx_t *ctx, const json_t *args) {
    int max_bytes = fa12_to_int(json_obj_get(args, "max_bytes"), 0);
    json_t *a = fa12_args(); json_set(a, "n", json_number((double)max_bytes + 1));
    json_t *r = fa12_call(ctx, "readexactly", a);
    if (fa12_is_raise(r)) return r;
    const char *body = "";
    if (r && r->type == JSON_OBJECT && json_obj_get(r, "__partial__"))
        body = fa12_str_or(json_obj_get(r, "__partial__"), "");
    else
        body = fa12_str_or(r, "");
    size_t len = strlen(body);
    json_free(r);
    if ((int)len > max_bytes) return fa12_err("ValueError", "payload too large");
    return json_string(body);
}

/* PoP: fa12__should_batch_media_event @ plugins/platforms/feishu/adapter.py:FeishuAdapter._should_batch_media_event */
static json_t *fa12__should_batch_media_event(fa12_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *ev = json_obj_get(args, "event");
    const json_t *urls = ev ? json_obj_get(ev, "media_urls") : NULL;
    if (ev && urls == NULL)
        return fa12_err("AttributeError", "'types.SimpleNamespace' object has no attribute 'media_urls'");
    const char *mt = fa12_str_or(ev ? json_obj_get(ev, "message_type") : NULL, "");
    bool batchable = (strcmp(mt, "photo") == 0 || strcmp(mt, "video") == 0
                      || strcmp(mt, "document") == 0 || strcmp(mt, "audio") == 0);
    return json_bool(fa12_truthy(urls) && batchable);
}

/* PoP: fa12__resolve_media_message_type @ plugins/platforms/feishu/adapter.py:FeishuAdapter._resolve_media_message_type */
static json_t *fa12__resolve_media_message_type(fa12_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char norm[512];
    fa12_lower(fa12_str_or(json_obj_get(args, "media_type"), ""), norm, sizeof norm);
    const char *def = fa12_str_or(json_obj_get(args, "default"), "text");
    if (fa12_starts(norm, "image/")) return json_string("photo");
    if (fa12_starts(norm, "audio/")) return json_string("audio");
    if (fa12_starts(norm, "video/")) return json_string("video");
    return json_string(def);
}

/* PoP: fa12__queue_empty @ plugins/platforms/feishu/adapter.py:FeishuAdapter._queue_empty */
static json_t *fa12__queue_empty(fa12_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *events = json_obj_get(args, "events");
    return json_bool(!fa12_truthy(events));
}

/* PoP: fa12__render_qr @ plugins/platforms/feishu/adapter.py:_render_qr */
static json_t *fa12__render_qr(fa12_ctx_t *ctx, const json_t *args) {
    if (!fa12_bool(json_obj_get(args, "available"), false)) return json_bool(false);
    const char *url = fa12_str_or(json_obj_get(args, "url"), "");
    json_t *r = fa12_call(ctx, "qr_construct", fa12_args());
    if (fa12_is_raise(r)) {
        if (fa12_raise_is_base(r)) return r;
        json_free(r); return json_bool(false);
    }
    json_free(r);
    json_t *a = fa12_args(); fa12_set_s(a, "url", url);
    r = fa12_call(ctx, "qr_add_data", a);
    if (fa12_is_raise(r)) {
        if (fa12_raise_is_base(r)) return r;
        json_free(r); return json_bool(false);
    }
    json_free(r);
    json_t *ma = fa12_args(); json_set(ma, "fit", json_bool(true));
    r = fa12_call(ctx, "qr_make", ma);
    if (fa12_is_raise(r)) {
        if (fa12_raise_is_base(r)) return r;
        json_free(r); return json_bool(false);
    }
    json_free(r);
    json_t *pa = fa12_args(); json_set(pa, "invert", json_bool(true));
    r = fa12_call(ctx, "qr_print_ascii", pa);
    if (fa12_is_raise(r)) {
        if (fa12_raise_is_base(r)) return r;
        json_free(r); return json_bool(false);
    }
    json_free(r);
    return json_bool(true);
}
