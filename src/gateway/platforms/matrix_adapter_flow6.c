/*
 * matrix_adapter_flow6.c -- FLOW port of UNCLAIMED helpers of
 * plugins/platforms/matrix/adapter.py (hermes-agent): connect/E2EE lifecycle,
 * sync dispatch, inbound event/text build, media download/cache, markdown,
 * reply context, and the crypto-pickle migration remainder.
 *
 * The flow1..flow5 siblings and their claimed funcs are NOT re-ported. Every
 * collaborator below (ported sibling, claimed sibling, module or stdlib) enters
 * through the ONE universal seam as a bare name -- none is re-ported. All
 * symbols are static and ma6_-prefixed; libjson only.
 *
 * Decision sequences only: every live I/O / injected dependency enters through
 * ma6_call(ctx, name, args). The harness wires it to a recording stub fed the
 * case's scripted results (in.io); the oracle installs recording stubs emitting
 * the identical event shape {"name","args","ret"} from the SAME scripts.
 *
 * Representation contract (shared with tests/oracle_matrix_adapter_flow6.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; scripted raises
 *   round-trip verbatim when they escape a drive. except Exception swallows the
 *   standard Exception family; SystemExit/KeyboardInterrupt/GeneratorExit pass.
 * - Attribute carriers are {"__attr__": {attrs}}. Sets emit as sorted arrays.
 * - logger.* calls are seams (recorded) with args {"msg":...,"args":[...]}.
 *   Fail-closed: a missing/exhausted seam script is a RuntimeError that
 *   propagates unless the live body has an except Exception around it.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *EMPTY_OBJ = NULL; /* lazily created; live .get(...,{}) default */

/* -- universal seam context -- */
typedef struct ma6_ctx {
    json_t *state;      /* owned; emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} ma6_ctx_t;

/* -- exception / value helpers -- */
static bool ma6_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static json_t *ma6_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type ? type : "RuntimeError");
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *ma6_make_exc(const json_t *spec) {
    return ma6_err(json_get_str(spec, "type", "RuntimeError"),
                   json_get_str(spec, "message", ""));
}
static bool ma6_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ma6_truthy(const json_t *v) {
    if (ma6_is_null(v)) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *ma6_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static double ma6_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static json_t *ma6_copy_or_null(const json_t *v) { return v ? json_copy(v) : json_null(); }
static void ma6_set_str(json_t *obj, const char *key, const char *s) {
    json_set(obj, key, s ? json_string(s) : json_null());
}
static const char *ma6_exc_msg(const json_t *v) {
    if (!v) return "";
    if (ma6_is_raise(v)) return json_get_str(v, "message", "");
    return ma6_str(v) ? ma6_str(v) : "";
}
static const char *ma6_attr(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    const json_t *in = json_obj_get(obj, "__attr__");
    if (in && in->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(in, key);
        return ma6_str(v);
    }
    return ma6_str(json_obj_get(obj, key));
}
static const json_t *ma6_attr_node(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    const json_t *in = json_obj_get(obj, "__attr__");
    if (in && in->type == JSON_OBJECT) return json_obj_get(in, key);
    return json_obj_get(obj, key);
}
static bool ma6_arr_has_str(const json_t *arr, const char *s) {
    if (!s || !arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *v = ma6_str(json_get(arr, i));
        if (v && strcmp(v, s) == 0) return true;
    }
    return false;
}

/* except Exception catches the standard Exception family; BaseException trio
 * (SystemExit, KeyboardInterrupt, GeneratorExit) passes through. */
static bool ma6_exc_is_exception(const json_t *v) {
    if (!ma6_is_raise(v)) return false;
    const char *k = json_get_str(v, "__raise__", "");
    if (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt") ||
        !strcmp(k, "GeneratorExit") || !strcmp(k, "BaseException")) return false;
    return true;
}

/* -- growable string buffer -- */
typedef struct { char *b; size_t n, cap; } ma6_buf_t;
static void mb_reserve(ma6_buf_t *b, size_t need) {
    if (b->b && b->n + need + 1 <= b->cap) return;
    size_t nc = b->cap ? b->cap : 32;
    while (nc < b->n + need + 1) nc *= 2;
    char *nb = realloc(b->b, nc);
    if (!nb) return;
    b->b = nb; b->cap = nc;
    if (b->n == 0) b->b[0] = '\0';
}
static void mb_addn(ma6_buf_t *b, const char *s, size_t n) {
    if (!s || n == 0) return;
    mb_reserve(b, n);
    if (!b->b) return;
    memcpy(b->b + b->n, s, n);
    b->n += n; b->b[b->n] = '\0';
}
static void mb_add(ma6_buf_t *b, const char *s) { if (s) mb_addn(b, s, strlen(s)); }
static void mb_addc(ma6_buf_t *b, char c) { mb_reserve(b, 1); if (b->b) { b->b[b->n++] = c; b->b[b->n] = '\0'; } }
static char *mb_take(ma6_buf_t *b) {
    if (!b->b) { char *e = malloc(1); if (e) e[0] = '\0'; return e; }
    b->b[b->n] = '\0';
    char *r = b->b; b->b = NULL; b->n = b->cap = 0; return r;
}
static char *ma6_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s); char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char ma6_tolower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }
static char *ma6_strip(const char *s) {
    if (!s) return ma6_strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}

/* PurePosixPath join: base / "crypto.db" (drop trailing slashes, keep one). */
static char *ma6_path_join(const char *base, const char *leaf) {
    if (!base || !*base) return ma6_strdup(leaf ? leaf : "");
    size_t n = strlen(base);
    ma6_buf_t b = {0};
    mb_add(&b, base);
    if (base[n - 1] != '/') mb_addc(&b, '/');
    mb_add(&b, leaf);
    return mb_take(&b);
}

/* Set-valued state array: add each string once (dedupe), deterministic order. */
static void ma6_set_add(json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return;
    if (!ma6_arr_has_str(arr, s)) json_append(arr, json_string(s));
}

