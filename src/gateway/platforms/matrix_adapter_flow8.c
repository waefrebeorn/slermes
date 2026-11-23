/*
 * matrix_adapter_flow8.c -- FLOW port of the remaining UNCLAIMED helpers of
 * plugins/platforms/matrix/adapter.py (hermes-agent): EA header, auth/E2EE
 * connect, capped media download, inbound media handling, reaction redaction,
 * approval/picker prompts, local-file upload, room-message send, markdown
 * fallback, standalone send/payloads, recovery-key output and handoff threads.
 *
 * flow1..flow7 siblings and their claimed funcs are NOT re-ported. Every
 * collaborator below enters through the ONE universal seam as a bare name --
 * none is re-ported. All symbols are static and ma8_-prefixed; libjson only.
 *
 * Decision sequences only: every live I/O / injected dependency enters through
 * ma8_call(ctx, name, args). The harness wires it to a recording stub fed the
 * case's scripted results (in.io); the oracle installs recording stubs emitting
 * the identical event shape {"name","args","ret"} from the SAME scripts.
 *
 * Representation contract (shared with tests/oracle_matrix_adapter_flow8.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; scripted raises
 *   round-trip verbatim when they escape a drive. except Exception swallows the
 *   standard Exception family; SystemExit/KeyboardInterrupt/GeneratorExit pass.
 * - Attribute carriers are {"__attr__": {...}}; sets emit as sorted arrays.
 * - logger.* calls are seams recorded with EMPTY args (only call order/name is
 *   in the contract; long format strings are deliberately not compared).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- universal seam context -- */
typedef struct ma8_ctx {
    json_t *state;
    const json_t *io;
    json_t *cursors;
    json_t *trace;
} ma8_ctx_t;

/* -- exception / value helpers -- */
static bool ma8_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static json_t *ma8_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type ? type : "RuntimeError");
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *ma8_make_exc(const json_t *spec) {
    return ma8_err(json_get_str(spec, "type", "RuntimeError"),
                   json_get_str(spec, "message", ""));
}
static bool ma8_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ma8_truthy(const json_t *v) {
    if (ma8_is_null(v)) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool ma8_obj_truthy(const json_t *v) {
    if (!v || v->type != JSON_OBJECT) return ma8_truthy(v);
    if (json_obj_get(v, "__attr__")) return true;
    return v->c.count > 0;
}
static const char *ma8_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static double ma8_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static bool ma8_bool_of(const json_t *v) { return (v && v->type == JSON_BOOL) ? v->bool_val : false; }
static json_t *ma8_copy_or_null(const json_t *v) { return v ? json_copy(v) : json_null(); }
static void ma8_set_str(json_t *obj, const char *key, const char *s) {
    json_set(obj, key, s ? json_string(s) : json_null());
}
static const char *ma8_exc_msg(const json_t *v) {
    if (!v) return "";
    if (ma8_is_raise(v)) return json_get_str(v, "message", "");
    return ma8_str(v) ? ma8_str(v) : "";
}
static const char *ma8_exc_kind(const json_t *v) {
    if (!v || !ma8_is_raise(v)) return "";
    return json_get_str(v, "__raise__", "");
}
static const json_t *ma8_attr_node(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    const json_t *in = json_obj_get(obj, "__attr__");
    if (in && in->type == JSON_OBJECT) return json_obj_get(in, key);
    return json_obj_get(obj, key);
}
static const char *ma8_attr(const json_t *obj, const char *key) {
    return ma8_str(ma8_attr_node(obj, key));
}
static double ma8_attr_num(const json_t *obj, const char *key) {
    return ma8_num(ma8_attr_node(obj, key));
}
static bool ma8_arr_has_str(const json_t *arr, const char *s) {
    if (!s || !arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *v = ma8_str(json_get(arr, i));
        if (v && strcmp(v, s) == 0) return true;
    }
    return false;
}
static bool ma8_exc_is_exception(const json_t *v) {
    if (!ma8_is_raise(v)) return false;
    const char *k = ma8_exc_kind(v);
    if (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt") ||
        !strcmp(k, "GeneratorExit") || !strcmp(k, "BaseException")) return false;
    return true;
}

static char *ma8_strdup(const char *s);

static char *ma8_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ma8_strdup("None");
    switch (v->type) {
        case JSON_STRING: return ma8_strdup(v->str_val);
        case JSON_BOOL: return ma8_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
            else snprintf(buf, sizeof buf, "%.17g", d);
            return ma8_strdup(buf);
        }
        default: return ma8_strdup("");
    }
}

/* -- growable string buffer -- */
typedef struct { char *b; size_t n, cap; } ma8_buf_t;
static void mb_reserve(ma8_buf_t *b, size_t need) {
    if (b->b && b->n + need + 1 <= b->cap) return;
    size_t nc = b->cap ? b->cap : 32;
    while (nc < b->n + need + 1) nc *= 2;
    char *nb = realloc(b->b, nc);
    if (!nb) return;
    b->b = nb; b->cap = nc;
    if (b->n == 0) b->b[0] = '\0';
}
static void mb_addn(ma8_buf_t *b, const char *s, size_t n) {
    if (!s || n == 0) return;
    mb_reserve(b, n);
    if (!b->b) return;
    memcpy(b->b + b->n, s, n);
    b->n += n; b->b[b->n] = '\0';
}
static void mb_add(ma8_buf_t *b, const char *s) { if (s) mb_addn(b, s, strlen(s)); }
static void mb_addc(ma8_buf_t *b, char c) { mb_reserve(b, 1); if (b->b) { b->b[b->n++] = c; b->b[b->n] = '\0'; } }
static char *mb_take(ma8_buf_t *b) {
    if (!b->b) { char *e = malloc(1); if (e) e[0] = '\0'; return e; }
    b->b[b->n] = '\0';
    char *r = b->b; b->b = NULL; b->n = b->cap = 0; return r;
}
static char *ma8_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s); char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *ma8_strip(const char *s) {
    if (!s) return ma8_strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}
static char *ma8_lower(const char *s) {
    char *o = ma8_strdup(s ? s : "");
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static bool ma8_starts(const char *s, const char *pfx) {
    return s && pfx && strncmp(s, pfx, strlen(pfx)) == 0;
}
static const char *ma8_basename(const char *p) {
    if (!p) return "";
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}
static const char *ma8_nested_str(const json_t *obj, const char *key, const char *sub) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    const json_t *n = json_obj_get(obj, key);
    if (!n || n->type != JSON_OBJECT) return NULL;
    return ma8_str(json_obj_get(n, sub));
}
static json_t *ma8_obj_keys(const json_t *obj) {
    json_t *a = json_array();
    if (obj && obj->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(obj); i++)
            json_append(a, json_string(json_object_get_key_at(obj, i)));
    }
    return a;
}
/* An attr-double seam result projects to its inner dict when it is the
 * function's final return (the oracle's build=attr_double + canon does this). */
static json_t *ma8_unwrap_attr(json_t *v) {
    if (v && v->type == JSON_OBJECT) {
        const json_t *inner = json_obj_get(v, "__attr__");
        if (inner && inner->type == JSON_OBJECT) {
            json_t *out = json_copy(inner);
            json_free(v);
            return out;
        }
    }
    return v;
}

/* The one universal seam. */
static json_t *ma8_call(ma8_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    char msg[256];
    if (!entry) {
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ma8_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ma8_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) ret = ma8_make_exc(raise);
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
static json_t *ma8_log0(ma8_ctx_t *ctx, const char *name) {
    return ma8_call(ctx, name, json_object());
}

/* ===================== ported helpers ===================== */

/* PoP: ma8_ea_header @ plugins/platforms/matrix/adapter.py:_EA_HEADER */
static json_t *ma8_ea_header(ma8_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "key", "gateway.exec_approval.header");
    json_t *r = ma8_call(ctx, "t", a);
    if (ma8_is_raise(r)) return r;
    ma8_buf_t b = {0};
    mb_add(&b, "\xe2\x9a\xa0\xef\xb8\x8f **");
    mb_add(&b, ma8_str(r));
    mb_add(&b, "**\n");
    char *s = mb_take(&b);
    json_free(r);
    json_t *out = json_string(s);
    free(s);
    return out;
}

