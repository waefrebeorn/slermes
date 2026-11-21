/*
 * feishu_adapter_flow11.c — Feishu/Lark adapter "remainders" FLOW cores of
 * plugins/platforms/feishu/adapter.py (hermes-agent).
 *
 * ELEVENTH remainder port beyond feishu_adapter_flow.c .. flow10. Ports 14
 * genuinely unclaimed defs (dotted-name-aware claim parser confirmed none are
 * claimed by flows 1-10):
 *   _hydrate_bot_identity, _install_lark_ws_isolation, _load_lark_oapi,
 *   _load_seen_message_ids, _load_settings, _maybe_extract_text_document,
 *   _on_drive_comment_event, _on_meeting_invited_event, _on_message_event,
 *   _persist_seen_message_ids (10 explicit) + the unclaimed nested closures
 *   _id_set, _secret, _extra_or_secret, _dispatch_connect (4).
 *
 * Universal seam: every seamed call enters through ONE callback
 * fa11_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting the
 * identical {"name","args","ret"} event from the SAME scripts. args is ADOPTED
 * into the trace event; the return is a NEW owned json_t (NULL == Python None;
 * {"__raise__":cls,"message":...} == a raised exception). A missing/exhausted
 * script is a fail-closed RuntimeError on both sides.
 *
 * FULL BASEEXCEPTION PASSTHROUGH: fa11_is_base_exc + fa11_raise_is_base name the
 * family that bypasses `except Exception`; those raises propagate verbatim.
 * `except (OSError, UnicodeDecodeError)` is modelled by fa11_is_osfamily.
 *
 * Representation contract (shared with tests/oracle_feishu_adapter_flow11.py):
 * - NULL json_t* == Python None; JSON_NULL node is a real null value.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__":cls,"message":str(exc)}; a scripted raise is
 *   {"__script__":true,"raise":{"type":cls,"message":msg}}.
 * - Carrier fields are read directly from args doubles (oracle materializes
 *   equivalent attribute doubles). Fixture domain: ASCII strings, scalars,
 *   JSON dict/list values, POSIX paths, integral-int numbers. State is {}.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and fa11_-prefixed.
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
typedef struct fa11_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} fa11_ctx_t;

/* ── basic helpers ── */
static bool fa11_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool fa11_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool fa11_is_base_exc(const char *kind) {
    return kind && (
        strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "asyncio.CancelledError") == 0);
}
static bool fa11_raise_is_base(const json_t *r) {
    return fa11_is_raise(r) && fa11_is_base_exc(json_get_str(r, "__raise__", ""));
}
/* OSError / UnicodeDecodeError family (swallowed by `except (OSError, UnicodeDecodeError)`). */
static bool fa11_is_osfamily(const char *kind) {
    return kind && (
        strcmp(kind, "OSError") == 0 || strcmp(kind, "FileNotFoundError") == 0
        || strcmp(kind, "IsADirectoryError") == 0 || strcmp(kind, "PermissionError") == 0
        || strcmp(kind, "NotADirectoryError") == 0 || strcmp(kind, "TimeoutError") == 0
        || strcmp(kind, "ConnectionError") == 0 || strcmp(kind, "UnicodeDecodeError") == 0
        || strcmp(kind, "UnicodeError") == 0 || strcmp(kind, "JSONDecodeError") == 0);
}
static double fa11_as_num(const json_t *v, double d) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : d;
}
static bool fa11_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *fa11_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static double fa11_num(const json_t *v, double d) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : d;
}
static int fa11_to_int(const json_t *v, int d) {
    if (!v) return d;
    if (v->type == JSON_NUMBER) return (int)(long long)v->num_val;
    if (v->type == JSON_STRING) return (int)strtol(v->str_val, NULL, 10);
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return d;
}
static bool fa11_bool(const json_t *v, bool d) {
    return (v && v->type == JSON_BOOL) ? v->bool_val : d;
}
static const char *fa11_str_or(const json_t *v, const char *d) {
    const char *s = fa11_str(v);
    return s ? s : d;
}
static json_t *fa11_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *fa11_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return fa11_err(type, msg);
}
/* The one universal seam. args is ADOPTED; the return is a fresh owned node. */
static json_t *fa11_call(fa11_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = fa11_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = fa11_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = fa11_make_exc(raise);
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
static json_t *fa11_args(void) { return json_object(); }
static void fa11_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void fa11_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
/* Python str.strip() for the ASCII fixture domain, into a caller buffer. */
static const char *fa11_snorm(const char *s, char *out, size_t cap, bool lower) {
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
/* Python str(x) for the scalar fixture domain. */
static const char *fa11_pystr(const json_t *v, char *buf, size_t cap) {
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
static bool fa11_json_equal(const json_t *a, const json_t *b) {
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
                if (!fa11_json_equal(json_get(a, i), json_get(b, i))) return false;
            return true;
        case JSON_OBJECT:
            if (json_object_size(a) != json_object_size(b)) return false;
            for (size_t i = 0; i < json_object_size(a); i++) {
                const char *k = json_object_get_key_at(a, i);
                if (!fa11_json_equal(json_object_get_at(a, i), json_obj_get(b, k))) return false;
            }
            return true;
    }
    return false;
}
/* Python {str(u).strip() for u in values if str(u).strip()} -> sorted JSON array. */
static int fa11_cmp_str(const void *a, const void *b) {
    const char *x = *(const char *const *)a, *y = *(const char *const *)b;
    return strcmp(x, y);
}
static json_t *fa11_id_set(const json_t *arr) {
    json_t *out = json_array();
    if (!arr || arr->type != JSON_ARRAY) return out;
    char store[256][256];
    const char *items[256];
    size_t n = 0;
    for (size_t i = 0; i < json_len(arr) && n < 256; i++) {
        char b[512]; const char *raw = fa11_pystr(json_get(arr, i), b, sizeof b);
        char t[256]; const char *s = fa11_snorm(raw, t, sizeof t, false);
        if (!s[0]) continue;
        bool dup = false;
        for (size_t j = 0; j < n; j++)
            if (strcmp(items[j], s) == 0) { dup = true; break; }
        if (!dup) { snprintf(store[n], sizeof store[n], "%s", s); items[n] = store[n]; n++; }
    }
    qsort(items, n, sizeof items[0], fa11_cmp_str);
    for (size_t i = 0; i < n; i++) json_append(out, json_string(items[i]));
    return out;
}
/* Python PurePosixPath(p).suffix: from the last dot of the last component. */
static const char *fa11_suffix(const char *p, char *out, size_t cap) {
    const char *slash = strrchr(p ? p : "", '/');
    const char *name = slash ? slash + 1 : (p ? p : "");
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name) { out[0] = '\0'; return out; }
    size_t n = 0;
    while (*dot && n + 1 < cap) out[n++] = *dot++;
    out[n] = '\0';
    return out;
}

/* ══ PoP functions ══ */

/* PoP: fa11__hydrate_bot_identity @ plugins/platforms/feishu/adapter.py:FeishuAdapter._hydrate_bot_identity */
static json_t *fa11__hydrate_bot_identity(fa11_ctx_t *ctx, const json_t *args) {
    if (!fa11_truthy(json_obj_get(args, "client"))) return json_null();
    char bid[4096], bname[4096];
    fa11_snorm(fa11_str_or(json_obj_get(args, "bot_open_id"), ""), bid, sizeof bid, false);
    fa11_snorm(fa11_str_or(json_obj_get(args, "bot_name"), ""), bname, sizeof bname, false);
    const char *appid = fa11_str_or(json_obj_get(args, "app_id"), "");

    /* ── block 1: /bot/v3/info ── */
    json_t *qa = fa11_args(); fa11_set_s(qa, "path", "/open-apis/bot/v3/info");
    json_t *content = fa11_call(ctx, "tenant_get_bot_info", qa);
    if (fa11_is_raise(content)) {
        if (fa11_raise_is_base(content)) return content;
        json_free(content);
    } else {
        const char *cs = fa11_str(content);
        json_t *payload = cs ? json_parse(cs, NULL) : NULL;
        json_free(content);
        if (payload && payload->type == JSON_OBJECT) {
            json_t *pa = fa11_args(); json_set(pa, "data", json_copy(payload));
            json_t *parsed = fa11_call(ctx, "parse_bot_response", pa);
            json_free(payload);
            if (fa11_is_raise(parsed)) {
                if (fa11_raise_is_base(parsed)) return parsed;
                json_free(parsed);
            } else {
                char ob[4096], nb[4096];
                const char *o = fa11_snorm(fa11_str_or(json_obj_get(parsed, "bot_open_id"), ""), ob, sizeof ob, false);
                if (o[0]) snprintf(bid, sizeof bid, "%s", o);
                const char *nm = fa11_snorm(fa11_str_or(json_obj_get(parsed, "bot_name"), ""), nb, sizeof nb, false);
                if (nm[0]) snprintf(bname, sizeof bname, "%s", nm);
                json_free(parsed);
            }
        } else if (payload) json_free(payload);
    }
    if (bname[0]) return json_null();

    /* ── block 2: application-info name-only fallback ── */
    json_t *ra = fa11_args();
    fa11_set_s(ra, "app_id", appid); fa11_set_s(ra, "lang", "en_us");
    json_t *req = fa11_call(ctx, "build_app_request", ra);
    if (fa11_is_raise(req)) {
        if (fa11_raise_is_base(req)) return req;
        json_free(req); return json_null();
    }
    json_free(req);
    json_t *rb = fa11_args();
    json_t *resp = fa11_call(ctx, "run_blocking_app", rb);
    if (fa11_is_raise(resp)) {
        if (fa11_raise_is_base(resp)) return resp;
        json_free(resp); return json_null();
    }
    if (fa11_truthy(resp)) {
        json_t *sa = fa11_args();
        json_t *ok = fa11_call(ctx, "response_success", sa);
        if (fa11_is_raise(ok)) {
            if (fa11_raise_is_base(ok)) { json_free(resp); return ok; }
            json_free(ok); json_free(resp); return json_null();
        }
        bool succeeded = fa11_truthy(ok);
        json_free(ok);
        if (succeeded) {
            const json_t *data = json_obj_get(resp, "data");
            const json_t *app = (data && data->type == JSON_OBJECT) ? json_obj_get(data, "app") : NULL;
            const char *an = (app && app->type == JSON_OBJECT) ? fa11_str_or(json_obj_get(app, "app_name"), "") : "";
            char ab[4096]; const char *ast = fa11_snorm(an, ab, sizeof ab, false);
            if (ast[0] && !bname[0]) snprintf(bname, sizeof bname, "%s", ast);
        }
    }
    json_free(resp);
    return json_null();
}

/* PoP: fa11__install_lark_ws_isolation @ plugins/platforms/feishu/adapter.py:FeishuAdapter._install_lark_ws_isolation */
static json_t *fa11__install_lark_ws_isolation(fa11_ctx_t *ctx, const json_t *args) {
    if (fa11_bool(json_obj_get(args, "installed"), false)) return json_null();
    json_t *wa = fa11_args(); fa11_set_copy(wa, "loop", json_obj_get(args, "loop"));
    json_t *r = fa11_call(ctx, "wrap_loop", wa);
    if (fa11_is_raise(r)) return r;
    json_free(r);
    json_t *ca = fa11_args();
    fa11_set_s(ca, "connect_name", fa11_str_or(json_obj_get(args, "connect_name"), "connect"));
    r = fa11_call(ctx, "install_connect", ca);
    if (fa11_is_raise(r)) return r;
    json_free(r);
    r = fa11_call(ctx, "install_receive", fa11_args());
    if (fa11_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: fa11__load_lark_oapi @ plugins/platforms/feishu/adapter.py:_load_lark_oapi */
static json_t *fa11__load_lark_oapi(fa11_ctx_t *ctx, const json_t *args) {
    if (fa11_bool(json_obj_get(args, "available"), false)) return json_bool(true);
    static const char *MODS[] = {
        "lark_oapi",
        "lark_oapi.api.application.v6",
        "lark_oapi.api.im.v1",
        "lark_oapi.core",
        "lark_oapi.core.const",
        "lark_oapi.core.model",
        "lark_oapi.event.callback.model.p2_card_action_trigger",
        "lark_oapi.event.dispatcher_handler",
        "lark_oapi.ws"
    };
    for (size_t i = 0; i < sizeof(MODS) / sizeof(MODS[0]); i++) {
        json_t *a = fa11_args(); fa11_set_s(a, "name", MODS[i]);
        json_t *mod = fa11_call(ctx, "import_module", a);
        if (fa11_is_raise(mod)) {
            const char *k = json_get_str(mod, "__raise__", "");
            bool caught = (!fa11_raise_is_base(mod)) &&
                (strcmp(k, "ImportError") == 0 || strcmp(k, "ModuleNotFoundError") == 0 ||
                 strcmp(k, "AttributeError") == 0);
            if (caught) { json_free(mod); return json_bool(false); }
            return mod; /* BaseException or other Exception propagates verbatim */
        }
        json_free(mod);
    }
    return json_bool(true);
}

/* PoP: fa11__load_seen_message_ids @ plugins/platforms/feishu/adapter.py:FeishuAdapter._load_seen_message_ids */
static json_t *fa11__load_seen_message_ids(fa11_ctx_t *ctx, const json_t *args) {
    json_t *a = fa11_args(); fa11_set_copy(a, "path", json_obj_get(args, "path"));
    json_t *raw = fa11_call(ctx, "read_text", a);
    if (fa11_is_raise(raw)) {
        if (fa11_raise_is_base(raw)) return raw;
        const char *k = json_get_str(raw, "__raise__", "");
        if (fa11_is_osfamily(k)) { json_free(raw); return json_null(); }
        const char *m = json_get_str(raw, "message", "");
        json_t *e = fa11_err(k, m);
        json_free(raw);
        return e; /* non-OSError exception propagates (live only catches OSError family) */
    }
    json_free(raw);
    return json_null();
}

/* PoP: fa11__load_settings @ plugins/platforms/feishu/adapter.py:FeishuAdapter._load_settings */
static json_t *fa11__load_settings(fa11_ctx_t *ctx, const json_t *args) {
    const json_t *extra = (args && args->type == JSON_OBJECT) ? args : NULL;
    /* local helper seams mirroring the nested closures _secret/_extra_or_secret */
    char tmp[8192];

    /* group_rules */
    json_t *group_rules = json_object();
    const json_t *gr = extra ? json_obj_get(extra, "group_rules") : NULL;
    if (gr && gr->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(gr); i++) {
            const char *cid = json_object_get_key_at(gr, i);
            const json_t *rc = json_object_get_at(gr, i);
            if (!rc || rc->type != JSON_OBJECT) continue;
            char pol[4096]; fa11_snorm(fa11_str_or(json_obj_get(rc, "policy"), "open"), pol, sizeof pol, true);
            json_t *al = fa11_id_set(json_obj_get(rc, "allowlist"));
            json_t *bl = fa11_id_set(json_obj_get(rc, "blacklist"));
            json_t *rm = fa11_bool(json_obj_get(rc, "require_mention"), false) ? json_bool(true) : json_null();
            const json_t *rv = json_obj_get(rc, "require_mention");
            if (rv && rv->type != JSON_NULL) {
                json_t *ba = fa11_args(); json_set(ba, "value", json_copy(rv));
                json_t *br = fa11_call(ctx, "to_boolean", ba);
                if (fa11_is_raise(br)) { if (fa11_raise_is_base(br)) return br; json_free(br); rm = json_null(); }
                else { rm = json_copy(br); json_free(br); }
            }
            json_t *ga = fa11_args();
            fa11_set_s(ga, "policy", pol);
            fa11_set_copy(ga, "allowlist", al); fa11_set_copy(ga, "blacklist", bl);
            json_set(ga, "require_mention", rm);
            json_t *rule = fa11_call(ctx, "group_rule", ga);
            if (fa11_is_raise(rule)) return rule;
            json_set(group_rules, cid, rule);
            json_free(al); json_free(bl);
        }
    }

    /* non-raising seam helpers: scoped_secret(name,default), extra_or_secret(key,env,default) */
    /* Each returns an owned json_t value (string result), propagated or plain. */
#define SC(NAME, DEF) do { \
        json_t *_a = fa11_args(); fa11_set_s(_a, "name", NAME); fa11_set_s(_a, "default", DEF); \
        json_t *_r = fa11_call(ctx, "scoped_secret", _a); \
        if (fa11_is_raise(_r)) return _r; \
        snprintf(tmp, sizeof tmp, "%s", fa11_str_or(_r, "")); json_free(_r); \
} while (0)
#define EOS(KEY, ENV, DEF) do { \
        json_t *_a = fa11_args(); fa11_set_s(_a, "key", KEY); fa11_set_s(_a, "env", ENV); fa11_set_s(_a, "default", DEF); \
        json_t *_r = fa11_call(ctx, "extra_or_secret", _a); \
        if (fa11_is_raise(_r)) return _r; \
        snprintf(tmp, sizeof tmp, "%s", fa11_str_or(_r, "")); json_free(_r); \
} while (0)

    /* allow_bots */
    EOS("allow_bots", "FEISHU_ALLOW_BOTS", "none");
    char allow_bots[4096]; fa11_snorm(tmp, allow_bots, sizeof allow_bots, true);
    if (strcmp(allow_bots, "none") != 0 && strcmp(allow_bots, "mentions") != 0 && strcmp(allow_bots, "all") != 0)
        snprintf(allow_bots, sizeof allow_bots, "none");

    /* allow_all_dm: any(...) short-circuits */
    bool allow_all_dm = false;
    SC("FEISHU_ALLOW_ALL_USERS", "");
    char v1[4096]; fa11_snorm(tmp, v1, sizeof v1, true);
    allow_all_dm = (strcmp(v1,"true")==0 || strcmp(v1,"1")==0 || strcmp(v1,"yes")==0);
    if (!allow_all_dm) {
        SC("GATEWAY_ALLOW_ALL_USERS", "");
        char v2[4096]; fa11_snorm(tmp, v2, sizeof v2, true);
        allow_all_dm = (strcmp(v2,"true")==0 || strcmp(v2,"1")==0 || strcmp(v2,"yes")==0);
    }

    /* Build the 30-field settings dict */
    json_t *s = json_object();
    char norm[4096];

    EOS("app_id", "FEISHU_APP_ID", ""); fa11_set_s(s, "app_id", tmp);
    EOS("app_secret", "FEISHU_APP_SECRET", ""); fa11_set_s(s, "app_secret", tmp);
    EOS("domain", "FEISHU_DOMAIN", "feishu"); fa11_snorm(tmp, norm, sizeof norm, true); fa11_set_s(s, "domain_name", norm);
    EOS("connection_mode", "FEISHU_CONNECTION_MODE", "websocket"); fa11_snorm(tmp, norm, sizeof norm, true); fa11_set_s(s, "connection_mode", norm);
    EOS("encrypt_key", "FEISHU_ENCRYPT_KEY", ""); fa11_set_s(s, "encrypt_key", tmp);
    EOS("verification_token", "FEISHU_VERIFICATION_TOKEN", ""); fa11_set_s(s, "verification_token", tmp);
    SC("FEISHU_GROUP_POLICY", "allowlist"); fa11_snorm(tmp, norm, sizeof norm, true); fa11_set_s(s, "group_policy", norm);
    SC("FEISHU_ALLOWED_USERS", "");
    { json_t *parts = json_array(); const char *p = tmp;
      while (1) { const char *comma = strchr(p, ','); size_t len = comma ? (size_t)(comma - p) : strlen(p);
        char seg[512]; size_t nn = len < 511 ? len : 511; memcpy(seg, p, nn); seg[nn] = '\0';
        json_append(parts, json_string(seg)); if (!comma) break; p = comma + 1; }
      json_t *ids = fa11_id_set(parts); json_free(parts); json_set(s, "allowed_group_users", ids); }
    SC("FEISHU_BOT_OPEN_ID", ""); fa11_set_s(s, "bot_open_id", tmp);
    SC("FEISHU_BOT_USER_ID", ""); fa11_set_s(s, "bot_user_id", tmp);
    SC("FEISHU_BOT_NAME", ""); fa11_set_s(s, "bot_name", tmp);

    double x;
    { json_t *a = fa11_args(); fa11_set_s(a, "name", "HERMES_FEISHU_DEDUP_CACHE_SIZE"); json_set(a, "default", json_number(2048));
      json_t *r = fa11_call(ctx, "env_int", a); if (fa11_is_raise(r)) return r; x = fa11_num(r, 2048); json_free(r);
      if (x < 32) x = 32; json_set(s, "dedup_cache_size", json_number(x)); }
    { json_t *a = fa11_args(); fa11_set_s(a, "name", "HERMES_FEISHU_TEXT_BATCH_DELAY_SECONDS"); json_set(a, "default", json_number(0.6));
      json_t *r = fa11_call(ctx, "env_float", a); if (fa11_is_raise(r)) return r; x = fa11_num(r, 0.6); json_free(r); json_set(s, "text_batch_delay_seconds", json_number(x)); }
    { json_t *a = fa11_args(); fa11_set_s(a, "name", "HERMES_FEISHU_TEXT_BATCH_SPLIT_DELAY_SECONDS"); json_set(a, "default", json_number(2.0));
      json_t *r = fa11_call(ctx, "env_float", a); if (fa11_is_raise(r)) return r; x = fa11_num(r, 2.0); json_free(r); json_set(s, "text_batch_split_delay_seconds", json_number(x)); }
    { json_t *a = fa11_args(); fa11_set_s(a, "name", "HERMES_FEISHU_TEXT_BATCH_MAX_MESSAGES"); json_set(a, "default", json_number(8));
      json_t *r = fa11_call(ctx, "env_int", a); if (fa11_is_raise(r)) return r; x = fa11_num(r, 8); json_free(r); if (x < 1) x = 1; json_set(s, "text_batch_max_messages", json_number(x)); }
    { json_t *a = fa11_args(); fa11_set_s(a, "name", "HERMES_FEISHU_TEXT_BATCH_MAX_CHARS"); json_set(a, "default", json_number(4000));
      json_t *r = fa11_call(ctx, "env_int", a); if (fa11_is_raise(r)) return r; x = fa11_num(r, 4000); json_free(r); if (x < 1) x = 1; json_set(s, "text_batch_max_chars", json_number(x)); }
    { json_t *a = fa11_args(); fa11_set_s(a, "name", "HERMES_FEISHU_MEDIA_BATCH_DELAY_SECONDS"); json_set(a, "default", json_number(0.8));
      json_t *r = fa11_call(ctx, "env_float", a); if (fa11_is_raise(r)) return r; x = fa11_num(r, 0.8); json_free(r); json_set(s, "media_batch_delay_seconds", json_number(x)); }

    EOS("webhook_host", "FEISHU_WEBHOOK_HOST", "127.0.0.1"); fa11_set_s(s, "webhook_host", tmp);
    /* webhook_port: extra value or scoped secret; int() */
    { const json_t *wv = extra ? json_obj_get(extra, "webhook_port") : NULL;
      int port;
      if (fa11_truthy(wv)) port = fa11_to_int(wv, 8765);
      else { SC("FEISHU_WEBHOOK_PORT", "8765"); port = (int)strtol(tmp, NULL, 10); }
      json_set(s, "webhook_port", json_number(port)); }
    EOS("webhook_path", "FEISHU_WEBHOOK_PATH", "/feishu/webhook");
    if (!tmp[0]) snprintf(tmp, sizeof tmp, "/feishu/webhook");
    fa11_set_s(s, "webhook_path", tmp);

    /* int coercion seams */
    { json_t *a = fa11_args(); fa11_set_copy(a, "value", extra ? json_obj_get(extra, "ws_reconnect_nonce") : NULL); json_set(a, "default", json_number(30)); json_set(a, "min_value", json_number(0));
      json_t *r = fa11_call(ctx, "coerce_required_int", a); if (fa11_is_raise(r)) return r; json_set(s, "ws_reconnect_nonce", json_copy(r)); json_free(r); }
    { json_t *a = fa11_args(); fa11_set_copy(a, "value", extra ? json_obj_get(extra, "ws_reconnect_interval") : NULL); json_set(a, "default", json_number(120)); json_set(a, "min_value", json_number(1));
      json_t *r = fa11_call(ctx, "coerce_required_int", a); if (fa11_is_raise(r)) return r; json_set(s, "ws_reconnect_interval", json_copy(r)); json_free(r); }
    { json_t *a = fa11_args(); fa11_set_copy(a, "value", extra ? json_obj_get(extra, "ws_ping_interval") : NULL); json_set(a, "default", json_null()); json_set(a, "min_value", json_number(1));
      json_t *r = fa11_call(ctx, "coerce_int", a); if (fa11_is_raise(r)) return r; json_set(s, "ws_ping_interval", json_copy(r)); json_free(r); }
    { json_t *a = fa11_args(); fa11_set_copy(a, "value", extra ? json_obj_get(extra, "ws_ping_timeout") : NULL); json_set(a, "default", json_null()); json_set(a, "min_value", json_number(1));
      json_t *r = fa11_call(ctx, "coerce_int", a); if (fa11_is_raise(r)) return r; json_set(s, "ws_ping_timeout", json_copy(r)); json_free(r); }

    json_t *admins = fa11_id_set(extra ? json_obj_get(extra, "admins") : NULL);
    json_set(s, "admins", admins);
    fa11_snorm(fa11_str_or(extra ? json_obj_get(extra, "default_group_policy") : NULL, ""), norm, sizeof norm, true);
    fa11_set_s(s, "default_group_policy", norm);
    json_set(s, "group_rules", group_rules);
    fa11_set_s(s, "allow_bots", allow_bots);

    /* require_mention: default expression _get_scoped_secret is ALWAYS evaluated */
    SC("FEISHU_REQUIRE_MENTION", "true");
    char rmraw[4096]; snprintf(rmraw, sizeof rmraw, "%s", tmp);
    const json_t *rv = extra ? json_obj_get(extra, "require_mention") : NULL;
    json_t *rbv = rv ? json_copy(rv) : json_string(rmraw);
    { json_t *a = fa11_args(); json_set(a, "value", rbv);
      json_t *r = fa11_call(ctx, "to_boolean", a);
      if (fa11_is_raise(r)) return r; json_set(s, "require_mention", json_copy(r)); json_free(r); }
    json_set(s, "allow_all_dm", json_bool(allow_all_dm));

    /* FeishuAdapterSettings(**fields) */
    json_t *sa = fa11_args();
    const char *KEYS[] = {
        "app_id","app_secret","domain_name","connection_mode","encrypt_key","verification_token",
        "group_policy","allowed_group_users","bot_open_id","bot_user_id","bot_name","dedup_cache_size",
        "text_batch_delay_seconds","text_batch_split_delay_seconds","text_batch_max_messages","text_batch_max_chars",
        "media_batch_delay_seconds","webhook_host","webhook_port","webhook_path","ws_reconnect_nonce",
        "ws_reconnect_interval","ws_ping_interval","ws_ping_timeout","admins","default_group_policy",
        "group_rules","allow_bots","require_mention","allow_all_dm"
    };
    for (size_t i = 0; i < sizeof(KEYS)/sizeof(KEYS[0]); i++)
        json_set(sa, KEYS[i], json_copy(json_obj_get(s, KEYS[i])));
    json_free(s);
    return fa11_call(ctx, "settings", sa);
#undef SC
#undef EOS
}