/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *ma6_call(ma6_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    char msg[256];
    if (!entry) {
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ma6_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ma6_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) ret = ma6_make_exc(raise);
            else {
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

/* logger helper: args {"msg":..., "args":[...]} ; arg array is adopted. */
static json_t *ma6_log(ma6_ctx_t *ctx, const char *name, const char *fmt, json_t *argarr) {
    json_t *a = json_object();
    ma6_set_str(a, "msg", fmt);
    json_set(a, "args", argarr ? argarr : json_array());
    return ma6_call(ctx, name, a);
}

/* ===================== ported helpers ===================== */

/* PoP: ma6_crypto_db_path @ plugins/platforms/matrix/adapter.py:_crypto_db_path */
/* _crypto_db_path(self) -> (self._store_dir or _get_hermes_dir(...)) / "crypto.db" */
static json_t *ma6_crypto_db_path(ma6_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *self = ctx->state;
    const char *sd = ma6_str(ma6_attr_node(self, "_store_dir"));
    const char *base = sd;
    char *owned = NULL;
    if (!base || !*base) {
        json_t *a = json_object();
        json_set_hdr(a, "legacy_path", "platforms/matrix/store");
        json_set_hdr(a, "new_path", "matrix/store");
        json_t *r = ma6_call(ctx, "_get_hermes_dir", a);
        if (ma6_is_raise(r)) return r;
        owned = ma6_strdup(ma6_str(r));
        json_free(r);
        base = owned;
    }
    char *p = ma6_path_join(base ? base : "", "crypto.db");
    free(owned);
    json_t *res = json_string(p);
    free(p);
    return res;
}

/* PoP: ma6_abort_connect @ plugins/platforms/matrix/adapter.py:_abort_connect */
/* _abort_connect(api, crypto_db=None) -> bool (always False) */
static json_t *ma6_abort_connect(ma6_ctx_t *ctx, const json_t *args) {
    const json_t *cdb = json_obj_get(args, "crypto_db");
    if (cdb && cdb->type != JSON_NULL && !ma6_is_null(cdb)) {
        json_t *a = json_object();
        json_t *r = ma6_call(ctx, "crypto_db.stop", a);
        if (ma6_is_raise(r)) return r;
        json_free(r);
    }
    json_t *a = json_object();
    json_t *r = ma6_call(ctx, "session.close", a);
    if (ma6_is_raise(r)) return r;
    json_free(r);
    return json_bool(false);
}

/* PoP: ma6_e2ee_setup_failed @ plugins/platforms/matrix/adapter.py:_e2ee_setup_failed */
/* _e2ee_setup_failed(self, what, exc, api) -> bool */
static json_t *ma6_e2ee_setup_failed(ma6_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const char *mode = ma6_str(ma6_attr_node(self, "_e2ee_mode"));
    const char *what = ma6_str(json_obj_get(args, "what"));
    const char *exc = ma6_exc_msg(json_obj_get(args, "exc"));
    if (mode && strcmp(mode, "optional") == 0) {
        json_t *arr = json_array();
        json_append(arr, json_string(what ? what : ""));
        json_append(arr, json_string(exc));
        json_append(arr, json_string("Install with: pip install 'mautrix[encryption]' asyncpg aiosqlite  (requires libolm C library)"));
        json_t *r = ma6_log(ctx, "logger.warning",
            "Matrix: failed to %s optional E2EE client; continuing without encrypted-room support: %s. %s", arr);
        if (ma6_is_raise(r)) return r;
        json_free(r);
        json_set(self, "_encryption", json_bool(false));
        return json_bool(true);
    }
    json_t *arr = json_array();
    json_append(arr, json_string(what ? what : ""));
    json_append(arr, json_string(exc));
    json_append(arr, json_string("Install with: pip install 'mautrix[encryption]' asyncpg aiosqlite  (requires libolm C library)"));
    json_t *r = ma6_log(ctx, "logger.error",
        "Matrix: failed to %s E2EE client: %s. %s", arr);
    if (ma6_is_raise(r)) return r;
    json_free(r);
    json_t *aa = json_object();
    json_set(aa, "api", ma6_copy_or_null(json_obj_get(args, "api")));
    return ma6_call(ctx, "_abort_connect", aa);
}

/* except Exception for _connect_initial_sync: describe_proxy_error(exc,
 * matrix_redaction_proxy(...)) or exc -> logger.warning. */
static json_t *ma6_cis_except(ma6_ctx_t *ctx, json_t *self, const json_t *exc) {
    json_t *pa = json_object();
    ma6_set_str(pa, "proxy_url", ma6_str(ma6_attr_node(self, "_proxy_url")));
    ma6_set_str(pa, "homeserver", ma6_str(ma6_attr_node(self, "_homeserver")));
    json_t *proxy = ma6_call(ctx, "matrix_redaction_proxy", pa);
    if (ma6_is_raise(proxy)) return proxy;
    json_t *da = json_object();
    ma6_set_str(da, "exc", ma6_exc_msg(exc));
    json_set(da, "proxy", ma6_copy_or_null(proxy));
    json_t *desc = ma6_call(ctx, "describe_proxy_error", da);
    json_free(proxy);
    if (ma6_is_raise(desc)) return desc;
    const char *shown = ma6_truthy(desc) ? ma6_str(desc) : ma6_exc_msg(exc);
    json_t *arr = json_array();
    json_append(arr, json_string(shown ? shown : ""));
    json_t *lr = ma6_log(ctx, "logger.warning", "Matrix: initial sync error: %s", arr);
    json_free(desc);
    return lr;
}

/* PoP: ma6_connect_initial_sync @ plugins/platforms/matrix/adapter.py:_connect_initial_sync */
/* _connect_initial_sync(self, client) -> None */
static json_t *ma6_connect_initial_sync(ma6_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const json_t *client = json_obj_get(args, "client");
    json_t *a = json_object();
    json_set(a, "timeout", json_number(10000));
    json_set(a, "full_state", json_bool(true));
    json_t *sync = ma6_call(ctx, "sync", a);
    if (ma6_is_raise(sync)) {
        if (!ma6_exc_is_exception(sync)) return sync;
        json_t *r = ma6_cis_except(ctx, self, sync);
        json_free(sync);
        if (ma6_is_raise(r)) return r;
        json_free(r);
        return json_null();
    }
    if (sync && sync->type == JSON_OBJECT) {
        json_t *jr = json_obj_get(self, "_joined_rooms");
        if (jr && jr->type == JSON_ARRAY) json_set(self, "_joined_rooms", json_array());
        json_t *aa = json_object();
        json_set(aa, "client", ma6_copy_or_null(client));
        json_set(aa, "sync_data", json_copy(sync));
        json_set(aa, "initial", json_bool(true));
        json_free(sync);
        json_t *r = ma6_call(ctx, "_absorb_sync", aa);
        if (ma6_is_raise(r)) {
            if (!ma6_exc_is_exception(r)) return r;
            json_t *er = ma6_cis_except(ctx, self, r);
            json_free(r);
            if (ma6_is_raise(er)) return er;
            json_free(er);
            return json_null();
        }
        json_free(r);
    } else {
        const char *tn = "NoneType";
        if (sync) {
            switch (sync->type) {
                case JSON_OBJECT: tn = "dict"; break;
                case JSON_ARRAY: tn = "list"; break;
                case JSON_STRING: tn = "str"; break;
                case JSON_NUMBER: tn = "int"; break;
                case JSON_BOOL: tn = "bool"; break;
                default: tn = "NoneType"; break;
            }
        }
        json_t *arr = json_array();
        json_append(arr, json_string(tn));
        json_t *r = ma6_log(ctx, "logger.warning", "Matrix: initial sync returned unexpected type %s", arr);
        json_free(sync);
        if (ma6_is_raise(r)) {
            if (!ma6_exc_is_exception(r)) return r;
            json_t *er = ma6_cis_except(ctx, self, r);
            if (ma6_is_raise(er)) return er;
            json_free(er);
            return json_null();
        }
        json_free(r);
    }
    return json_null();
}

/* PoP: ma6_extract_reply_context @ plugins/platforms/matrix/adapter.py:_extract_reply_context */
/* _extract_reply_context(self, room_id, body, relates_to) -> 5-tuple */
static json_t *ma6_extract_reply_context(ma6_ctx_t *ctx, const json_t *args) {
    const json_t *rt = json_obj_get(args, "relates_to");
    const json_t *mirt = (rt && rt->type == JSON_OBJECT) ? json_obj_get(rt, "m.in_reply_to") : NULL;
    const char *reply_to = (mirt && mirt->type == JSON_OBJECT) ? ma6_str(json_obj_get(mirt, "event_id")) : NULL;
    const char *body = ma6_str(json_obj_get(args, "body"));
    const char *room = ma6_str(json_obj_get(args, "room_id"));
    const char *rtext = NULL, *rauthor = NULL, *rname = NULL;
    char *body_owned = NULL;
    if (reply_to && *reply_to && body && strncmp(body, "> ", 2) == 0) {
        json_t *a = json_object();
        ma6_set_str(a, "body", body);
        json_t *r = ma6_call(ctx, "_extract_reply_fallback", a);
        if (ma6_is_raise(r)) return r;
        if (r && r->type == JSON_ARRAY && json_len(r) >= 2) {
            const json_t *v0 = json_get(r, 0), *v1 = json_get(r, 1);
            rtext = ma6_str(v0) ? ma6_strdup(ma6_str(v0)) : NULL;
            rauthor = ma6_str(v1) ? ma6_strdup(ma6_str(v1)) : NULL;
        }
        json_free(r);
        json_t *a2 = json_object();
        ma6_set_str(a2, "body", body);
        r = ma6_call(ctx, "_strip_reply_fallback", a2);
        if (ma6_is_raise(r)) { free((void*)rtext); free((void*)rauthor); return r; }
        body_owned = ma6_strdup(ma6_str(r));
        body = body_owned;
        json_free(r);
        if (rauthor && *rauthor) {
            json_t *a3 = json_object();
            ma6_set_str(a3, "room_id", room);
            ma6_set_str(a3, "user_id", rauthor);
            r = ma6_call(ctx, "_get_display_name", a3);
            if (ma6_is_raise(r)) { free((void*)rtext); free((void*)rauthor); free(body_owned); return r; }
            rname = ma6_str(r) ? ma6_strdup(ma6_str(r)) : NULL;
            json_free(r);
        }
    }
    json_t *out = json_array();
    json_append(out, body ? json_string(body) : json_null());
    json_append(out, (reply_to && *reply_to) ? json_string(reply_to) : json_null());
    json_append(out, rtext ? json_string(rtext) : json_null());
    json_append(out, rauthor ? json_string(rauthor) : json_null());
    json_append(out, rname ? json_string(rname) : json_null());
    free((void*)rtext); free((void*)rauthor); free((void*)rname); free(body_owned);
    return out;
}

/* PoP: ma6_check_e2ee_deps @ plugins/platforms/matrix/adapter.py:_check_e2ee_deps */
/* _check_e2ee_deps() -> bool (import gate; no seams) */
static json_t *ma6_check_e2ee_deps(ma6_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *blocked = json_obj_get(args, "blocked");
    /* LIVE returns False only when one of the four in-function imports fails:
     * mautrix.crypto (OlmMachine / PgCryptoStore), asyncpg or aiosqlite.
     * Blocking an unrelated module leaves the imports intact -> True. */
    static const char *E2EE_MODS[] = {"mautrix.crypto", "asyncpg", "aiosqlite"};
    bool ok = true;
    if (blocked && blocked->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(blocked) && ok; i++) {
            const char *mod = ma6_str(json_get(blocked, i));
            if (!mod) continue;
            for (size_t j = 0; j < sizeof(E2EE_MODS) / sizeof(E2EE_MODS[0]); j++) {
                if (strcmp(mod, E2EE_MODS[j]) == 0) { ok = false; break; }
            }
        }
    }
    return json_bool(ok);
}

/* PoP: ma6_create_matrix_session @ plugins/platforms/matrix/adapter.py:_create_matrix_session */
/* _create_matrix_session(proxy_url) */
static json_t *ma6_create_matrix_session(ma6_ctx_t *ctx, const json_t *args) {
    const char *proxy = ma6_str(json_obj_get(args, "proxy_url"));
    if (!proxy || !*proxy) {
        json_t *ta = json_object();
        json_t *tv = ma6_call(ctx, "gateway_trust_env", ta);
        if (ma6_is_raise(tv)) return tv;
        json_t *ca = json_object();
        json_set(ca, "trust_env", ma6_copy_or_null(tv));
        json_free(tv);
        return ma6_call(ctx, "ClientSession", ca);
    }
    /* lower(proxy.split("://")[0]).startswith("socks") */
    const char *sep = strstr(proxy, "://");
    size_t scheme_len = sep ? (size_t)(sep - proxy) : strlen(proxy);
    bool socks = scheme_len >= 5;
    if (socks) {
        for (size_t i = 0; i < 5; i++) {
            if (ma6_tolower(proxy[i]) != "socks"[i]) { socks = false; break; }
        }
    }
    bool socks_ok = json_get_bool(args, "socks_available", true);
    if (socks && socks_ok) {
        json_t *pa = json_object();
        ma6_set_str(pa, "url", proxy);
        json_set(pa, "rdns", json_bool(true));
        json_t *conn = ma6_call(ctx, "ProxyConnector.from_url", pa);
        if (ma6_is_raise(conn)) return conn;
        json_t *ca = json_object();
        json_set(ca, "connector", ma6_copy_or_null(conn));
        json_free(conn);
        return ma6_call(ctx, "ClientSession", ca);
    }
    if (socks && !socks_ok) {
        json_t *sa = json_object();
        ma6_set_str(sa, "url", proxy);
        json_t *safe = ma6_call(ctx, "safe_proxy_url_for_log", sa);
        if (ma6_is_raise(safe)) return safe;
        json_t *arr = json_array();
        json_append(arr, ma6_copy_or_null(safe));
        json_t *lr = ma6_log(ctx, "logger.warning",
            "aiohttp_socks not installed — SOCKS proxy %s ignored. Run: pip install aiohttp-socks", arr);
        json_free(safe);
        if (ma6_is_raise(lr)) return lr;
        json_free(lr);
        json_t *ta = json_object();
        json_t *tv = ma6_call(ctx, "gateway_trust_env", ta);
        if (ma6_is_raise(tv)) return tv;
        json_t *ca = json_object();
        json_set(ca, "trust_env", ma6_copy_or_null(tv));
        json_free(tv);
        return ma6_call(ctx, "ClientSession", ca);
    }
    json_t *ca = json_object();
    ma6_set_str(ca, "proxy", proxy);
    return ma6_call(ctx, "ClientSession", ca);
}

/* PoP: ma6_dispatch_sync @ plugins/platforms/matrix/adapter.py:_dispatch_sync */
/* _dispatch_sync(self, sync_data) -> None */
static json_t *ma6_dispatch_sync(ma6_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const json_t *client = ma6_attr_node(self, "_client");
    if (!client || client->type == JSON_NULL) return json_null();
    json_t *a = json_object();
    json_set(a, "sync_data", ma6_copy_or_null(json_obj_get(args, "sync_data")));
    json_t *tasks = ma6_call(ctx, "handle_sync", a);
    if (ma6_is_raise(tasks)) return tasks;
    bool awaitable = json_get_bool(args, "tasks_awaitable", false);
    if (awaitable) {
        /* scripted as {"__awaitable__": <list>} -> take inner value */
        const json_t *inner = (tasks && tasks->type == JSON_OBJECT) ? json_obj_get(tasks, "__awaitable__") : NULL;
        json_t *nt = inner ? json_copy(inner) : json_null();
        json_free(tasks); tasks = nt;
    }
    if (ma6_truthy(tasks)) {
        json_t *ga = json_object();
        json_set(ga, "tasks", ma6_copy_or_null(tasks));
        json_t *results = ma6_call(ctx, "gather", ga);
        json_free(tasks);
        if (ma6_is_raise(results)) return results;
        if (results && results->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(results); i++) {
                json_t *res = json_get(results, i);
                if (ma6_exc_is_exception(res)) {
                    json_t *arr = json_array();
                    json_append(arr, json_string(ma6_exc_msg(res)));
                    json_t *r = ma6_log(ctx, "logger.warning",
                        "Matrix: event handler failed during sync dispatch: %s", arr);
                    if (ma6_is_raise(r)) { json_free(results); return r; }
                    json_free(r);
                }
            }
        }
        json_free(results);
    } else {
        json_free(tasks);
    }
    return json_null();
}