/* PoP: ma8_send_room_message @ plugins/platforms/matrix/adapter.py:_send_room_message */
static json_t *ma8_send_room_message(ma8_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    ma8_set_str(a, "room_id", ma8_str(json_obj_get(args, "chat_id")));
    json_set_hdr(a, "event_type", "m.room.message");
    json_set(a, "msg_content", ma8_copy_or_null(json_obj_get(args, "msg_content")));
    json_t *r = ma8_call(ctx, "client.send_message_event", a);
    if (ma8_is_raise(r)) return r;
    char *s = ma8_py_str(r);
    json_free(r);
    json_t *out = json_string(s);
    free(s);
    return out;
}

/* PoP: ma8_connect_authenticate @ plugins/platforms/matrix/adapter.py:_connect_authenticate */
static json_t *ma8_connect_authenticate(ma8_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const char *access = json_get_str(self, "_access_token", "");
    const char *cfg_user = json_get_str(self, "_user_id", "");
    const char *cfg_dev = json_get_str(self, "_device_id", "");
    const char *pw = json_get_str(self, "_password", "");
    char *mxid = ma8_strdup(cfg_user && *cfg_user ? cfg_user : "@me:s");
    char *devid = ma8_strdup(cfg_dev ? cfg_dev : "");

    if (access && *access) {
        json_t *r = ma8_call(ctx, "client.whoami", json_object());
        if (ma8_is_raise(r)) {
            if (!ma8_exc_is_exception(r)) { free(mxid); free(devid); return r; }
            json_free(r);
            json_t *lr = ma8_log0(ctx, "logger.error");
            if (ma8_is_raise(lr)) { free(mxid); free(devid); return lr; }
            json_free(lr);
            json_t *ab = ma8_call(ctx, "_abort_connect", json_object());
            free(mxid); free(devid);
            return ab;
        }
        const char *ru = ma8_attr(r, "user_id");
        const char *rd = ma8_attr(r, "device_id");
        const char *resolved_user = (ru && *ru) ? ru : cfg_user;
        const char *resolved_dev = rd ? rd : "";
        if (resolved_user && *resolved_user) {
            free(mxid); mxid = ma8_strdup(resolved_user);
        }
        const char *eff = NULL;
        if (resolved_dev && *resolved_dev && cfg_dev && *cfg_dev && strcmp(resolved_dev, cfg_dev) != 0) {
            json_t *lr = ma8_log0(ctx, "logger.error");
            if (ma8_is_raise(lr)) { json_free(r); free(mxid); free(devid); return lr; }
            json_free(lr);
            eff = resolved_dev;
        } else {
            eff = (cfg_dev && *cfg_dev) ? cfg_dev : resolved_dev;
        }
        if (eff && *eff) { free(devid); devid = ma8_strdup(eff); }
        json_free(r);

        if (!devid || !*devid) {
            json_t *a = json_object();
            ma8_set_str(a, "mxid", mxid);
            json_t *qr = ma8_call(ctx, "client.query_keys", a);
            if (ma8_is_raise(qr)) {
                if (!ma8_exc_is_exception(qr)) { free(mxid); free(devid); return qr; }
                json_free(qr);
                json_t *lr = ma8_log0(ctx, "logger.warning");
                if (ma8_is_raise(lr)) { free(mxid); free(devid); return lr; }
                json_free(lr);
            } else {
                const json_t *dk = ma8_attr_node(qr, "device_keys");
                const json_t *all = dk ? json_obj_get(dk, mxid) : NULL;
                size_t n = (all && all->type == JSON_OBJECT) ? json_object_size(all) : 0;
                if (n == 1) {
                    free(devid); devid = ma8_strdup(json_object_get_key_at(all, 0));
                } else if (n == 0) {
                    json_t *lr = ma8_log0(ctx, "logger.warning");
                    if (ma8_is_raise(lr)) { json_free(qr); free(mxid); free(devid); return lr; }
                    json_free(lr);
                }
                json_free(qr);
            }
        }
        if (!devid || !*devid) {
            json_t *lr = ma8_log0(ctx, "logger.warning");
            if (ma8_is_raise(lr)) { free(mxid); free(devid); return lr; }
            json_free(lr);
        }
        json_t *lr = ma8_log0(ctx, "logger.info");
        if (ma8_is_raise(lr)) { free(mxid); free(devid); return lr; }
        json_free(lr);
        free(mxid); free(devid);
        return json_bool(true);
    } else if (pw && *pw && cfg_user && *cfg_user) {
        json_t *a = json_object();
        ma8_set_str(a, "identifier", cfg_user);
        ma8_set_str(a, "password", pw);
        json_set_hdr(a, "device_name", "Hermes Agent");
        if (cfg_dev && *cfg_dev) ma8_set_str(a, "device_id", cfg_dev);
        else json_set(a, "device_id", json_null());
        json_t *r = ma8_call(ctx, "client.login", a);
        if (ma8_is_raise(r)) {
            if (!ma8_exc_is_exception(r)) { free(mxid); free(devid); return r; }
            json_free(r);
            json_t *lr = ma8_log0(ctx, "logger.error");
            if (ma8_is_raise(lr)) { free(mxid); free(devid); return lr; }
            json_free(lr);
            json_t *ab = ma8_call(ctx, "_abort_connect", json_object());
            free(mxid); free(devid);
            return ab;
        }
        const char *rd = ma8_attr(r, "device_id");
        if (rd) { free(devid); devid = ma8_strdup(rd); }
        json_free(r);
        json_t *lr = ma8_log0(ctx, "logger.info");
        if (ma8_is_raise(lr)) { free(mxid); free(devid); return lr; }
        json_free(lr);
        free(mxid); free(devid);
        return json_bool(true);
    } else {
        json_t *lr = ma8_log0(ctx, "logger.error");
        if (ma8_is_raise(lr)) { free(mxid); free(devid); return lr; }
        json_free(lr);
        json_t *ab = ma8_call(ctx, "_abort_connect", json_object());
        free(mxid); free(devid);
        return ab;
    }
}