/* PoP: fa11__maybe_extract_text_document @ plugins/platforms/feishu/adapter.py:FeishuAdapter._maybe_extract_text_document */
static json_t *fa11__maybe_extract_text_document(fa11_ctx_t *ctx, const json_t *args) {
    const char *path = fa11_str_or(json_obj_get(args, "cached_path"), "");
    const char *mt = fa11_str_or(json_obj_get(args, "media_type"), "");
    if (!path[0] || strncmp(mt, "text/", 5) != 0) return json_string("");

    json_t *sa = fa11_args(); fa11_set_s(sa, "path", path);
    json_t *sz = fa11_call(ctx, "getsize", sa);
    if (fa11_is_raise(sz)) {
        if (!fa11_raise_is_base(sz) && fa11_is_osfamily(json_get_str(sz, "__raise__", ""))) { json_free(sz); return json_string(""); }
        return sz;
    }
    double size = fa11_as_num(sz, 0); json_free(sz);
    if (size > 102400.0) return json_string("");

    char suf[256]; fa11_snorm(fa11_suffix(path, suf, sizeof suf), suf, sizeof suf, true);
    bool ext_ok = (strcmp(suf, ".txt") == 0 || strcmp(suf, ".md") == 0);
    bool mt_ok = (strcmp(mt, "text/plain") == 0 || strcmp(mt, "text/markdown") == 0);
    if (!ext_ok && !mt_ok) return json_string("");

    json_t *ra = fa11_args(); fa11_set_s(ra, "path", path);
    json_t *content = fa11_call(ctx, "read_text", ra);
    if (fa11_is_raise(content)) {
        if (!fa11_raise_is_base(content) && fa11_is_osfamily(json_get_str(content, "__raise__", ""))) { json_free(content); return json_string(""); }
        return content;
    }
    const char *cs = fa11_str_or(content, "");

    json_t *da = fa11_args(); fa11_set_s(da, "cached_path", path);
    json_t *disp = fa11_call(ctx, "display_name", da);
    if (fa11_is_raise(disp)) {
        if (!fa11_raise_is_base(disp) && fa11_is_osfamily(json_get_str(disp, "__raise__", ""))) { json_free(disp); json_free(content); return json_string(""); }
        json_free(content); return disp;
    }
    const char *dn = fa11_str_or(disp, "");
    char out[16384];
    snprintf(out, sizeof out, "[Content of %s]:\n%s", dn, cs);
    json_free(disp); json_free(content);
    return json_string(out);
}