/* PoP: ma6_build_text_message_content @ plugins/platforms/matrix/adapter.py:_build_text_message_content */
/* _build_text_message_content(self, text, msgtype="m.text") -> dict */
static json_t *ma6_build_text_message_content(ma6_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const char *text = ma6_str(json_obj_get(args, "text"));
    const char *msgtype = ma6_str(json_obj_get(args, "msgtype"));
    if (!msgtype || !*msgtype) msgtype = "m.text";
    json_t *out = json_object();
    ma6_set_str(out, "msgtype", msgtype);
    ma6_set_str(out, "body", text);
    json_t *a1 = json_object();
    ma6_set_str(a1, "text", text);
    json_t *ment = ma6_call(ctx, "_extract_outbound_mentions", a1);
    if (ma6_is_raise(ment)) { json_free(out); return ment; }
    if (ma6_truthy(ment)) {
        json_t *m = json_object();
        json_set(m, "user_ids", ma6_copy_or_null(ment));
        json_set(out, "m.mentions", m);
    }
    json_free(ment);
    bool allow = ma6_truthy(ma6_attr_node(self, "_allow_room_mentions"));
    if (allow) {
        json_t *a2 = json_object();
        ma6_set_str(a2, "text", text);
        json_t *rm = ma6_call(ctx, "_has_outbound_room_mention", a2);
        if (ma6_is_raise(rm)) { json_free(out); return rm; }
        bool hit = ma6_truthy(rm);
        json_free(rm);
        if (hit) {
            json_t *m = json_obj_get(out, "m.mentions");
            if (!m || m->type == JSON_NULL) { m = json_object(); json_set(out, "m.mentions", m); }
            json_set(m, "room", json_bool(true));
        }
    }
    json_t *a3 = json_object();
    ma6_set_str(a3, "text", text);
    json_t *linked = ma6_call(ctx, "_inject_outbound_mention_links", a3);
    if (ma6_is_raise(linked)) { json_free(out); return linked; }
    json_t *a4 = json_object();
    json_set(a4, "text", ma6_copy_or_null(linked));
    json_free(linked);
    json_t *html = ma6_call(ctx, "_markdown_to_html", a4);
    if (ma6_is_raise(html)) { json_free(out); return html; }
    const char *hs = ma6_str(html);
    if (hs && *hs && (!text || strcmp(hs, text) != 0)) {
        ma6_set_str(out, "format", "org.matrix.custom.html");
        json_set(out, "formatted_body", ma6_copy_or_null(html));
        json_t *a5 = json_object();
        json_set(a5, "content", ma6_copy_or_null(out));
        json_t *cb = ma6_call(ctx, "_content_bytes", a5);
        if (ma6_is_raise(cb)) { json_free(html); return cb; }
        long n = (long)ma6_num(cb);
        bool over = n > 45000; /* _MAX_CONTENT_BYTES */
        json_free(cb);
        if (over) {
            json_obj_del(out, "format");
            json_obj_del(out, "formatted_body");
        }
    }
    json_free(html);
    return out;
}