/* PoP: ma8_connect_setup_e2ee @ plugins/platforms/matrix/adapter.py:_connect_setup_e2ee */
static json_t *ma8_connect_setup_e2ee(ma8_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const char *devid = ma8_attr(json_obj_get(args, "client"), "device_id");
    const char *mode = json_get_str(self, "_e2ee_mode", "");
    json_t *r = ma8_call(ctx, "check_e2ee_deps", json_object());
    if (ma8_is_raise(r)) return r;
    if (!ma8_truthy(r)) {
        json_free(r);
        if (mode && !strcmp(mode, "optional")) {
            json_t *lr = ma8_log0(ctx, "logger.warning");
            if (ma8_is_raise(lr)) return lr;
            json_free(lr);
            return json_bool(true);
        }
        json_t *lr = ma8_log0(ctx, "logger.error");
        if (ma8_is_raise(lr)) return lr;
        json_free(lr);
        return ma8_call(ctx, "_abort_connect", json_object());
    }
    json_free(r);
    if (!ma8_truthy(json_obj_get(self, "_encryption"))) return json_bool(true);

    const char *phase = "import";
    json_t *a = json_object();
    json_set(a, "parents", json_bool(true));
    json_set(a, "exist_ok", json_bool(true));
    r = ma8_call(ctx, "store_dir.mkdir", a);
    if (ma8_is_raise(r)) goto e2e_fail;
    json_free(r);
    phase = "create";
    r = ma8_log0(ctx, "legacy_pickle.exists");
    if (ma8_is_raise(r)) goto e2e_fail;
    bool legacy = ma8_truthy(r);
    json_free(r);
    if (legacy) {
        r = ma8_log0(ctx, "legacy_pickle.unlink");
        if (ma8_is_raise(r)) goto e2e_fail;
        json_free(r);
    }
    const char *acct = json_get_str(self, "_user_id", "");
    if (!acct || !*acct) acct = "hermes";
    const char *dbp = json_get_str(self, "_crypto_db_path", "");
    char url[512];
    snprintf(url, sizeof url, "sqlite:///%s", dbp ? dbp : "");
    a = json_object();
    ma8_set_str(a, "url", url);
    r = ma8_call(ctx, "Database.create", a);
    if (ma8_is_raise(r)) goto e2e_fail;
    json_free(r);
    r = ma8_log0(ctx, "db.start");
    if (ma8_is_raise(r)) goto e2e_fail;
    json_free(r);
    const char *cfg_dev = json_get_str(self, "_device_id", "");
    const char *keydev = (devid && *devid) ? devid : ((cfg_dev && *cfg_dev) ? cfg_dev : "default");
    char pickle[512];
    snprintf(pickle, sizeof pickle, "%s:%s", acct, keydev);
    a = json_object();
    ma8_set_str(a, "account_id", acct);
    ma8_set_str(a, "pickle_key", pickle);
    r = ma8_call(ctx, "PgCryptoStore", a);
    if (ma8_is_raise(r)) goto e2e_fail;
    json_free(r);
    r = ma8_log0(ctx, "store.open");
    if (ma8_is_raise(r)) goto e2e_fail;
    json_free(r);
    bool reset = false;
    if (devid && *devid) {
        a = json_object();
        ma8_set_str(a, "device_id", devid);
        r = ma8_call(ctx, "reset_crypto_store", a);
        if (ma8_is_raise(r)) goto e2e_fail;
        reset = ma8_truthy(r);
        json_free(r);
        r = ma8_log0(ctx, "store.put_device_id");
        if (ma8_is_raise(r)) goto e2e_fail;
        json_free(r);
    }
    if (!reset) {
        r = ma8_log0(ctx, "migrate_legacy");
        if (ma8_is_raise(r)) goto e2e_fail;
        if (!ma8_truthy(r)) {
            json_t *lr = ma8_log0(ctx, "logger.warning");
            if (ma8_is_raise(lr)) { json_free(r); return lr; }
            json_free(lr);
        }
        json_free(r);
    }
    a = json_object();
    json_set(a, "joined_rooms", ma8_copy_or_null(json_obj_get(self, "_joined_rooms")));
    r = ma8_call(ctx, "CryptoStateStore", a);
    if (ma8_is_raise(r)) goto e2e_fail;
    json_free(r);
    r = ma8_log0(ctx, "OlmMachine");
    if (ma8_is_raise(r)) goto e2e_fail;
    json_free(r);
    r = ma8_log0(ctx, "olm.load");
    if (ma8_is_raise(r)) goto e2e_fail;
    json_free(r);
    r = ma8_log0(ctx, "verify_device_keys");
    if (ma8_is_raise(r)) goto e2e_fail;
    if (!ma8_truthy(r)) {
        json_free(r);
        return ma8_call(ctx, "_abort_connect", json_object());
    }
    json_free(r);
    r = ma8_log0(ctx, "olm.share_keys");
    if (ma8_is_raise(r)) {
        if (!ma8_exc_is_exception(r)) return r;
        const char *em = ma8_exc_msg(r);
        bool stale = em && strstr(em, "already exists");
        json_free(r);
        if (stale) {
            json_t *lr = ma8_log0(ctx, "logger.error");
            if (ma8_is_raise(lr)) return lr;
            json_free(lr);
            return ma8_call(ctx, "_abort_connect", json_object());
        }
        json_t *lr = ma8_log0(ctx, "logger.warning");
        if (ma8_is_raise(lr)) return lr;
        json_free(lr);
    } else {
        json_free(r);
    }
    r = ma8_log0(ctx, "verify_cross_signing");
    if (ma8_is_raise(r)) goto e2e_fail;
    json_free(r);
    r = ma8_log0(ctx, "logger.info");
    if (ma8_is_raise(r)) return r;
    json_free(r);
    return json_bool(true);

e2e_fail:
    if (!ma8_exc_is_exception(r)) return r;
    {
        json_t *a2 = json_object();
        ma8_set_str(a2, "phase", phase);
        json_free(r);
        return ma8_call(ctx, "e2ee_setup_failed", a2);
    }
}

/* PoP: ma8_download_external_media_with_cap @ plugins/platforms/matrix/adapter.py:_download_external_media_with_cap */
static json_t *ma8_download_external_media_with_cap(ma8_ctx_t *ctx, const json_t *args) {
    const char *url = ma8_str(json_obj_get(args, "url"));
    json_t *self = ctx->state;
    json_t *a = json_object();
    ma8_set_str(a, "url", url);
    json_t *r = ma8_call(ctx, "is_safe_url", a);
    if (ma8_is_raise(r)) return r;
    if (!ma8_truthy(r)) { json_free(r); return ma8_err("ValueError", "blocked unsafe media URL"); }
    json_free(r);

    a = json_object();
    ma8_set_str(a, "proxy_url", json_get_str(self, "_proxy_url", ""));
    r = ma8_call(ctx, "proxy_kwargs", a);
    if (ma8_is_raise(r)) return r;
    json_free(r);
    r = ma8_log0(ctx, "ClientSession");
    if (ma8_is_raise(r)) return r;
    json_free(r);

    char *fetch = ma8_strdup(url ? url : "");
    for (int hop = 0; hop < 20; hop++) {
        a = json_object();
        ma8_set_str(a, "url", fetch);
        json_set(a, "allow_redirects", json_bool(false));
        json_t *resp = ma8_call(ctx, "http.get", a);
        if (ma8_is_raise(resp)) { free(fetch); return resp; }
        long status = (long)ma8_attr_num(resp, "status");
        const char *loc = ma8_attr(resp, "location");
        const char *ct = ma8_attr(resp, "content_type");
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            if (!loc || !*loc) { json_free(resp); free(fetch); return ma8_err("ValueError", "redirect missing Location"); }
            char *next = NULL;
            if (loc[0] == '/') {
                const char *scheme = strstr(fetch, "://");
                const char *hostend = scheme ? strchr(scheme + 3, '/') : NULL;
                size_t plen = hostend ? (size_t)(hostend - fetch) : strlen(fetch);
                next = malloc(plen + strlen(loc) + 1);
                if (next) { memcpy(next, fetch, plen); strcpy(next + plen, loc); }
            } else {
                next = ma8_strdup(loc);
            }
            json_free(resp);
            free(fetch);
            fetch = next;
            a = json_object();
            ma8_set_str(a, "url", fetch);
            json_t *sr = ma8_call(ctx, "is_safe_url", a);
            if (ma8_is_raise(sr)) { free(fetch); return sr; }
            if (!ma8_truthy(sr)) { json_free(sr); free(fetch); return ma8_err("ValueError", "blocked unsafe redirect URL"); }
            json_free(sr);
            continue;
        }
        json_t *rr = ma8_log0(ctx, "raise_for_status");
        if (ma8_is_raise(rr)) { json_free(resp); free(fetch); return rr; }
        json_free(rr);
        a = json_object();
        json_set(a, "max_bytes", json_number((double)json_get_num(self, "_max_media_bytes", 0)));
        json_t *data = ma8_call(ctx, "read_body_capped", a);
        if (ma8_is_raise(data)) { json_free(resp); free(fetch); return data; }
        char *low = ma8_lower(ct);
        char *semi = low ? strchr(low, ';') : NULL;
        if (semi) *semi = '\0';
        char *norm = ma8_strip(low);
        free(low);
        if (!ma8_starts(norm, "image/")) {
            free(norm); json_free(data); json_free(resp); free(fetch);
            return ma8_err("ValueError", "external media is not an image");
        }
        const char *baseurl = ma8_basename(url ? url : "");
        const char *q = strchr(baseurl, '?');
        size_t blen = q ? (size_t)(q - baseurl) : strlen(baseurl);
        char *fname = malloc(blen + 1);
        if (fname) { memcpy(fname, baseurl, blen); fname[blen] = '\0'; }
        if (!fname || !*fname) { free(fname); fname = ma8_strdup("image.png"); }
        json_t *out = json_array();
        json_append(out, data);
        json_append(out, json_string(norm));
        json_append(out, json_string(fname));
        free(norm); free(fname); json_free(resp); free(fetch);
        return out;
    }
    free(fetch);
    return ma8_err("ValueError", "too many redirects");
}