/* PoP: fa11__on_drive_comment_event @ plugins/platforms/feishu/adapter.py:FeishuAdapter._on_drive_comment_event */
static json_t *fa11__on_drive_comment_event(fa11_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = fa11_args(); fa11_set_s(a, "label", "drive comment event");
    json_t *r = fa11_call(ctx, "submit_if_ready", a);
    if (fa11_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: fa11__on_meeting_invited_event @ plugins/platforms/feishu/adapter.py:FeishuAdapter._on_meeting_invited_event */
static json_t *fa11__on_meeting_invited_event(fa11_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = fa11_args(); fa11_set_s(a, "label", "meeting invite event");
    json_t *r = fa11_call(ctx, "submit_if_ready", a);
    if (fa11_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: fa11__on_message_event @ plugins/platforms/feishu/adapter.py:FeishuAdapter._on_message_event */
static json_t *fa11__on_message_event(fa11_ctx_t *ctx, const json_t *args) {
    const json_t *loop = json_obj_get(args, "loop");
    const json_t *data = json_obj_get(args, "data");
    json_t *la = fa11_args(); fa11_set_copy(la, "loop", loop);
    json_t *ok = fa11_call(ctx, "loop_accepts", la);
    if (fa11_is_raise(ok)) return ok;
    bool accepts = fa11_truthy(ok); json_free(ok);
    if (!accepts) {
        json_t *ea = fa11_args(); fa11_set_copy(ea, "data", data);
        json_t *enq = fa11_call(ctx, "enqueue_pending", ea);
        if (fa11_is_raise(enq)) return enq;
        bool should = fa11_truthy(enq); json_free(enq);
        if (should) {
            json_t *ta = fa11_args(); fa11_set_s(ta, "name", "feishu-pending-inbound-drainer");
            json_t *r = fa11_call(ctx, "thread_start", ta);
            if (fa11_is_raise(r)) return r;
            json_free(r);
        }
        return json_null();
    }
    json_t *ha = fa11_args(); fa11_set_copy(ha, "data", data);
    json_t *hd = fa11_call(ctx, "handle_event_data", ha);
    if (fa11_is_raise(hd)) return hd;
    json_free(hd);
    json_t *sa = fa11_args(); fa11_set_copy(sa, "loop", loop);
    json_t *r = fa11_call(ctx, "submit_on_loop", sa);
    if (fa11_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: fa11__persist_seen_message_ids @ plugins/platforms/feishu/adapter.py:FeishuAdapter._persist_seen_message_ids */
static json_t *fa11__persist_seen_message_ids(fa11_ctx_t *ctx, const json_t *args) {
    const char *path = fa11_str_or(json_obj_get(args, "path"), "");
    /* derive parent dir (last '/') */
    char parent[4096];
    const char *slash = strrchr(path, '/');
    if (slash) { size_t n = (size_t)(slash - path); if (n == 0) n = 1; memcpy(parent, path, n); parent[n] = '\0'; }
    else snprintf(parent, sizeof parent, ".");
    json_t *ma = fa11_args(); fa11_set_s(ma, "parent", parent);
    json_t *r = fa11_call(ctx, "mkdir", ma);
    if (fa11_is_raise(r)) {
        if (!fa11_raise_is_base(r) && fa11_is_osfamily(json_get_str(r, "__raise__", ""))) { json_free(r); return json_null(); }
        return r;
    }
    json_free(r);
    /* build recent = last cache_size of order; payload = {k: ids[k] for k in recent if k in ids} */
    const json_t *order = json_obj_get(args, "seen_order");
    const json_t *ids = json_obj_get(args, "seen_ids");
    int cap = fa11_to_int(json_obj_get(args, "cache_size"), 2);
    json_t *pids = json_object();
    if (order && order->type == JSON_ARRAY) {
        size_t n = json_len(order);
        size_t start = (cap < 0) ? 0 : (n > (size_t)cap ? n - (size_t)cap : 0);
        for (size_t i = start; i < n; i++) {
            const char *k = fa11_str_or(json_get(order, i), "");
            const json_t *v = ids ? json_obj_get(ids, k) : NULL;
            if (v) json_set(pids, k, json_copy(v));
        }
    }
    json_t *payload = json_object(); json_set(payload, "message_ids", pids);
    json_t *wa = fa11_args(); fa11_set_s(wa, "path", path); json_set(wa, "payload", payload); json_set(wa, "indent", json_null());
    r = fa11_call(ctx, "atomic_write", wa);
    if (fa11_is_raise(r)) {
        if (!fa11_raise_is_base(r) && fa11_is_osfamily(json_get_str(r, "__raise__", ""))) { json_free(r); return json_null(); }
        return r;
    }
    json_free(r);
    return json_null();
}

/* ── unclaimed nested closures (ported as standalone flow targets) ── */

/* PoP: fa11__id_set @ plugins/platforms/feishu/adapter.py:_id_set */
static json_t *fa11__id_set(fa11_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return fa11_id_set(json_obj_get(args, "values"));
}

/* PoP: fa11__secret @ plugins/platforms/feishu/adapter.py:_secret */
static json_t *fa11__secret(fa11_ctx_t *ctx, const json_t *args) {
    json_t *a = fa11_args();
    fa11_set_s(a, "name", fa11_str_or(json_obj_get(args, "name"), ""));
    fa11_set_s(a, "default", fa11_str_or(json_obj_get(args, "default"), ""));
    json_t *r = fa11_call(ctx, "scoped_secret", a);
    if (fa11_is_raise(r)) return r;
    char t[4096]; const char *s = fa11_snorm(fa11_str_or(r, ""), t, sizeof t, false);
    json_t *out = json_string(s); json_free(r);
    return out;
}

/* PoP: fa11__extra_or_secret @ plugins/platforms/feishu/adapter.py:_extra_or_secret */
static json_t *fa11__extra_or_secret(fa11_ctx_t *ctx, const json_t *args) {
    json_t *a = fa11_args();
    fa11_set_s(a, "key", fa11_str_or(json_obj_get(args, "key"), ""));
    fa11_set_s(a, "env", fa11_str_or(json_obj_get(args, "env"), ""));
    fa11_set_s(a, "default", fa11_str_or(json_obj_get(args, "default"), ""));
    json_t *r = fa11_call(ctx, "extra_or_secret", a);
    if (fa11_is_raise(r)) return r;
    char pb[4096]; const char *ps = fa11_pystr(r, pb, sizeof pb);
    char t[4096]; const char *s = fa11_snorm(ps, t, sizeof t, false);
    json_t *out = json_string(s); json_free(r);
    return out;
}

/* PoP: fa11__dispatch_connect @ plugins/platforms/feishu/adapter.py:_dispatch_connect */
static json_t *fa11__dispatch_connect(fa11_ctx_t *ctx, const json_t *args) {
    const json_t *pos = json_obj_get(args, "pos_args");
    const json_t *kw = json_obj_get(args, "kwargs");
    json_t *ka = fa11_args();
    json_t *ovr = fa11_call(ctx, "connect_kwargs", ka);
    if (fa11_is_raise(ovr)) return ovr;
    json_t *merged = json_object();
    if (kw && kw->type == JSON_OBJECT)
        for (size_t i = 0; i < json_object_size(kw); i++) {
            const char *k = json_object_get_key_at(kw, i);
            json_set(merged, k, json_copy(json_object_get_at(kw, i)));
        }
    if (ovr && ovr->type == JSON_OBJECT)
        for (size_t i = 0; i < json_object_size(ovr); i++) {
            const char *k = json_object_get_key_at(ovr, i);
            if (json_obj_get(merged, k) == NULL)
                json_set(merged, k, json_copy(json_object_get_at(ovr, i)));
        }
    json_free(ovr);
    json_t *ca = fa11_args();
    json_set(ca, "args", pos ? json_copy(pos) : json_array());
    json_set(ca, "kwargs", merged);
    return fa11_call(ctx, "real_connect", ca);
}