/* PoP: ma6_absorb_sync @ plugins/platforms/matrix/adapter.py:_absorb_sync */
/* _absorb_sync(self, client, sync_data, *, initial=False) -> next_batch */
static json_t *ma6_absorb_sync(ma6_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const json_t *client = json_obj_get(args, "client");
    const json_t *sd = json_obj_get(args, "sync_data");
    bool initial = json_get_bool(args, "initial", false);
    if (!EMPTY_OBJ) EMPTY_OBJ = json_object();
    json_t *ta = json_object();
    json_t *tv = ma6_call(ctx, "time.time", ta);
    if (ma6_is_raise(tv)) return tv;
    json_set(self, "_last_sync_ts", ma6_copy_or_null(tv));
    json_free(tv);
    const json_t *rooms = (sd && sd->type == JSON_OBJECT) ? json_obj_get(sd, "rooms") : NULL;
    const json_t *rj = (rooms && rooms->type == JSON_OBJECT) ? json_obj_get(rooms, "join") : NULL;
    if (!rj || rj->type != JSON_OBJECT) rj = EMPTY_OBJ; /* live .get("join", {}) default */
    bool has_join = json_len(rj) > 0;
    if (has_join || initial) {
        json_t *jr = json_obj_get(self, "_joined_rooms");
        if (!jr || jr->type != JSON_ARRAY) { jr = json_array(); json_set(self, "_joined_rooms", jr); }
        if (rj && rj->type == JSON_OBJECT) {
            for (size_t i = 0; i < json_object_size(rj); i++) {
                const char *k = json_object_get_key_at(rj, i);
                if (k) ma6_set_add(jr, k);
            }
        }
        json_t *ia = json_object();
        json_t *r = ma6_call(ctx, "_invalidate_room_identities", ia);
        if (ma6_is_raise(r)) return r;
        json_free(r);
    }
    json_t *wa = json_object();
    json_set(wa, "rooms_join", ma6_copy_or_null(rj));
    json_set(wa, "client", ma6_copy_or_null(client));
    json_t *wr = ma6_call(ctx, "_warn_encrypted_drops", wa);
    if (ma6_is_raise(wr)) return wr;
    json_free(wr);
    const json_t *nbnode = (sd && sd->type == JSON_OBJECT) ? json_obj_get(sd, "next_batch") : NULL;
    const char *nb = ma6_str(nbnode);
    if (nb && *nb) {
        json_t *pa = json_object();
        ma6_set_str(pa, "next_batch", nb);
        json_t *r = ma6_call(ctx, "put_next_batch", pa);
        if (ma6_is_raise(r)) return r;
        json_free(r);
    }
    if (initial) {
        json_t *jr = json_obj_get(self, "_joined_rooms");
        int cnt = (jr && jr->type == JSON_ARRAY) ? (int)json_len(jr) : 0;
        json_t *arr = json_array();
        json_append(arr, json_number(cnt));
        json_t *r = ma6_log(ctx, "logger.info", "Matrix: initial sync complete, joined %d rooms", arr);
        if (ma6_is_raise(r)) return r;
        json_free(r);
        json_t *ra = json_object();
        r = ma6_call(ctx, "_refresh_dm_cache", ra);
        if (ma6_is_raise(r)) return r;
        json_free(r);
    }
    json_t *da = json_object();
    json_set(da, "sync_data", ma6_copy_or_null(sd));
    json_t *dr = ma6_call(ctx, "_dispatch_sync", da);
    if (ma6_is_raise(dr)) {
        if (!ma6_exc_is_exception(dr)) return dr;
        json_t *arr = json_array();
        json_append(arr, json_string(initial ? "initial sync event dispatch error" : "sync event dispatch error"));
        json_append(arr, json_string(ma6_exc_msg(dr)));
        json_free(dr);
        json_t *r = ma6_log(ctx, "logger.warning", "Matrix: %s: %s", arr);
        if (ma6_is_raise(r)) return r;
        json_free(r);
    } else {
        json_free(dr);
    }
    json_t *sa = json_object();
    json_set(sa, "sync_data", ma6_copy_or_null(sd));
    json_t *sr = ma6_call(ctx, "_schedule_pending_invite_joins", sa);
    if (ma6_is_raise(sr)) return sr;
    json_free(sr);
    return nbnode ? json_copy(nbnode) : json_null();
}