/* PoP: ma8_handle_media_message @ plugins/platforms/matrix/adapter.py:_handle_media_message */
static json_t *ma8_handle_media_message(ma8_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const json_t *sc = json_obj_get(args, "source_content");
    const char *room = ma8_str(json_obj_get(args, "room_id"));
    const char *sender = ma8_str(json_obj_get(args, "sender"));
    const char *eid = ma8_str(json_obj_get(args, "event_id"));
    const char *msgtype = ma8_str(json_obj_get(args, "msgtype"));
    const char *body = ma8_str(sc ? json_obj_get(sc, "body") : NULL);
    const char *url = ma8_str(sc ? json_obj_get(sc, "url") : NULL);
    if (!body) body = "";
    if (url && *url && !ma8_starts(url, "mxc://")) {
        json_t *lr = ma8_log0(ctx, "logger.warning");
        if (ma8_is_raise(lr)) return lr;
        json_free(lr);
        return json_null();
    }
    const json_t *info = sc ? ma8_attr_node(sc, "info") : NULL;
    if (!info || info->type != JSON_OBJECT) info = NULL;
    const char *mime = info ? ma8_str(json_obj_get(info, "mimetype")) : NULL;
    long size = (long)ma8_num(info ? json_obj_get(info, "size") : NULL);
    if (size && size > (long)json_get_num(self, "_max_media_bytes", 0)) {
        json_t *lr = ma8_log0(ctx, "logger.warning");
        if (ma8_is_raise(lr)) return lr;
        json_free(lr);
        return json_null();
    }
    const json_t *fc = sc ? json_obj_get(sc, "file") : NULL;
    if ((!url || !*url) && fc && fc->type == JSON_OBJECT) {
        const char *fu = ma8_str(json_obj_get(fc, "url"));
        url = (fu && *fu) ? fu : "";
        if (url && *url && !ma8_starts(url, "mxc://")) {
            json_t *lr = ma8_log0(ctx, "logger.warning");
            if (ma8_is_raise(lr)) return lr;
            json_free(lr);
            return json_null();
        }
    }
    bool encrypted = fc && fc->type == JSON_OBJECT && ma8_str(json_obj_get(fc, "url")) != NULL;

    json_t *a = json_object();
    ma8_set_str(a, "msgtype", msgtype);
    ma8_set_str(a, "mimetype", mime ? mime : "");
    json_t *cls = ma8_call(ctx, "_classify_inbound_media", a);
    if (ma8_is_raise(cls)) return cls;
    char *msg_type = ma8_strdup("");
    char *media_type = ma8_strdup("");
    bool is_voice = false;
    if (cls->type == JSON_ARRAY && json_len(cls) >= 3) {
        free(msg_type); msg_type = ma8_strdup(ma8_str(json_get(cls, 0)));
        free(media_type); media_type = ma8_strdup(ma8_str(json_get(cls, 1)));
        is_voice = ma8_truthy(json_get(cls, 2));
    }
    json_free(cls);

    a = json_object();
    ma8_set_str(a, "room_id", room);
    ma8_set_str(a, "body", body);
    json_set(a, "mention_claimed", json_bool(json_get_bool(args, "mention_claimed", false)));
    json_t *pm = ma8_call(ctx, "_voice_may_park", a);
    if (ma8_is_raise(pm)) return pm;
    bool park = ma8_truthy(pm);
    json_free(pm);

    json_t *gate = NULL;
    if (park) {
        a = json_object();
        ma8_set_str(a, "room_id", room);
        ma8_set_str(a, "sender", sender);
        gate = ma8_call(ctx, "parked_voices.begin", a);
        if (ma8_is_raise(gate)) return gate;
    }
    a = json_object();
    ma8_set_str(a, "room_id", room);
    ma8_set_str(a, "sender", sender);
    ma8_set_str(a, "event_id", eid);
    ma8_set_str(a, "body", body);
    json_set(a, "mention_claimed", json_bool(json_get_bool(args, "mention_claimed", false)));
    json_set(a, "voice_gate", gate ? json_copy(gate) : json_null());
    json_t *ctxv = ma8_call(ctx, "_resolve_message_context", a);
    if (gate) {
        a = json_object();
        ma8_set_str(a, "room_id", room);
        ma8_set_str(a, "sender", sender);
        json_set(a, "gate", json_copy(gate));
        json_t *rel = ma8_call(ctx, "parked_voices.release", a);
        if (ma8_is_raise(rel)) { json_free(ctxv); json_free(gate); return rel; }
        json_free(rel);
        json_free(gate);
    }
    if (ma8_is_raise(ctxv)) return ctxv;
    if (ma8_is_null(ctxv)) { json_free(ctxv); return json_null(); }
    json_free(ctxv);

    const char *cached = NULL;
    json_t *cachedv = NULL;
    if (url && *url) {
        a = json_object();
        ma8_set_str(a, "url", url);
        ma8_set_str(a, "event_id", eid);
        ma8_set_str(a, "msg_type", msg_type);
        ma8_set_str(a, "media_type", media_type);
        json_set(a, "is_voice", json_bool(is_voice));
        ma8_set_str(a, "body", body);
        json_t *dl = ma8_call(ctx, "_download_and_cache_media", a);
        if (ma8_is_raise(dl)) {
            if (!ma8_exc_is_exception(dl)) return dl;
            json_free(dl);
            json_t *lr = ma8_log0(ctx, "logger.warning");
            if (ma8_is_raise(lr)) return lr;
            json_free(lr);
        } else {
            cachedv = dl;
            cached = ma8_str(dl);
        }
    }
    const char *http_url = "";
    if (url && *url && !encrypted) {
        a = json_object();
        ma8_set_str(a, "url", url);
        json_t *hu = ma8_call(ctx, "_mxc_to_http", a);
        if (ma8_is_raise(hu)) { json_free(cachedv); return hu; }
        http_url = ma8_str(hu);
        if (!http_url) http_url = "";
        /* keep hu alive only for its string; free after building event below */
        a = json_object();
        ma8_set_str(a, "room_id", room);
        ma8_set_str(a, "sender", sender);
        ma8_set_str(a, "event_id", eid);
        ma8_set_str(a, "body", body);
        ma8_set_str(a, "message_type", msg_type);
        ma8_set_str(a, "media_msgtype", msgtype);
        json_t *mev = ma8_call(ctx, "_build_inbound_event", a);
        json_free(hu);
        json_free(cachedv);
        if (ma8_is_raise(mev)) return mev;
        if (!ma8_is_null(mev)) {
            a = json_object();
            json_set(a, "event", ma8_copy_or_null(mev));
            json_t *hm = ma8_call(ctx, "handle_message", a);
            if (ma8_is_raise(hm)) { json_free(mev); return hm; }
            json_free(hm);
            json_free(mev);
        }
        return json_null();
    }
    a = json_object();
    ma8_set_str(a, "room_id", room);
    ma8_set_str(a, "sender", sender);
    ma8_set_str(a, "event_id", eid);
    ma8_set_str(a, "body", body);
    ma8_set_str(a, "message_type", msg_type);
    ma8_set_str(a, "media_msgtype", msgtype);
    json_t *mev = ma8_call(ctx, "_build_inbound_event", a);
    json_free(cachedv);
    if (ma8_is_raise(mev)) return mev;
    if (!ma8_is_null(mev)) {
        a = json_object();
        json_set(a, "event", ma8_copy_or_null(mev));
        json_t *hm = ma8_call(ctx, "handle_message", a);
        if (ma8_is_raise(hm)) { json_free(mev); return hm; }
        json_free(hm);
        json_free(mev);
    }
    (void)http_url; (void)cached;
    free(msg_type); free(media_type);
    return json_null();
}

/* PoP: ma8_schedule_reaction_redaction @ plugins/platforms/matrix/adapter.py:_schedule_reaction_redaction */
static json_t *ma8_schedule_reaction_redaction(ma8_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    json_t *a = json_object();
    ma8_set_str(a, "room_id", ma8_str(json_obj_get(args, "room_id")));
    ma8_set_str(a, "reaction_event_id", ma8_str(json_obj_get(args, "reaction_event_id")));
    { const char *rsn = ma8_str(json_obj_get(args, "reason")); ma8_set_str(a, "reason", rsn ? rsn : ""); }
    json_set(a, "delay", json_number((double)json_get_num(self, "_reaction_redaction_delay_seconds", 0)));
    json_t *r = ma8_call(ctx, "create_task", a);
    if (ma8_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: ma8_send_exec_approval_prompt @ plugins/platforms/matrix/adapter.py:_send_exec_approval_prompt */
static json_t *ma8_send_exec_approval_prompt(ma8_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const json_t *prompt = json_obj_get(args, "prompt");
    const json_t *choices = ma8_attr_node(prompt, "choices");
    bool smart = ma8_truthy(ma8_attr_node(prompt, "smart_denied"));
    const char *tier = "once";
    if (!smart) {
        if (ma8_arr_has_str(choices, "always")) tier = "always";
        else if (ma8_arr_has_str(choices, "session")) tier = "session";
    }
    ma8_buf_t hint = {0};
    bool has_once = ma8_arr_has_str(choices, "once");
    if (has_once) {
        const char *key = ma8_nested_str(self, "_EA_TYPED_HINT_KEYS", tier);
        json_t *a = json_object();
        ma8_set_str(a, "key", key);
        json_t *r = ma8_call(ctx, "t", a);
        if (ma8_is_raise(r)) { free(hint.b); return r; }
        mb_add(&hint, ma8_str(r));
        mb_add(&hint, "\n\n");
        json_free(r);
    }
    json_t *a = json_object();
    json_set_hdr(a, "key", "platform.matrix.approval.legend_intro");
    json_t *r = ma8_call(ctx, "t", a);
    if (ma8_is_raise(r)) { free(hint.b); return r; }
    mb_add(&hint, ma8_str(r));
    mb_add(&hint, "\n");
    json_free(r);
    json_t *emojis = json_array();
    bool first = true;
    if (choices && choices->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(choices); i++) {
            const char *c = ma8_str(json_get(choices, i));
            const char *k = ma8_nested_str(self, "_EA_LEGEND_KEYS", c);
            json_t *ta = json_object();
            ma8_set_str(ta, "key", k);
            json_t *tr = ma8_call(ctx, "t", ta);
            if (ma8_is_raise(tr)) { free(hint.b); json_free(emojis); return tr; }
            if (!first) mb_add(&hint, "\n");
            first = false;
            mb_add(&hint, ma8_str(tr));
            json_free(tr);
            const char *ej = ma8_nested_str(self, "_EA_REACTIONS", c);
            json_append(emojis, ej ? json_string(ej) : json_null());
        }
    }
    ma8_buf_t full = {0};
    mb_add(&full, ma8_attr(prompt, "text"));
    mb_add(&full, "\n\n");
    mb_addn(&full, hint.b, hint.n);
    free(hint.b);
    char *text = mb_take(&full);
    a = json_object();
    ma8_set_str(a, "chat_id", ma8_attr(prompt, "chat_id"));
    ma8_set_str(a, "text", text);
    json_set(a, "emojis", emojis);
    json_set_hdr(a, "label", "approval");
    free(text);
    return ma8_unwrap_attr(ma8_call(ctx, "send_reaction_prompt", a));
}

/* PoP: ma8_send_picker @ plugins/platforms/matrix/adapter.py:_send_picker */
static json_t *ma8_send_picker(ma8_ctx_t *ctx, const json_t *args) {
    ma8_buf_t b = {0};
    const json_t *lines = json_obj_get(args, "lines");
    if (lines && lines->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(lines); i++) {
            if (i) mb_add(&b, "\n");
            mb_add(&b, ma8_str(json_get(lines, i)));
        }
    }
    char *text = mb_take(&b);
    json_t *a = json_object();
    ma8_set_str(a, "chat_id", ma8_str(json_obj_get(args, "chat_id")));
    ma8_set_str(a, "text", text);
    json_set(a, "emojis", ma8_obj_keys(json_obj_get(args, "choices")));
    ma8_set_str(a, "label", ma8_str(json_obj_get(args, "label")));
    free(text);
    return ma8_unwrap_attr(ma8_call(ctx, "send_reaction_prompt", a));
}

/* PoP: ma8_send_local_file @ plugins/platforms/matrix/adapter.py:_send_local_file */
static json_t *ma8_send_local_file(ma8_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const char *fp = ma8_str(json_obj_get(args, "file_path"));
    const char *rid = ma8_str(json_obj_get(args, "room_id"));
    const char *mt = ma8_str(json_obj_get(args, "msgtype"));
    bool voice = json_get_bool(args, "is_voice", false);
    json_t *r = ma8_log0(ctx, "path.exists");
    if (ma8_is_raise(r)) return r;
    bool exists = ma8_truthy(r);
    json_free(r);
    if (!exists) {
        json_t *lr = ma8_log0(ctx, "logger.warning");
        if (ma8_is_raise(lr)) return lr;
        json_free(lr);
        json_t *a = json_object();
        json_set_hdr(a, "key", "platform.shared.media.attachment_failed");
        json_t *tr = ma8_call(ctx, "t", a);
        if (ma8_is_raise(tr)) return tr;
        a = json_object();
        ma8_set_str(a, "room_id", rid);
        ma8_set_str(a, "text", ma8_str(tr));
        json_free(tr);
        return ma8_unwrap_attr(ma8_call(ctx, "emit_media_warning", a));
    }
    r = ma8_log0(ctx, "path.stat");
    if (ma8_is_raise(r)) return r;
    double size = ma8_attr_num(r, "st_size");
    json_free(r);
    if (size > (double)json_get_num(self, "_max_media_bytes", 0)) {
        json_t *a = json_object();
        json_set(a, "size", json_number(size));
        return ma8_unwrap_attr(ma8_call(ctx, "_media_too_large", a));
    }
    const char *fn = ma8_str(json_obj_get(args, "file_name"));
    if (!fn || !*fn) fn = ma8_basename(fp);
    if (voice) {
        json_t *a = json_object();
        ma8_set_str(a, "path", fp);
        json_t *vm = ma8_call(ctx, "voice_metadata", a);
        if (ma8_is_raise(vm)) return vm;
        json_free(vm);
    }
    r = ma8_log0(ctx, "path.read_bytes");
    if (ma8_is_raise(r)) return r;
    json_free(r);
    json_t *a = json_object();
    ma8_set_str(a, "path", fn);
    json_t *gt = ma8_call(ctx, "guess_type", a);
    if (ma8_is_raise(gt)) return gt;
    const char *mraw = (gt->type == JSON_ARRAY && json_len(gt) >= 1) ? ma8_str(json_get(gt, 0)) : NULL;
    char *mime = ma8_strdup((mraw && *mraw) ? mraw : "application/octet-stream");
    json_free(gt);
    a = json_object();
    ma8_set_str(a, "room_id", rid);
    ma8_set_str(a, "fname", fn);
    ma8_set_str(a, "mime", mime);
    free(mime);
    ma8_set_str(a, "msgtype", mt);
    json_set(a, "is_voice", json_bool(voice));
    return ma8_unwrap_attr(ma8_call(ctx, "_upload_and_send", a));
}