/* PoP: ma6_download_and_cache_media @ plugins/platforms/matrix/adapter.py:_download_and_cache_media */
/* _download_and_cache_media(self, url, event_id, encrypted_file, msg_type, media_type, is_voice_message, body) */
static json_t *ma6_download_and_cache_media(ma6_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const char *url = ma6_str(json_obj_get(args, "url"));
    const char *eid = ma6_str(json_obj_get(args, "event_id"));
    const json_t *ef = json_obj_get(args, "encrypted_file");
    const char *msgtype = ma6_str(json_obj_get(args, "msg_type"));
    const char *media_type = ma6_str(json_obj_get(args, "media_type"));
    bool voice = json_get_bool(args, "is_voice_message", false);
    const char *body = ma6_str(json_obj_get(args, "body"));
    json_t *da = json_object();
    ma6_set_str(da, "url", url);
    json_set(da, "max_bytes", json_number(json_get_num(self, "_max_media_bytes", 0)));
    json_t *fb = ma6_call(ctx, "download_mxc_capped", da);
    if (ma6_is_raise(fb)) return fb;
    if (ef && ef->type == JSON_OBJECT) {
        const json_t *hashes = json_obj_get(ef, "hashes");
        const char *hv = (hashes && hashes->type == JSON_OBJECT) ? ma6_str(json_obj_get(hashes, "sha256")) : NULL;
        const json_t *kn = json_obj_get(ef, "key");
        const char *kv = (kn && kn->type == JSON_OBJECT) ? ma6_str(json_obj_get(kn, "k")) : ma6_str(kn);
        const char *iv = ma6_str(json_obj_get(ef, "iv"));
        if (!(kv && *kv && hv && *hv && iv && *iv)) {
            json_t *arr = json_array();
            json_append(arr, json_string(eid ? eid : ""));
            json_t *r = ma6_log(ctx, "logger.warning", "[Matrix] Encrypted media event missing decryption metadata for %s", arr);
            json_free(fb);
            if (ma6_is_raise(r)) return r;
            json_free(r);
            return json_null();
        }
        json_t *pa = json_object();
        json_set(pa, "file_bytes", ma6_copy_or_null(fb));
        ma6_set_str(pa, "key", kv);
        ma6_set_str(pa, "hash", hv);
        ma6_set_str(pa, "iv", iv);
        json_free(fb);
        fb = ma6_call(ctx, "decrypt_attachment", pa);
        if (ma6_is_raise(fb)) return fb;
    }
    if (msgtype && strcmp(msgtype, "photo") == 0) {
        const char *ext = ".jpg";
        if (media_type) {
            if (!strcmp(media_type, "image/jpeg")) ext = ".jpg";
            else if (!strcmp(media_type, "image/png")) ext = ".png";
            else if (!strcmp(media_type, "image/gif")) ext = ".gif";
            else if (!strcmp(media_type, "image/webp")) ext = ".webp";
        }
        json_t *ca = json_object();
        json_set(ca, "file_bytes", ma6_copy_or_null(fb));
        ma6_set_str(ca, "ext", ext);
        json_free(fb);
        json_t *r = ma6_call(ctx, "cache_image", ca);
        if (ma6_is_raise(r)) return r;
        json_t *arr = json_array();
        json_append(arr, ma6_copy_or_null(r));
        json_t *lr = ma6_log(ctx, "logger.info", "[Matrix] Cached user image at %s", arr);
        if (ma6_is_raise(lr)) return lr;
        json_free(lr);
        return r;
    }
    if (msgtype && (!strcmp(msgtype, "audio") || !strcmp(msgtype, "voice"))) {
        const char *name = (body && *body) ? body : (voice ? "voice.ogg" : "audio.ogg");
        const char *dot = strrchr(name, '.');
        const char *ext = (dot && dot[1]) ? dot : ".ogg";
        json_t *ca = json_object();
        json_set(ca, "file_bytes", ma6_copy_or_null(fb));
        ma6_set_str(ca, "ext", ext);
        json_free(fb);
        return ma6_call(ctx, "cache_audio", ca);
    }
    const char *fname = (body && *body) ? body : ((msgtype && !strcmp(msgtype, "video")) ? "video.mp4" : "document");
    json_t *ca = json_object();
    json_set(ca, "file_bytes", ma6_copy_or_null(fb));
    ma6_set_str(ca, "filename", fname);
    json_free(fb);
    return ma6_call(ctx, "cache_document", ca);
}