/* PoP: ma8_send_reaction_prompt @ plugins/platforms/matrix/adapter.py:_send_reaction_prompt */
static json_t *ma8_send_reaction_prompt(ma8_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const json_t *meta = json_obj_get(args, "metadata");
    json_t *a = json_object();
    ma8_set_str(a, "chat_id", ma8_str(json_obj_get(args, "chat_id")));
    ma8_set_str(a, "text", ma8_str(json_obj_get(args, "text")));
    json_set(a, "metadata", ma8_copy_or_null(meta));
    json_t *res = ma8_call(ctx, "send", a);
    if (ma8_is_raise(res)) return res;
    bool ok = ma8_truthy(ma8_attr_node(res, "success"));
    const char *mid = ma8_attr(res, "message_id");
    if (!ok || !mid || !*mid) return ma8_unwrap_attr(res);
    const char *requester = meta ? ma8_str(json_obj_get(meta, "requester_user_id")) : NULL;
    json_t *tm = ma8_log0(ctx, "time.monotonic");
    if (ma8_is_raise(tm)) { json_free(res); return tm; }
    double expires = ma8_num(tm) + fmax(0.0, (double)json_get_num(self, "_approval_timeout_seconds", 0));
    json_free(tm);
    a = json_object();
    ma8_set_str(a, "message_id", mid);
    if (requester && *requester) ma8_set_str(a, "requester", requester);
    else json_set(a, "requester", json_null());
    json_set(a, "expires_at", json_number(expires));
    json_t *pr = ma8_call(ctx, "make_prompt", a);
    if (ma8_is_raise(pr)) { json_free(res); return pr; }
    json_free(pr);
    const json_t *emojis = json_obj_get(args, "emojis");
    if (emojis && emojis->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(emojis); i++) {
            a = json_object();
            ma8_set_str(a, "chat_id", ma8_str(json_obj_get(args, "chat_id")));
            ma8_set_str(a, "message_id", mid);
            ma8_set_str(a, "emoji", ma8_str(json_get(emojis, i)));
            json_t *rr = ma8_call(ctx, "_send_reaction", a);
            if (ma8_is_raise(rr)) {
                if (!ma8_exc_is_exception(rr)) { json_free(res); return rr; }
                json_free(rr);
                json_t *lr = ma8_log0(ctx, "logger.debug");
                if (ma8_is_raise(lr)) { json_free(res); return lr; }
                json_free(lr);
            } else {
                json_free(rr);
            }
        }
    }
    return ma8_unwrap_attr(res);
}

/* ---- markdown fallback: small focused subset matching the live transforms ---- */
static void ma8_md_escape(ma8_buf_t *out, const char *s) {
    if (!s) return;
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '&': mb_add(out, "&amp;"); break;
            case '<': mb_add(out, "&lt;"); break;
            case '>': mb_add(out, "&gt;"); break;
            case '"': mb_add(out, "&quot;"); break;
            case '\'': mb_add(out, "&#x27;"); break;
            default: mb_addc(out, *p); break;
        }
    }
}
/* Apply the live inline transforms (double-star/double-underscore strong,
 * single-star/single-underscore em, double-tilde del) on escaped text. */
static char *ma8_md_inline(const char *s) {
    ma8_buf_t out = {0};
    size_t n = strlen(s);
    size_t i = 0;
    while (i < n) {
        const char *pat = NULL; const char *op = NULL, *cl = NULL;
        bool star_strong = false;
        if (i + 1 < n && ((s[i] == '*' && s[i+1] == '*') || (s[i] == '_' && s[i+1] == '_'))) {
            pat = s[i] == '*' ? "**" : "__"; op = "<strong>"; cl = "</strong>";
            star_strong = (s[i] == '*');
        } else if (i + 1 < n && s[i] == '~' && s[i+1] == '~') {
            pat = "~~"; op = "<del>"; cl = "</del>";
        } else if (s[i] == '*' || s[i] == '_') {
            char ch = s[i];
            pat = (ch == '*') ? "*" : "_"; op = "<em>"; cl = "</em>";
        }
        bool matched = false;
        if (pat) {
            size_t plen = strlen(pat);
            const char *end = strstr(s + i + plen, pat);
            if (end && end > s + i + plen) {
                mb_add(&out, op);
                char *inner = (char*)malloc((size_t)(end - (s + i + plen)) + 1);
                if (inner) {
                    size_t il = (size_t)(end - (s + i + plen));
                    memcpy(inner, s + i + plen, il); inner[il] = '\0';
                    char *rec = ma8_md_inline(inner);
                    mb_add(&out, rec); free(rec); free(inner);
                }
                mb_add(&out, cl);
                i = (size_t)(end - s) + plen;
                matched = true;
            }
        }
        if (!matched && star_strong) {
            /* A "**" opener with no closing "**" is NOT swallowed as literal
             * stars: LIVE's later re.sub(r"\*(.+?)\*") sees the leading '*' and
             * turns "***x" into "<em>*</em>x". Fall back to the single-star em
             * transform at the same position, closing on the first later '*'
             * with at least one character in between. */
            const char *eend = strstr(s + i + 2, "*");
            if (eend) {
                mb_add(&out, "<em>");
                size_t il = (size_t)(eend - (s + i + 1));
                char *inner = (char *)malloc(il + 1);
                if (inner) {
                    memcpy(inner, s + i + 1, il); inner[il] = '\0';
                    char *rec = ma8_md_inline(inner);
                    mb_add(&out, rec); free(rec); free(inner);
                }
                mb_add(&out, "</em>");
                i = (size_t)(eend - s) + 1;
                matched = true;
            }
        }
        if (!matched) { mb_addc(&out, s[i]); i++; }
    }
    return mb_take(&out);
}
static bool ma8_md_is_hr(const char *line) {
    char c = 0; size_t cnt = 0;
    for (const char *p = line; *p; p++) {
        if (*p == ' ' || *p == '\t') continue;
        if (*p != '-' && *p != '*' && *p != '_') return false;
        if (!c) c = *p;
        if (*p != c) return false;
        cnt++;
    }
    return cnt >= 3;
}
static bool ma8_md_is_bq(const char *line) {
    return ma8_starts(line, "&gt; ") || ma8_starts(line, "> ") ||
           !strcmp(line, "&gt;") || !strcmp(line, ">");
}
static bool ma8_md_list(const char *line, bool *ordered, const char **item) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '-' || *p == '*' || *p == '+') {
        p++; if (*p != ' ' && *p != '\t') return false;
        while (*p == ' ' || *p == '\t') p++;
        *ordered = false; *item = p; return *p != '\0';
    }
    if (isdigit((unsigned char)*p)) {
        while (isdigit((unsigned char)*p)) p++;
        if (*p != '.' && *p != ')') return false;
        p++; if (*p != ' ' && *p != '\t') return false;
        while (*p == ' ' || *p == '\t') p++;
        *ordered = true; *item = p; return *p != '\0';
    }
    return false;
}

/* PoP: ma8_markdown_to_html_fallback @ plugins/platforms/matrix/adapter.py:_markdown_to_html_fallback */
static json_t *ma8_markdown_to_html_fallback(ma8_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *text = ma8_str(json_obj_get(args, "text"));
    if (!text) text = "";
    ma8_buf_t lines_out = {0};
    size_t n = strlen(text), start = 0;
    while (start <= n) {
        size_t end = start;
        while (end < n && text[end] != '\n') end++;
        size_t len = end - start;
        char *line = malloc(len + 1);
        if (line) { memcpy(line, text + start, len); line[len] = '\0'; }
        if (!line) line = ma8_strdup("");
        if (ma8_md_is_hr(line)) {
            mb_add(&lines_out, "<hr>");
        } else if (line[0] == '#') {
            size_t lv = 0;
            while (line[lv] == '#') lv++;
            if (lv >= 1 && lv <= 6 && isspace((unsigned char)line[lv])) {
                char *rest = ma8_strip(line + lv + 1);
                ma8_buf_t esc = {0}; ma8_md_escape(&esc, rest); char *escs = mb_take(&esc);
                char *inl = ma8_md_inline(escs);
                char tag[8];
                snprintf(tag, sizeof tag, "<h%zu>", lv);
                mb_add(&lines_out, tag);
                mb_add(&lines_out, inl);
                snprintf(tag, sizeof tag, "</h%zu>", lv);
                mb_add(&lines_out, tag);
                free(inl); free(escs); free(rest);
            } else {
                ma8_buf_t esc = {0}; ma8_md_escape(&esc, line); char *escs = mb_take(&esc);
                char *e = ma8_md_inline(escs); mb_add(&lines_out, e); free(e); free(escs);
            }
        } else if (ma8_md_is_bq(line)) {
            /* Collect consecutive blockquote lines. */
            ma8_buf_t parts = {0};
            bool firstb = true;
            size_t cur = start;
            while (cur <= n) {
                size_t e = cur; while (e < n && text[e] != '\n') e++;
                size_t l = e - cur;
                char *ln = malloc(l + 1);
                if (ln) { memcpy(ln, text + cur, l); ln[l] = '\0'; }
                if (!ln) ln = ma8_strdup("");
                if (!ma8_md_is_bq(ln)) { free(ln); break; }
                const char *content = ma8_starts(ln, "&gt; ") ? ln + 5 :
                                      (ma8_starts(ln, "> ") ? ln + 2 : "");
                if (!firstb) mb_add(&parts, "<br>");
                firstb = false;
                ma8_buf_t esc = {0}; ma8_md_escape(&esc, content); char *escs = mb_take(&esc);
                char *inl = ma8_md_inline(escs);
                mb_add(&parts, inl);
                free(inl); free(escs); free(ln);
                start = e;
                cur = e + 1;
                if (e == n) { start = n + 1; break; }
            }
            mb_add(&lines_out, "<blockquote>");
            mb_addn(&lines_out, parts.b, parts.n);
            mb_add(&lines_out, "</blockquote>");
            free(parts.b);
            free(line);
            if (start > n) break;
            continue;
        } else {
            bool ord = false; const char *item = NULL;
            if (ma8_md_list(line, &ord, &item)) {
                ma8_buf_t items = {0};
                bool firsti = true;
                size_t cur = start;
                while (cur <= n) {
                    size_t e = cur; while (e < n && text[e] != '\n') e++;
                    size_t l = e - cur;
                    char *ln = malloc(l + 1);
                    if (ln) { memcpy(ln, text + cur, l); ln[l] = '\0'; }
                    if (!ln) ln = ma8_strdup("");
                    bool o2 = false; const char *it2 = NULL;
                    if (!ma8_md_list(ln, &o2, &it2) || o2 != ord) { free(ln); break; }
                    if (!firsti) mb_add(&items, "</li><li>");
                    firsti = false;
                    ma8_buf_t esc = {0}; ma8_md_escape(&esc, it2); char *escs = mb_take(&esc);
                    char *inl = ma8_md_inline(escs);
                    mb_add(&items, inl);
                    free(inl); free(escs); free(ln);
                    start = e;
                    cur = e + 1;
                    if (e == n) { start = n + 1; break; }
                }
                mb_add(&lines_out, ord ? "<ol><li>" : "<ul><li>");
                mb_addn(&lines_out, items.b, items.n);
                mb_add(&lines_out, ord ? "</li></ol>" : "</li></ul>");
                free(items.b);
                free(line);
                if (start > n) break;
                continue;
            }
            ma8_buf_t esc = {0}; ma8_md_escape(&esc, line); char *escs = mb_take(&esc);
            char *e2 = ma8_md_inline(escs);
            mb_add(&lines_out, e2);
            free(e2); free(escs);
        }
        free(line);
        if (end == n) { start = n + 1; break; }
        mb_add(&lines_out, "\n");
        start = end + 1;
    }
    /* \n -> <br>\n, then drop <br>\n adjacent to block tags. */
    char *joined = mb_take(&lines_out);
    ma8_buf_t fin = {0};
    for (const char *p = joined; *p; p++) {
        if (*p == '\n') mb_add(&fin, "<br>\n");
        else mb_addc(&fin, *p);
    }
    free(joined);
    char *res = mb_take(&fin);
    /* Remove "<br>\n" immediately before a block open tag. */
    ma8_buf_t fin2 = {0};
    const char *p = res;
    while (*p) {
        if (!strncmp(p, "<br>\n", 5) && p[5] == '<' &&
            (!strncmp(p + 5, "</", 2) || !strncmp(p + 5, "<pre", 4) ||
             !strncmp(p + 5, "<blockquote", 11) || !strncmp(p + 5, "<h", 2) ||
             !strncmp(p + 5, "<ul", 3) || !strncmp(p + 5, "<ol", 3) ||
             !strncmp(p + 5, "<li", 3) || !strncmp(p + 5, "<hr", 3))) {
            p += 5;
            continue;
        }
        mb_addc(&fin2, *p++);
    }
    free(res);
    res = mb_take(&fin2);
    json_t *out = json_string(res);
    free(res);
    return out;
}

/* PoP: ma8_standalone_payloads @ plugins/platforms/matrix/adapter.py:_standalone_payloads */
static json_t *ma8_standalone_payloads(ma8_ctx_t *ctx, const json_t *args) {
    const char *msg = ma8_str(json_obj_get(args, "message"));
    json_t *a = json_object();
    ma8_set_str(a, "message", msg);
    json_set(a, "limit", json_number(4096));
    json_t *chunks = ma8_call(ctx, "truncate_message", a);
    if (ma8_is_raise(chunks)) return chunks;
    json_t *out = json_array();
    size_t nc = (chunks->type == JSON_ARRAY) ? json_len(chunks) : 0;
    for (size_t i = 0; i < nc; i++) {
        const char *chunk = ma8_str(json_get(chunks, i));
        a = json_object();
        ma8_set_str(a, "text", chunk);
        json_t *lt = ma8_call(ctx, "latex_to_tokens", a);
        if (ma8_is_raise(lt)) { json_free(chunks); json_free(out); return lt; }
        const char *tok = (lt->type == JSON_ARRAY && json_len(lt) >= 1) ? ma8_str(json_get(lt, 0)) : chunk;
        json_free(lt);
        a = json_object();
        ma8_set_str(a, "text", tok);
        json_t *ext = json_array();
        json_append(ext, json_string("fenced_code"));
        json_append(ext, json_string("tables"));
        json_set(a, "extensions", ext);
        json_t *md = ma8_call(ctx, "markdown", a);
        if (ma8_is_raise(md)) { json_free(chunks); json_free(out); return md; }
        const char *html = ma8_str(md);
        /* re.sub(r"<h[1-6]>(.*?)</h[1-6]>", r"<strong>\1</strong>"). */
        ma8_buf_t conv = {0};
        const char *hp = html ? html : "";
        while (*hp) {
            const char *open = strstr(hp, "<h");
            if (!open || open[2] < '1' || open[2] > '6' || open[3] != '>') {
                mb_addc(&conv, *hp++); continue;
            }
            char ctag[8]; snprintf(ctag, sizeof ctag, "</h%c>", open[2]);
            const char *cl = strstr(open + 4, ctag);
            if (!cl) { mb_addc(&conv, *hp++); continue; }
            mb_add(&conv, "<strong>");
            mb_addn(&conv, open + 4, (size_t)(cl - (open + 4)));
            mb_add(&conv, "</strong>");
            hp = cl + strlen(ctag);
        }
        char *h2 = mb_take(&conv);
        a = json_object();
        ma8_set_str(a, "html", h2);
        free(h2);
        json_t *san = ma8_call(ctx, "sanitize_html", a);
        if (ma8_is_raise(san)) { json_free(md); json_free(chunks); json_free(out); return san; }
        const char *sans = ma8_str(san);
        json_t *payload = json_object();
        json_set_hdr(payload, "msgtype", "m.text");
        ma8_set_str(payload, "body", chunk);
        json_set_hdr(payload, "format", "org.matrix.custom.html");
        ma8_set_str(payload, "formatted_body", sans);
        a = json_object();
        json_set(a, "payload", json_copy(payload));
        json_t *cb = ma8_call(ctx, "content_bytes", a);
        if (ma8_is_raise(cb)) { json_free(san); json_free(md); json_free(chunks); json_free(out); return cb; }
        bool fits = ma8_num(cb) <= 4096.0;
        json_free(cb);
        if (!fits) {
            json_t *plain = json_object();
            json_set_hdr(plain, "msgtype", "m.text");
            ma8_set_str(plain, "body", chunk);
            payload = plain;
        }
        json_append(out, payload);
        json_free(san); json_free(md);
    }
    json_free(chunks);
    return out;
}