/* PoP: ma6_background_read_receipt @ plugins/platforms/matrix/adapter.py:_background_read_receipt */
/* _background_read_receipt(self, room_id, event_id) -> None (ensure_future schedules _send) */
static json_t *ma6_background_read_receipt(ma6_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    (void)args;
    /* The live body defines an async _send closure and hands it to
     * asyncio.ensure_future; the closure is not awaited at call time. The
     * harness/oracle model ensure_future as a seam that, when scripted to
     * "run", drives the closure (send_read_receipt + optional logger.debug).
     * The C port emits the ensure_future seam only; closure internals are
     * scripted through the same seam by the harness. */
    json_t *a = json_object();
    ma6_set_str(a, "room_id", ma6_str(json_obj_get(args, "room_id")));
    ma6_set_str(a, "event_id", ma6_str(json_obj_get(args, "event_id")));
    json_t *r = ma6_call(ctx, "ensure_future", a);
    if (ma6_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: ma6_markdown_to_html @ plugins/platforms/matrix/adapter.py:_markdown_to_html */
/* _markdown_to_html(self, text) -> str */
static json_t *ma6_markdown_to_html(ma6_ctx_t *ctx, const json_t *args) {
    const char *text = ma6_str(json_obj_get(args, "text"));
    bool md_available = json_get_bool(args, "markdown_available", true);
    json_t *a1 = json_object();
    ma6_set_str(a1, "text", text);
    json_t *s = ma6_call(ctx, "_pre_sanitize_matrix_markdown", a1);
    if (ma6_is_raise(s)) return s;
    json_t *a2 = json_object();
    json_set(a2, "text", ma6_copy_or_null(s));
    json_free(s);
    json_t *lt = ma6_call(ctx, "_latex_to_tokens", a2);
    if (ma6_is_raise(lt)) return lt;
    const char *lt_text = (lt && lt->type == JSON_ARRAY && json_len(lt) >= 1) ? ma6_str(json_get(lt, 0)) : "";
    char *clean = ma6_strdup(lt_text);
    const json_t *tex_store = (lt && lt->type == JSON_ARRAY && json_len(lt) >= 2) ? json_get(lt, 1) : NULL;
    if (md_available) {
        /* markdown import ok: construct Markdown(extensions=[...]) */
        json_t *ma = json_object();
        json_t *exts = json_array();
        const char *E[] = {"fenced_code", "tables", "nl2br", "sane_lists"};
        for (size_t i = 0; i < 4; i++) json_append(exts, json_string(E[i]));
        json_set(ma, "extensions", exts);
        json_t *md = ma6_call(ctx, "Markdown", ma);
        if (ma6_is_raise(md)) { free(clean); json_free(lt); return md; }
        /* if "html_block" in md.preprocessors: deregister */
        bool has_hb = json_get_bool(args, "html_block_present", false);
        if (has_hb) {
            json_t *da = json_object();
            json_t *dv = ma6_call(ctx, "md.preprocessors.deregister", da);
            if (ma6_is_raise(dv)) { free(clean); json_free(md); json_free(lt); return dv; }
            json_free(dv);
        }
        json_t *ca = json_object();
        ma6_set_str(ca, "text", clean);
        json_t *conv = ma6_call(ctx, "md.convert", ca);
        if (ma6_is_raise(conv)) { free(clean); json_free(md); json_free(lt); return conv; }
        json_t *ra = json_object();
        json_t *rv = ma6_call(ctx, "md.reset", ra);
        if (ma6_is_raise(rv)) { free(clean); json_free(conv); json_free(md); json_free(lt); return rv; }
        json_free(rv);
        const char *html = ma6_str(conv);
        char *final = ma6_strdup(html ? html : "");
        /* if html.count("<p>") == 1: strip one <p> and one </p> */
        if (html) {
            int cnt = 0; const char *p = html;
            while ((p = strstr(p, "<p>")) != NULL) { cnt++; p += 3; }
            if (cnt == 1) {
                char *a = ma6_strdup(html);
                char *q = strstr(a, "<p>");
                if (q) { memmove(q, q + 3, strlen(q + 3) + 1); }
                char *r2 = strstr(a, "</p>");
                if (r2) { *r2 = '\0'; }
                free(final); final = a;
            }
        }
        json_free(conv);
        json_t *sa = json_object();
        ma6_set_str(sa, "html", final);
        json_set(sa, "tex_store", ma6_copy_or_null(tex_store));
        free(final); free(clean); json_free(md); json_free(lt);
        return ma6_call(ctx, "_sanitize_matrix_html", sa);
    }
    json_t *fa = json_object();
    ma6_set_str(fa, "text", clean);
    json_t *fb = ma6_call(ctx, "_markdown_to_html_fallback", fa);
    if (ma6_is_raise(fb)) { free(clean); json_free(lt); return fb; }
    json_t *sa = json_object();
    json_set(sa, "html", ma6_copy_or_null(fb));
    json_set(sa, "tex_store", ma6_copy_or_null(tex_store));
    json_free(fb); free(clean); json_free(lt);
    return ma6_call(ctx, "_sanitize_matrix_html", sa);
}

/* PoP: ma6_migrate_legacy_crypto_pickle @ plugins/platforms/matrix/adapter.py:_migrate_legacy_crypto_pickle */
/* _migrate_legacy_crypto_pickle(self, crypto_store, crypto_db, acct_id, pickle_key) -> bool */
static json_t *ma6_migrate_legacy_crypto_pickle(ma6_ctx_t *ctx, const json_t *args) {
    const json_t *store = json_obj_get(args, "crypto_store");
    const json_t *db = json_obj_get(args, "crypto_db");
    const char *acct = ma6_str(json_obj_get(args, "acct_id"));
    const char *pkey = ma6_str(json_obj_get(args, "pickle_key"));
    bool fast_ok = json_get_bool(args, "fast_ok", true);
    json_t *ga = json_object();
    json_t *first = ma6_call(ctx, "get_account", ga);
    if (!ma6_is_raise(first)) {
        json_free(first);
        return json_bool(true);
    }
    /* with suppress(Exception): await crypto_store.get_account() -- fast path raised */
    if (!ma6_exc_is_exception(first)) return first; /* BaseException escapes suppress */
    json_free(first);
    const char *legacy[2];
    char k0[512], k1[512];
    snprintf(k0, sizeof k0, "%s:default", acct ? acct : "");
    k0[sizeof k0 - 1] = '\0';
    snprintf(k1, sizeof k1, "%s", acct ? acct : "");
    legacy[0] = k0; legacy[1] = k1;
    for (int i = 0; i < 2; i++) {
        if (pkey && strcmp(legacy[i], pkey) == 0) continue;
        json_t *pa = json_object();
        ma6_set_str(pa, "account_id", acct);
        ma6_set_str(pa, "pickle_key", legacy[i]);
        json_set(pa, "db", ma6_copy_or_null(db));
        json_t *acct_obj = ma6_call(ctx, "PgCryptoStore.get_account", pa);
        json_t *account = NULL;
        if (ma6_is_raise(acct_obj)) {
            if (!ma6_exc_is_exception(acct_obj)) return acct_obj;
            json_free(acct_obj);
        } else if (!ma6_is_null(acct_obj)) {
            account = acct_obj;
        } else {
            json_free(acct_obj);
        }
        if (!account) continue;
        json_t *ra = json_object();
        json_set(ra, "db", ma6_copy_or_null(db));
        ma6_set_str(ra, "acct_id", acct);
        ma6_set_str(ra, "legacy_key", legacy[i]);
        ma6_set_str(ra, "pickle_key", pkey);
        json_t *rr = ma6_call(ctx, "_repickle_crypto_sessions", ra);
        if (ma6_is_raise(rr)) {
            if (!ma6_exc_is_exception(rr)) { json_free(account); return rr; }
            json_t *arr = json_array();
            json_append(arr, json_string(ma6_exc_msg(rr)));
            json_free(rr);
            json_t *r = ma6_log(ctx, "logger.error",
                "Matrix: pickle key migration failed while re-pickling sessions (%s) — leaving the account under the legacy key so the migration is retried on the next start.", arr);
            json_free(account);
            if (ma6_is_raise(r)) return r;
            json_free(r);
            return json_bool(false);
        }
        json_free(rr);
        json_t *pa2 = json_object();
        json_set(pa2, "account", ma6_copy_or_null(account));
        json_free(account);
        json_t *pr = ma6_call(ctx, "put_account", pa2);
        if (ma6_is_raise(pr)) return pr;
        json_free(pr);
        json_t *arr = json_array();
        json_t *lr = ma6_log(ctx, "logger.info",
            "Matrix: re-pickled crypto store account and sessions under the current pickle key (device ID was configured after the account was created)", arr);
        if (ma6_is_raise(lr)) return lr;
        json_free(lr);
        return json_bool(true);
    }
    json_t *arr = json_array();
    json_t *r = ma6_log(ctx, "logger.error",
        "Matrix: crypto store account exists but cannot be unpickled with the current or any legacy pickle key. If MATRIX_DEVICE_ID was changed manually, restore its previous value.", arr);
    if (ma6_is_raise(r)) return r;
    json_free(r);
    return json_bool(false);
}

/* PoP: ma6_build_inbound_event @ plugins/platforms/matrix/adapter.py:_build_inbound_event */
/* _build_inbound_event(self, room_id, sender, event_id, body, source_content, relates_to, ctx=None, **extra) */
static json_t *ma6_build_inbound_event(ma6_ctx_t *ctx, const json_t *args) {
    const json_t *ctx_node = json_obj_get(args, "ctx");
    json_t *resolved = NULL;
    if (!ctx_node || ctx_node->type == JSON_NULL) {
        json_t *a = json_object();
        ma6_set_str(a, "room_id", ma6_str(json_obj_get(args, "room_id")));
        ma6_set_str(a, "sender", ma6_str(json_obj_get(args, "sender")));
        ma6_set_str(a, "event_id", ma6_str(json_obj_get(args, "event_id")));
        ma6_set_str(a, "body", ma6_str(json_obj_get(args, "body")));
        json_set(a, "source_content", ma6_copy_or_null(json_obj_get(args, "source_content")));
        json_set(a, "relates_to", ma6_copy_or_null(json_obj_get(args, "relates_to")));
        resolved = ma6_call(ctx, "_resolve_message_context", a);
        if (ma6_is_raise(resolved)) return resolved;
        if (ma6_is_null(resolved)) return json_null();
    } else {
        resolved = json_copy(ctx_node);
    }
    const char *b0 = (resolved && resolved->type == JSON_ARRAY && json_len(resolved) >= 1) ? ma6_str(json_get(resolved, 0)) : "";
    const char *d0 = (resolved && resolved->type == JSON_ARRAY && json_len(resolved) >= 5) ? ma6_str(json_get(resolved, 4)) : "";
    const char *s0 = (resolved && resolved->type == JSON_ARRAY && json_len(resolved) >= 6) ? ma6_str(json_get(resolved, 5)) : "";
    char *body = ma6_strdup(b0 ? b0 : "");
    char *display = ma6_strdup(d0 ? d0 : "");
    char *source = ma6_strdup(s0 ? s0 : "");
    json_free(resolved);
    char *bbody = ma6_strdup(body ? body : "");
    json_t *ea = json_object();
    ma6_set_str(ea, "room_id", ma6_str(json_obj_get(args, "room_id")));
    ma6_set_str(ea, "body", bbody);
    json_set(ea, "relates_to", ma6_copy_or_null(json_obj_get(args, "relates_to")));
    json_t *erc = ma6_call(ctx, "_extract_reply_context", ea);
    if (ma6_is_raise(erc)) { free(bbody); return erc; }
    const char *ebody = (erc && erc->type == JSON_ARRAY && json_len(erc) >= 1) ? ma6_str(json_get(erc, 0)) : bbody;
    char *final_body = ma6_strdup(ebody ? ebody : "");
    const char *mtype_val = NULL;
    json_t *extra = json_obj_get(args, "extra");
    const json_t *mmt = (extra && extra->type == JSON_OBJECT) ? json_obj_get(extra, "media_msgtype") : NULL;
    bool mmt_null = !mmt || mmt->type == JSON_NULL;
    if (mmt_null) {
        json_t *na = json_object();
        ma6_set_str(na, "body", final_body);
        json_t *norm = ma6_call(ctx, "_normalize_matrix_bang_command", na);
        if (ma6_is_raise(norm)) { free(bbody); free(final_body); json_free(erc); return norm; }
        const char *nb = ma6_str(norm);
        free(final_body);
        final_body = ma6_strdup(nb ? nb : "");
        json_free(norm);
        mtype_val = (final_body[0] == '/') ? "command" : "text";
    } else {
        json_t *ba = json_object();
        ma6_set_str(ba, "media_msgtype", ma6_str(mmt));
        ma6_set_str(ba, "body", final_body);
        json_t *bare = ma6_call(ctx, "_is_bare_media_filename", ba);
        if (ma6_is_raise(bare)) { free(bbody); free(final_body); json_free(erc); return bare; }
        if (ma6_truthy(bare)) { free(final_body); final_body = ma6_strdup(""); }
        json_free(bare);
    }
    /* MessageEvent seam: emit the constructed event as args; ret passed through */
    json_t *mea = json_object();
    ma6_set_str(mea, "text", final_body);
    ma6_set_str(mea, "source", source);
    json_set(mea, "raw_message", ma6_copy_or_null(json_obj_get(args, "source_content")));
    ma6_set_str(mea, "message_id", ma6_str(json_obj_get(args, "event_id")));
    ma6_set_str(mea, "user_id", ma6_str(json_obj_get(args, "sender")));
    ma6_set_str(mea, "user_name", display);
    const json_t *r0 = (erc && erc->type == JSON_ARRAY && json_len(erc) >= 2) ? json_get(erc, 1) : NULL;
    const json_t *r1 = (erc && erc->type == JSON_ARRAY && json_len(erc) >= 3) ? json_get(erc, 2) : NULL;
    const json_t *r2 = (erc && erc->type == JSON_ARRAY && json_len(erc) >= 4) ? json_get(erc, 3) : NULL;
    const json_t *r3 = (erc && erc->type == JSON_ARRAY && json_len(erc) >= 5) ? json_get(erc, 4) : NULL;
    json_set(mea, "reply_to_message_id", ma6_copy_or_null(r0));
    json_set(mea, "reply_to_text", ma6_copy_or_null(r1));
    json_set(mea, "reply_to_author_id", ma6_copy_or_null(r2));
    json_set(mea, "reply_to_author_name", ma6_copy_or_null(r3));
    if (extra && extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(extra); i++) {
            const char *k = json_object_get_key_at(extra, i);
            if (k && strcmp(k, "media_msgtype") != 0) json_set(mea, k, json_copy(json_object_get_at(extra, i)));
        }
    }
    if (mtype_val) ma6_set_str(mea, "message_type", mtype_val);
    free(bbody); free(final_body); free(body); free(display); free(source); json_free(erc);
    return ma6_call(ctx, "MessageEvent", mea);
}

/* PoP: ma6_handle_text_message @ plugins/platforms/matrix/adapter.py:_handle_text_message */
/* _handle_text_message(self, room_id, sender, event_id, event_ts, source_content, relates_to) -> None */
static json_t *ma6_handle_text_message(ma6_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const char *room = ma6_str(json_obj_get(args, "room_id"));
    const char *sender = ma6_str(json_obj_get(args, "sender"));
    const char *eid = ma6_str(json_obj_get(args, "event_id"));
    double ts = json_get_num(args, "event_ts", 0);
    const json_t *sc = json_obj_get(args, "source_content");
    const char *body = (sc && sc->type == JSON_OBJECT) ? ma6_str(json_obj_get(sc, "body")) : NULL;
    if (!body || !*body) return json_null();
    json_t *sa = json_object();
    ma6_set_str(sa, "room_id", room);
    ma6_set_str(sa, "sender", sender);
    json_t *pr = ma6_call(ctx, "pending", sa);
    if (ma6_is_raise(pr)) return pr;
    bool pend = ma6_truthy(pr);
    json_free(pr);
    if (pend) {
        json_t *sta = json_object();
        ma6_set_str(sta, "body", body);
        json_t *stripped = ma6_call(ctx, "_strip_mention", sta);
        if (ma6_is_raise(stripped)) return stripped;
        char *ss = ma6_strip(ma6_str(stripped));
        json_free(stripped);
        bool empty = ss[0] == '\0';
        free(ss);
        if (empty) {
                json_t *ca = json_object();
                ma6_set_str(ca, "body", body);
                json_set(ca, "content", ma6_copy_or_null(sc));
                json_t *cm = ma6_call(ctx, "_content_mentions_bot", ca);
                if (ma6_is_raise(cm)) return cm;
                bool mentions = ma6_truthy(cm);
                json_free(cm);
                if (mentions) {
                    json_t *ma = json_object();
                    json_t *mr = ma6_call(ctx, "mark", ma);
                    if (ma6_is_raise(mr)) return mr;
                    double limit = ma6_num(mr);
                    json_free(mr);
                    json_t *sea = json_object();
                    ma6_set_str(sea, "room_id", room);
                    ma6_set_str(sea, "sender", sender);
                    json_t *sr = ma6_call(ctx, "settle", sea);
                    if (ma6_is_raise(sr)) return sr;
                    json_free(sr);
                    json_t *cla = json_object();
                    ma6_set_str(cla, "room_id", room);
                    ma6_set_str(cla, "sender", sender);
                    json_set(cla, "before", json_number(limit));
                    json_t *parked = ma6_call(ctx, "claim", cla);
                    if (ma6_is_raise(parked)) return parked;
                    if (parked && parked->type == JSON_ARRAY && json_len(parked) >= 3) {
                        const char *vid = ma6_str(json_get(parked, 0));
                        const json_t *vc = json_get(parked, 1);
                        const json_t *vr = json_get(parked, 2);
                        json_t *ha = json_object();
                        ma6_set_str(ha, "room_id", room);
                        ma6_set_str(ha, "sender", sender);
                        ma6_set_str(ha, "event_id", vid);
                        json_set(ha, "event_ts", json_number(ts));
                        json_set(ha, "source_content", ma6_copy_or_null(vc));
                        json_set(ha, "relates_to", ma6_copy_or_null(vr));
                        ma6_set_str(ha, "msgtype", "m.audio");
                        json_set(ha, "mention_claimed", json_bool(true));
                        json_free(parked);
                        json_t *hr = ma6_call(ctx, "_handle_media_message", ha);
                        if (ma6_is_raise(hr)) return hr;
                        json_free(hr);
                        json_t *bra = json_object();
                        ma6_set_str(bra, "room_id", room);
                        ma6_set_str(bra, "event_id", eid);
                        json_t *br = ma6_call(ctx, "_background_read_receipt", bra);
                        if (ma6_is_raise(br)) return br;
                        json_free(br);
                        return json_null();
                    }
                    json_free(parked);
                }
            }
        }
    json_t *na = json_object();
    ma6_set_str(na, "body", body);
    json_t *norm = ma6_call(ctx, "_normalize_matrix_bang_command", na);
    if (ma6_is_raise(norm)) return norm;
    json_t *ba = json_object();
    ma6_set_str(ba, "room_id", room);
    ma6_set_str(ba, "sender", sender);
    ma6_set_str(ba, "event_id", eid);
    json_set(ba, "body", ma6_copy_or_null(norm));
    json_set(ba, "source_content", ma6_copy_or_null(sc));
    json_set(ba, "relates_to", ma6_copy_or_null(json_obj_get(args, "relates_to")));
    json_free(norm);
    json_t *ev = ma6_call(ctx, "_build_inbound_event", ba);
    if (ma6_is_raise(ev)) return ev;
    if (ma6_is_null(ev)) return json_null();
    /* msg_event.message_type check: scripted via an attribute carrier */
    const char *mt = ma6_attr(ev, "message_type");
    bool is_text = mt && strcmp(mt, "text") == 0;
    double delay = json_get_num(self, "_text_batch_delay_seconds", 0);
    json_t *out = json_object();
    if (is_text && delay > 0) {
        json_set(out, "event", ma6_copy_or_null(ev));
        json_free(ev);
        json_t *r = ma6_call(ctx, "_enqueue_text_event", out);
        if (ma6_is_raise(r)) return r;
        json_free(r);
    } else {
        json_set(out, "event", ma6_copy_or_null(ev));
        json_free(ev);
        json_t *r = ma6_call(ctx, "handle_message", out);
        if (ma6_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}