/* PoP: ma8_standalone_send @ plugins/platforms/matrix/adapter.py:_standalone_send */
static json_t *ma8_standalone_send(ma8_ctx_t *ctx, const json_t *args) {
    const json_t *pc = json_obj_get(args, "pconfig");
    const json_t *extra = ma8_attr_node(pc, "extra");
    const char *chat = ma8_str(json_obj_get(args, "chat_id"));
    const char *msg = ma8_str(json_obj_get(args, "message"));
    const char *tok = ma8_attr(pc, "token");
    const char *hs = extra ? ma8_str(json_obj_get(extra, "homeserver")) : NULL;
    char *home = ma8_strdup(hs ? hs : "");
    char *token = ma8_strdup(tok ? tok : "");
    size_t hl = strlen(home);
    while (hl > 0 && home[hl - 1] == '/') home[--hl] = '\0';
    json_t *r = NULL;
    if (!*home) {
        json_t *a = json_object();
        json_set_hdr(a, "key", "MATRIX_HOMESERVER");
        json_set_hdr(a, "default", "");
        r = ma8_call(ctx, "get_secret", a);
        if (ma8_is_raise(r)) goto ss_fail;
        free(home); home = ma8_strdup(ma8_str(r));
        json_free(r); r = NULL;
        hl = strlen(home);
        while (hl > 0 && home[hl - 1] == '/') home[--hl] = '\0';
    }
    if (!*token) {
        json_t *a = json_object();
        json_set_hdr(a, "key", "MATRIX_ACCESS_TOKEN");
        json_set_hdr(a, "default", "");
        r = ma8_call(ctx, "get_secret", a);
        if (ma8_is_raise(r)) goto ss_fail;
        free(token); token = ma8_strdup(ma8_str(r));
        json_free(r); r = NULL;
    }
    if (!*home || !*token) {
        json_t *a = json_object();
        json_set_hdr(a, "message", "Matrix not configured (MATRIX_HOMESERVER, MATRIX_ACCESS_TOKEN required)");
        r = ma8_call(ctx, "send_error", a);
        free(home); free(token);
        return r;
    }
    free(home); free(token);
    r = ma8_log0(ctx, "ClientSession");
    if (ma8_is_raise(r)) goto ss_fail;
    json_free(r); r = NULL;
    json_t *a = json_object();
    ma8_set_str(a, "message", msg);
    json_t *payloads = ma8_call(ctx, "standalone_payloads", a);
    if (ma8_is_raise(payloads)) { r = payloads; goto ss_fail; }
    json_t *result = NULL;
    size_t np = (payloads && payloads->type == JSON_ARRAY) ? json_len(payloads) : 0;
    for (size_t i = 0; i < np; i++) {
        a = json_object();
        json_set(a, "payload", json_copy(json_get(payloads, i)));
        json_t *resp = ma8_call(ctx, "put", a);
        if (ma8_is_raise(resp)) {
            json_free(payloads);
            if (!strcmp(ma8_exc_kind(resp), "TimeoutError")) {
                json_free(resp);
                a = json_object();
                json_set_hdr(a, "message", "Matrix API timeout (30s)");
                return ma8_call(ctx, "send_error", a);
            }
            r = resp; goto ss_fail;
        }
        long status = (long)ma8_attr_num(resp, "status");
        if (status != 200 && status != 201) {
            json_t *tr = ma8_log0(ctx, "resp.text");
            if (ma8_is_raise(tr)) { json_free(resp); json_free(payloads); r = tr; goto ss_fail; }
            char em[512];
            snprintf(em, sizeof em, "Matrix API error (%ld): %s", status, ma8_str(tr));
            json_free(tr);
            a = json_object();
            ma8_set_str(a, "message", em);
            json_free(resp); json_free(payloads);
            return ma8_call(ctx, "send_error", a);
        }
        a = json_object();
        json_t *data = ma8_call(ctx, "resp.json", a);
        if (ma8_is_raise(data)) { json_free(resp); json_free(payloads); r = data; goto ss_fail; }
        const char *ev = (data && data->type == JSON_OBJECT) ? ma8_str(json_obj_get(data, "event_id")) : NULL;
        result = json_object();
        json_set(result, "success", json_bool(true));
        json_set_hdr(result, "platform", "matrix");
        ma8_set_str(result, "chat_id", chat);
        if (ev) ma8_set_str(result, "message_id", ev);
        else json_set(result, "message_id", json_null());
        json_free(data); json_free(resp);
    }
    json_free(payloads);
    if (!result) result = json_null();
    return result;

ss_fail:
    free(home); free(token);
    if (!ma8_exc_is_exception(r)) return r;
    {
        char em[768];
        snprintf(em, sizeof em, "Matrix send failed: %s", ma8_exc_msg(r));
        json_free(r);
        json_t *a = json_object();
        ma8_set_str(a, "message", em);
        return ma8_call(ctx, "send_error", a);
    }
}

/* PoP: ma8_write_recovery_key_output_file @ plugins/platforms/matrix/adapter.py:_write_matrix_recovery_key_output_file */
static json_t *ma8_write_recovery_key_output_file(ma8_ctx_t *ctx, const json_t *args) {
    const char *key = ma8_str(json_obj_get(args, "recovery_key"));
    json_t *p = ma8_call(ctx, "_recovery_key_output_path", json_object());
    if (ma8_is_raise(p)) return p;
    if (ma8_is_null(p)) return p;
    const char *path = ma8_str(p);
    json_t *a = json_object();
    json_set(a, "parents", json_bool(true));
    json_set(a, "exist_ok", json_bool(true));
    json_t *mr = ma8_call(ctx, "path.parent.mkdir", a);
    if (ma8_is_raise(mr)) { json_free(p); return mr; }
    json_free(mr);
    a = json_object();
    ma8_set_str(a, "path", path);
    json_set(a, "flags", json_number(7));
    json_set(a, "mode", json_number(384));
    json_t *fd = ma8_call(ctx, "os.open", a);
    if (ma8_is_raise(fd)) { json_free(p); return fd; }
    int fdval = (int)ma8_num(fd);
    json_free(fd);
    a = json_object();
    ma8_set_str(a, "data", key);
    json_t *w = ma8_call(ctx, "file.write", a);
    if (ma8_is_raise(w)) {
        if (!ma8_exc_is_exception(w)) { json_free(p); return w; }
        a = json_object();
        json_set(a, "fd", json_number(fdval));
        json_t *c = ma8_call(ctx, "os.close", a);
        if (ma8_is_raise(c)) { json_free(p); return c; }
        json_free(c); json_free(p);
        return w;
    }
    json_free(w);
    a = json_object();
    json_set_hdr(a, "data", "\n");
    w = ma8_call(ctx, "file.write", a);
    if (ma8_is_raise(w)) {
        if (!ma8_exc_is_exception(w)) { json_free(p); return w; }
        a = json_object();
        json_set(a, "fd", json_number(fdval));
        json_t *c = ma8_call(ctx, "os.close", a);
        if (ma8_is_raise(c)) { json_free(p); return c; }
        json_free(c); json_free(p);
        return w;
    }
    json_free(w);
    return p;
}

/* PoP: ma8_create_handoff_thread @ plugins/platforms/matrix/adapter.py:create_handoff_thread */
static json_t *ma8_create_handoff_thread(ma8_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    if (!ma8_obj_truthy(json_obj_get(self, "_client"))) return json_null();
    const char *name = ma8_str(json_obj_get(args, "name"));
    char *txt = ma8_strip(name);
    if (!txt || !*txt) {
        json_t *a = json_object();
        json_set_hdr(a, "key", "platform.matrix.handoff.default_name");
        json_t *tr = ma8_call(ctx, "t", a);
        if (ma8_is_raise(tr)) { free(txt); return tr; }
        free(txt); txt = ma8_strdup(ma8_str(tr));
        json_free(tr);
    }
    json_t *a = json_object();
    ma8_set_str(a, "chat_id", ma8_str(json_obj_get(args, "parent_chat_id")));
    ma8_set_str(a, "text", txt);
    json_set(a, "metadata", json_null());
    free(txt);
    json_t *res = ma8_call(ctx, "send", a);
    if (ma8_is_raise(res)) return res;
    bool ok = ma8_truthy(ma8_attr_node(res, "success"));
    const char *mid = ma8_attr(res, "message_id");
    if (!ok || !mid || !*mid) { json_free(res); return json_null(); }
    a = json_object();
    ma8_set_str(a, "thread_id", mid);
    json_t *mr = ma8_call(ctx, "mark_async", a);
    if (ma8_is_raise(mr)) { json_free(res); return mr; }
    json_free(mr);
    json_t *out = json_string(mid);
    json_free(res);
    return out;
}
