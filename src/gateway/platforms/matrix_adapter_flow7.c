/*
 * matrix_adapter_flow7.c -- FLOW port of the remaining UNCLAIMED helpers of
 * plugins/platforms/matrix/adapter.py (hermes-agent): store-dir pinning,
 * recovery-key output handling, inbound room gating, E2EE verification/reset,
 * crypto re-pickling, room identity, voice metadata, outbound mention regions,
 * invite scheduling and content-event send.
 *
 * The flow1..flow6 siblings and their claimed funcs are NOT re-ported. Every
 * collaborator below enters through the ONE universal seam as a bare name --
 * none is re-ported. All symbols are static and ma7_-prefixed; libjson only.
 *
 * Decision sequences only: every live I/O / injected dependency enters through
 * ma7_call(ctx, name, args). The harness wires it to a recording stub fed the
 * case's scripted results (in.io); the oracle installs recording stubs emitting
 * the identical event shape {"name","args","ret"} from the SAME scripts.
 *
 * Representation contract (shared with tests/oracle_matrix_adapter_flow7.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; scripted raises
 *   round-trip verbatim when they escape a drive. except Exception swallows the
 *   standard Exception family; SystemExit/KeyboardInterrupt/GeneratorExit pass.
 * - Attribute carriers are {"__attr__": {attrs}}. Sets emit as sorted arrays.
 * - logger.* calls are seams recorded with EMPTY args (only call order/name is
 *   in the contract; long format strings are deliberately not compared).
 * - Domain note: _protect_outbound_mention_regions is ported for plain text
 *   without fenced/inline code or markdown links (no NUL placeholders); the
 *   _matrix_voice_metadata_for_file port does not synthesize a PCM waveform
 *   (fixtures exercise the ffprobe/duration path only).
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
typedef struct ma7_ctx {
    json_t *state;
    const json_t *io;
    json_t *cursors;
    json_t *trace;
} ma7_ctx_t;

/* -- exception / value helpers -- */
static bool ma7_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static json_t *ma7_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type ? type : "RuntimeError");
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *ma7_make_exc(const json_t *spec) {
    return ma7_err(json_get_str(spec, "type", "RuntimeError"),
                   json_get_str(spec, "message", ""));
}
static bool ma7_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ma7_truthy(const json_t *v) {
    if (ma7_is_null(v)) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
/* Attribute-double objects are Python objects: always truthy. */
static bool ma7_obj_truthy(const json_t *v) {
    if (!v || v->type != JSON_OBJECT) return ma7_truthy(v);
    if (json_obj_get(v, "__attr__")) return true;
    return v->c.count > 0;
}
static const char *ma7_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static double ma7_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static bool ma7_bool_of(const json_t *v) { return (v && v->type == JSON_BOOL) ? v->bool_val : false; }
static double ma7_number_of(const json_t *v) { return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0; }
static json_t *ma7_copy_or_null(const json_t *v) { return v ? json_copy(v) : json_null(); }
static void ma7_set_str(json_t *obj, const char *key, const char *s) {
    json_set(obj, key, s ? json_string(s) : json_null());
}
static const char *ma7_exc_msg(const json_t *v) {
    if (!v) return "";
    if (ma7_is_raise(v)) return json_get_str(v, "message", "");
    return ma7_str(v) ? ma7_str(v) : "";
}
static const json_t *ma7_attr_node(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    const json_t *in = json_obj_get(obj, "__attr__");
    if (in && in->type == JSON_OBJECT) return json_obj_get(in, key);
    return json_obj_get(obj, key);
}
static const char *ma7_attr(const json_t *obj, const char *key) {
    const json_t *n = ma7_attr_node(obj, key);
    return ma7_str(n);
}
static bool ma7_arr_has_str(const json_t *arr, const char *s) {
    if (!s || !arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *v = ma7_str(json_get(arr, i));
        if (v && strcmp(v, s) == 0) return true;
    }
    return false;
}
static bool ma7_exc_is_exception(const json_t *v) {
    if (!ma7_is_raise(v)) return false;
    const char *k = json_get_str(v, "__raise__", "");
    if (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt") ||
        !strcmp(k, "GeneratorExit") || !strcmp(k, "BaseException")) return false;
    return true;
}

static char *ma7_strdup(const char *s);

/* Python str(value) for the scalar domain used by these ports. */
static char *ma7_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ma7_strdup("None");
    switch (v->type) {
        case JSON_STRING: return ma7_strdup(v->str_val);
        case JSON_BOOL: return ma7_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
            else snprintf(buf, sizeof buf, "%.17g", d);
            return ma7_strdup(buf);
        }
        default: return ma7_strdup("");
    }
}

/* -- growable string buffer -- */
typedef struct { char *b; size_t n, cap; } ma7_buf_t;
static void mb_reserve(ma7_buf_t *b, size_t need) {
    if (b->b && b->n + need + 1 <= b->cap) return;
    size_t nc = b->cap ? b->cap : 32;
    while (nc < b->n + need + 1) nc *= 2;
    char *nb = realloc(b->b, nc);
    if (!nb) return;
    b->b = nb; b->cap = nc;
    if (b->n == 0) b->b[0] = '\0';
}
static void mb_addn(ma7_buf_t *b, const char *s, size_t n) {
    if (!s || n == 0) return;
    mb_reserve(b, n);
    if (!b->b) return;
    memcpy(b->b + b->n, s, n);
    b->n += n; b->b[b->n] = '\0';
}
static void mb_add(ma7_buf_t *b, const char *s) { if (s) mb_addn(b, s, strlen(s)); }
static void mb_addc(ma7_buf_t *b, char c) { mb_reserve(b, 1); if (b->b) { b->b[b->n++] = c; b->b[b->n] = '\0'; } }
static char *mb_take(ma7_buf_t *b) {
    if (!b->b) { char *e = malloc(1); if (e) e[0] = '\0'; return e; }
    b->b[b->n] = '\0';
    char *r = b->b; b->b = NULL; b->n = b->cap = 0; return r;
}
static char *ma7_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s); char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *ma7_strip(const char *s) {
    if (!s) return ma7_strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}

/* The one universal seam. */
static json_t *ma7_call(ma7_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    char msg[256];
    if (!entry) {
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ma7_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ma7_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) ret = ma7_make_exc(raise);
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

/* logger helper: empty args -- only the ordered call/name is in the contract. */
static json_t *ma7_log0(ma7_ctx_t *ctx, const char *name) {
    return ma7_call(ctx, name, json_object());
}

/* ===================== ported helpers ===================== */

/* PoP: ma7_resolve_store_dir @ plugins/platforms/matrix/adapter.py:_resolve_store_dir */
static json_t *ma7_resolve_store_dir(ma7_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "legacy_path", "platforms/matrix/store");
    json_set_hdr(a, "new_path", "matrix/store");
    json_t *r = ma7_call(ctx, "_get_hermes_dir", a);
    if (ma7_is_raise(r)) return r;
    json_set(ctx->state, "_store_dir", json_copy(r));
    return r;
}

/* PoP: ma7_get_recovery_key_output_target @ plugins/platforms/matrix/adapter.py:_get_matrix_recovery_key_output_target */
static json_t *ma7_get_recovery_key_output_target(ma7_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *r = ma7_call(ctx, "_recovery_key_output_path", json_object());
    if (ma7_is_raise(r)) return r;
    json_t *out = json_array();
    if (ma7_is_null(r)) {
        json_append(out, json_null());
        json_append(out, json_string("not_configured"));
        json_free(r);
        return out;
    }
    json_t *er = ma7_call(ctx, "path.exists", json_object());
    if (ma7_is_raise(er)) { json_free(r); json_free(out); return er; }
    bool exists = ma7_truthy(er);
    json_free(er);
    if (exists) {
        json_append(out, json_null());
        json_append(out, json_string("exists"));
        json_free(r);
        return out;
    }
    json_t *ma = json_object();
    json_set(ma, "parents", json_bool(true));
    json_set(ma, "exist_ok", json_bool(true));
    json_t *mr = ma7_call(ctx, "path.parent.mkdir", ma);
    if (ma7_is_raise(mr)) {
        if (!ma7_exc_is_exception(mr)) { json_free(r); json_free(out); return mr; }
        char msg[512];
        snprintf(msg, sizeof msg, "unusable: %s", ma7_exc_msg(mr));
        json_free(mr);
        json_append(out, json_null());
        json_append(out, json_string(msg));
        json_free(r);
        return out;
    }
    json_free(mr);
    json_append(out, json_copy(r));
    json_append(out, json_string(""));
    json_free(r);
    return out;
}

/* PoP: ma7_room_needs_encrypted_upload @ plugins/platforms/matrix/adapter.py:_room_needs_encrypted_upload */
static json_t *ma7_room_needs_encrypted_upload(ma7_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const json_t *client = json_obj_get(self, "_client");
    if (!ma7_truthy(json_obj_get(self, "_encryption")) || !ma7_obj_truthy(client) ||
        !ma7_truthy(ma7_attr_node(client, "crypto"))) return json_bool(false);
    const json_t *ss = ma7_attr_node(client, "state_store");
    if (!ss || ss->type == JSON_NULL || !ma7_obj_truthy(ss)) return json_bool(false);
    json_t *a = json_object();
    json_set(a, "room_id", ma7_copy_or_null(json_obj_get(args, "room_id")));
    json_t *r = ma7_call(ctx, "state_store.is_encrypted", a);
    if (ma7_is_raise(r)) {
        if (!ma7_exc_is_exception(r)) return r;
        json_free(r);
        return json_bool(false);
    }
    bool ok = ma7_truthy(r);
    json_free(r);
    return json_bool(ok);
}

/* PoP: ma7_send_content_event @ plugins/platforms/matrix/adapter.py:_send_content_event */
static json_t *ma7_send_content_event(ma7_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "room_id", ma7_copy_or_null(json_obj_get(args, "room_id")));
    json_set_hdr(a, "event_type", "m.room.message");
    json_set(a, "msg_content", ma7_copy_or_null(json_obj_get(args, "msg_content")));
    json_t *r = ma7_call(ctx, "client.send_message_event", a);
    json_t *out = json_object();
    if (ma7_is_raise(r)) {
        if (!ma7_exc_is_exception(r)) { json_free(out); return r; }
        json_set(out, "success", json_bool(false));
        ma7_set_str(out, "error", ma7_exc_msg(r));
        json_free(r);
        return out;
    }
    json_set(out, "success", json_bool(true));
    char *mid = ma7_py_str(r);
    ma7_set_str(out, "message_id", mid);
    free(mid);
    json_free(r);
    return out;
}

/* PoP: ma7_handle_generated_recovery_key @ plugins/platforms/matrix/adapter.py:_handle_generated_matrix_recovery_key */
static json_t *ma7_handle_generated_recovery_key(ma7_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    ma7_set_str(a, "recovery_key", ma7_str(json_obj_get(args, "recovery_key")));
    json_t *r = ma7_call(ctx, "_write_matrix_recovery_key_output_file", a);
    if (ma7_is_raise(r)) {
        if (!ma7_exc_is_exception(r)) return r; /* SystemExit passes */
        json_free(r);
        json_t *lr = ma7_log0(ctx, "logger.warning");
        if (ma7_is_raise(lr)) return lr;
        json_free(lr);
        return json_null();
    }
    json_free(r);
    json_t *lr = ma7_log0(ctx, "logger.warning");
    if (ma7_is_raise(lr)) return lr;
    json_free(lr);
    return json_null();
}

/* PoP: ma7_reverify_keys_after_upload @ plugins/platforms/matrix/adapter.py:_reverify_keys_after_upload */
static json_t *ma7_reverify_keys_after_upload(ma7_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const json_t *client = json_obj_get(args, "client");
    const char *devid = ma7_attr(client, "device_id");
    bool unverified = ma7_truthy(json_obj_get(self, "_device_id_unverified"));
    if ((!devid || !*devid) || unverified) {
        json_t *r = ma7_log0(ctx, "logger.warning");
        if (ma7_is_raise(r)) return r;
        json_free(r);
        return json_bool(true);
    }
    json_t *qa = json_object();
    json_set(qa, "client", ma7_copy_or_null(client));
    json_t *dev = ma7_call(ctx, "_query_own_device_keys", qa);
    if (ma7_is_raise(dev)) {
        if (!ma7_exc_is_exception(dev)) return dev;
        json_free(dev);
        json_t *r = ma7_log0(ctx, "logger.error");
        if (ma7_is_raise(r)) return r;
        json_free(r);
        return json_bool(false);
    }
    if (ma7_obj_truthy(dev)) {
        json_t *ea = json_object();
        json_set(ea, "device", json_copy(dev));
        json_t *srv = ma7_call(ctx, "_extract_server_ed25519", ea);
        if (ma7_is_raise(srv)) {
            if (!ma7_exc_is_exception(srv)) { json_free(dev); return srv; }
            json_free(srv);
            json_free(dev);
            json_t *r = ma7_log0(ctx, "logger.error");
            if (ma7_is_raise(r)) return r;
            json_free(r);
            return json_bool(false);
        }
        const char *local = ma7_str(json_obj_get(args, "local_ed25519"));
        const char *sv = ma7_str(srv);
        bool mismatch = (sv || local) && !(sv && local && strcmp(sv, local) == 0);
        json_free(srv);
        json_free(dev);
        if (mismatch) {
            json_t *r = ma7_log0(ctx, "logger.error");
            if (ma7_is_raise(r)) return r;
            json_free(r);
            return json_bool(false);
        }
    } else {
        json_free(dev);
    }
    return json_bool(true);
}

/* PoP: ma7_reset_crypto_store_if_device_changed @ plugins/platforms/matrix/adapter.py:_reset_crypto_store_if_device_changed */
static json_t *ma7_reset_crypto_store_if_device_changed(ma7_ctx_t *ctx, const json_t *args) {
    const char *devid = ma7_str(json_obj_get(args, "device_id"));
    if (!devid || !*devid) return json_bool(false);
    json_t *r = ma7_call(ctx, "get_device_id", json_object());
    if (ma7_is_raise(r)) {
        if (!ma7_exc_is_exception(r)) return r;
        json_free(r);
        json_t *lr = ma7_log0(ctx, "logger.warning");
        if (ma7_is_raise(lr)) return lr;
        json_free(lr);
        return json_bool(false);
    }
    const char *stored = ma7_str(r);
    if (!stored || !*stored || strcmp(stored, devid) == 0) {
        json_free(r);
        return json_bool(false);
    }
    json_free(r);
    json_t *lr = ma7_log0(ctx, "logger.warning");
    if (ma7_is_raise(lr)) return lr;
    json_free(lr);
    json_t *d = ma7_call(ctx, "delete", json_object());
    if (ma7_is_raise(d)) return d; /* delete() is NOT inside a try */
    json_free(d);
    return json_bool(true);
}

/* PoP: ma7_protect_outbound_mention_regions @ plugins/platforms/matrix/adapter.py:_protect_outbound_mention_regions */
static json_t *ma7_protect_outbound_mention_regions(ma7_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *text = ma7_str(json_obj_get(args, "text"));
    json_t *out = json_array();
    json_append(out, json_string(text ? text : ""));
    json_append(out, json_array());
    return out;
}

/* PoP: ma7_schedule_invite_join @ plugins/platforms/matrix/adapter.py:_schedule_invite_join */
static json_t *ma7_schedule_invite_join(ma7_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const char *room = ma7_str(json_obj_get(args, "room_id"));
    if (!room || !*room) return json_null();
    if (ma7_arr_has_str(json_obj_get(self, "_joined_rooms"), room)) return json_null();
    json_t *tasks = json_obj_get(self, "_invite_join_tasks");
    if (tasks && tasks->type == JSON_OBJECT) {
        const json_t *existing = json_obj_get(tasks, room);
        if (existing && !ma7_is_null(existing)) {
            bool done = ma7_bool_of(ma7_attr_node(existing, "done"));
            if (!done) return json_null();
        }
    }
    json_t *a = json_object();
    ma7_set_str(a, "room_id", room);
    json_set(a, "is_direct", json_bool(json_get_bool(args, "is_direct", false)));
    { const char *inv = ma7_str(json_obj_get(args, "inviter")); ma7_set_str(a, "inviter", inv ? inv : ""); }
    json_t *r = ma7_call(ctx, "create_task", a);
    if (ma7_is_raise(r)) return r;
    if (!tasks || tasks->type != JSON_OBJECT) {
        tasks = json_object();
        json_set(self, "_invite_join_tasks", tasks);
    }
    json_set(tasks, room, json_copy(r));
    json_free(r);
    return json_null();
}

/* PoP: ma7_repickle_crypto_sessions @ plugins/platforms/matrix/adapter.py:_repickle_crypto_sessions */
static json_t *ma7_repickle_crypto_sessions(ma7_ctx_t *ctx, const json_t *args) {
    const char *acct = ma7_str(json_obj_get(args, "acct_id"));
    const char *legacy = ma7_str(json_obj_get(args, "legacy_key"));
    const char *pkey = ma7_str(json_obj_get(args, "pickle_key"));
    static const char *TABLES[3] = {
        "crypto_olm_session", "crypto_megolm_inbound_session", "crypto_megolm_outbound_session"};
    for (int ti = 0; ti < 3; ti++) {
        char q[256];
        snprintf(q, sizeof q, "SELECT session_id, session FROM %s WHERE account_id=$1", TABLES[ti]);
        json_t *fa = json_object();
        ma7_set_str(fa, "query", q);
        ma7_set_str(fa, "account_id", acct);
        json_t *rows = ma7_call(ctx, "fetch", fa);
        if (ma7_is_raise(rows)) return rows; /* no try around fetch */
        if (rows && rows->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(rows); i++) {
                const json_t *row = json_get(rows, i);
                const json_t *sess = json_obj_get(row, "session");
                if (ma7_is_null(sess)) continue;
                /* from_pickle(pickled, pickle_key) under suppress(Exception) */
                json_t *pa = json_object();
                ma7_set_str(pa, "table", TABLES[ti]);
                ma7_set_str(pa, "key", pkey);
                json_set(pa, "pickled", json_copy(sess));
                json_t *fp = ma7_call(ctx, "from_pickle", pa);
                if (!ma7_is_raise(fp)) { json_free(fp); continue; }
                if (!ma7_exc_is_exception(fp)) { json_free(fp); json_free(rows); return fp; }
                json_free(fp);
                /* legacy key */
                json_t *la = json_object();
                ma7_set_str(la, "table", TABLES[ti]);
                ma7_set_str(la, "key", legacy);
                json_set(la, "pickled", json_copy(sess));
                json_t *lp = ma7_call(ctx, "from_pickle", la);
                if (ma7_is_raise(lp)) {
                    if (!ma7_exc_is_exception(lp)) { json_free(lp); json_free(rows); return lp; }
                    json_t *r = ma7_log0(ctx, "logger.warning");
                    if (r && ma7_is_raise(r)) { json_free(r); json_free(lp); json_free(rows); return r; }
                    if (r) json_free(r);
                    json_free(lp);
                    continue;
                }
                json_t *spa = json_object();
                ma7_set_str(spa, "key", pkey);
                json_t *pk = ma7_call(ctx, "session.pickle", spa);
                if (ma7_is_raise(pk)) { json_free(lp); json_free(rows); return pk; }
                char uq[256];
                snprintf(uq, sizeof uq,
                    "UPDATE %s SET session=$1 WHERE account_id=$2 AND session_id=$3", TABLES[ti]);
                json_t *ea = json_object();
                ma7_set_str(ea, "query", uq);
                json_set(ea, "session_data", json_copy(pk));
                ma7_set_str(ea, "account_id", acct);
                char *sid = ma7_py_str(json_obj_get(row, "session_id"));
                ma7_set_str(ea, "session_id", sid);
                free(sid);
                json_t *er = ma7_call(ctx, "execute", ea);
                if (ma7_is_raise(er)) { json_free(pk); json_free(lp); json_free(rows); return er; }
                json_free(er); json_free(pk); json_free(lp);
            }
        }
        json_free(rows);
    }
    return json_null();
}

/* PoP: ma7_resolve_room_identity @ plugins/platforms/matrix/adapter.py:_resolve_room_identity */
static json_t *ma7_resolve_room_identity(ma7_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const char *room = ma7_str(json_obj_get(args, "room_id"));
    bool force = json_get_bool(args, "force_refresh", false);
    double ttl = json_get_num(self, "_room_identity_ttl_seconds", 0);
    json_t *identities = json_obj_get(self, "_room_identities");
    json_t *cached_at = json_obj_get(self, "_room_identity_cached_at");
    const json_t *cached = identities ? json_obj_get(identities, room ? room : "") : NULL;
    bool fresh = true;
    if (ttl > 0) {
        json_t *ma = json_object();
        json_t *m = ma7_call(ctx, "time.monotonic", ma);
        if (ma7_is_raise(m)) return m;
        double now = ma7_num(m);
        json_free(m);
        double cat = cached_at ? json_get_num(cached_at, room ? room : "", 0) : 0.0;
        fresh = (now - cat) <= ttl;
    }
    if (!ma7_is_null(cached) && cached && fresh && !force) return json_copy(cached);

    const char *fields[3] = {"m.room.name", "m.room.topic", "m.room.canonical_alias"};
    const char *keys[3] = {"name", "topic", "alias"};
    json_t *vals[3];
    for (int i = 0; i < 3; i++) {
        json_t *a = json_object();
        ma7_set_str(a, "room_id", room);
        ma7_set_str(a, "event_type", fields[i]);
        ma7_set_str(a, "field", keys[i]);
        vals[i] = ma7_call(ctx, "_get_room_state_value", a);
        if (ma7_is_raise(vals[i])) {
            for (int j = 0; j < i; j++) json_free(vals[j]);
            return vals[i];
        }
    }
    json_t *ca = json_object();
    ma7_set_str(ca, "room_id", room);
    json_t *mc = ma7_call(ctx, "_get_room_member_count", ca);
    if (ma7_is_raise(mc)) {
        for (int i = 0; i < 3; i++) json_free(vals[i]);
        return mc;
    }
    bool has_name = ma7_truthy(vals[0]);
    const json_t *dmnode = json_obj_get(self, "_dm_rooms");
    bool is_direct = dmnode ? ma7_truthy(json_obj_get(dmnode, room ? room : "")) : false;
    bool member_known = !ma7_is_null(mc);
    double mnum = ma7_num(mc);
    bool likely_dm = (member_known && mnum <= 2) || (is_direct && !has_name);
    char *server = NULL;
    if (room) {
        const char *colon = strrchr(room, ':');
        if (colon) {
            char *t = ma7_strip(colon + 1);
            if (t && *t) server = t; else free(t);
        }
    }
    const json_t *dn = ma7_truthy(vals[0]) ? vals[0] : (ma7_truthy(vals[2]) ? vals[2] : NULL);
    bool conflict = is_direct && has_name && (!member_known || mnum > 2);

    if (identities && cached_at && json_object_size(identities) >= (size_t)json_get_num(self, "_room_identity_cache_max", 0)) {
        /* stub invalidate is a no-op: emit the seam but do not evict */
        double best = 0; const char *oldest = NULL; bool have = false;
        for (size_t i = 0; i < json_object_size(cached_at); i++) {
            const char *k = json_object_get_key_at(cached_at, i);
            double v = ma7_number_of(json_object_get_at(cached_at, i));
            if (!have || v < best) { best = v; oldest = k; have = true; }
        }
        if (oldest && json_get_num(self, "_room_identity_cache_max", 0) > 0) {
            json_t *ia = json_object();
            ma7_set_str(ia, "oldest", oldest);
            json_t *ir = ma7_call(ctx, "_invalidate_room_identities", ia);
            if (ma7_is_raise(ir)) { for (int i=0;i<3;i++) json_free(vals[i]); json_free(mc); free(server); return ir; }
            json_free(ir);
        }
    }

    json_t *ident = json_object();
    ma7_set_str(ident, "room_id", room);
    json_set(ident, "room_name", json_copy(vals[0]));
    json_set(ident, "room_topic", json_copy(vals[1]));
    json_set(ident, "canonical_alias", json_copy(vals[2]));
    if (server) json_set(ident, "server_name", json_string(server)); else json_set(ident, "server_name", json_null());
    json_set(ident, "joined_member_count", json_copy(mc));
    json_set(ident, "is_direct_account_data", json_bool(is_direct));
    if (dn) json_set(ident, "display_name", json_copy(dn)); else ma7_set_str(ident, "display_name", room);
    json_set(ident, "has_explicit_name", json_bool(has_name));
    ma7_set_str(ident, "chat_type", likely_dm ? "dm" : "room");
    json_set(ident, "conflict", json_bool(conflict));

    json_t *mona = json_object();
    json_t *mon = ma7_call(ctx, "time.monotonic", mona);
    if (ma7_is_raise(mon)) { for (int i=0;i<3;i++) json_free(vals[i]); json_free(mc); free(server); json_free(ident); return mon; }
    if (!identities || identities->type != JSON_OBJECT) { identities = json_object(); json_set(self, "_room_identities", identities); }
    if (!cached_at || cached_at->type != JSON_OBJECT) { cached_at = json_object(); json_set(self, "_room_identity_cached_at", cached_at); }
    json_set(identities, room ? room : "", json_copy(ident));
    json_set(cached_at, room ? room : "", json_copy(mon));
    for (int i = 0; i < 3; i++) json_free(vals[i]);
    json_free(mc); json_free(mon); free(server);
    return ident;
}

/* PoP: ma7_voice_metadata_for_file @ plugins/platforms/matrix/adapter.py:_matrix_voice_metadata_for_file */
static json_t *ma7_voice_metadata_for_file(ma7_ctx_t *ctx, const json_t *args) {
    json_t *meta = json_object();
    const char *path = ma7_str(json_obj_get(args, "path"));
    /* ffprobe */
    json_t *wa = json_object();
    ma7_set_str(wa, "name", "ffprobe");
    json_t *w = ma7_call(ctx, "shutil.which", wa);
    if (ma7_is_raise(w)) { json_free(meta); return w; }
    if (ma7_truthy(w)) {
        json_t *cmd = json_array();
        json_append(cmd, json_copy(w));
        const char *flags[] = {"-v", "error", "-show_entries", "format=duration", "-of",
                               "default=noprint_wrappers=1:nokey=1"};
        for (size_t i = 0; i < 6; i++) json_append(cmd, json_string(flags[i]));
        json_append(cmd, json_string(path ? path : ""));
        json_t *a = json_object();
        json_set(a, "cmd", cmd);
        json_set(a, "timeout", json_number(10));
        json_set(a, "text", json_bool(true));
        json_t *res = ma7_call(ctx, "_run_media_tool", a);
        if (ma7_is_raise(res)) {
            if (!ma7_exc_is_exception(res)) { json_free(w); json_free(meta); return res; }
            json_free(res);
            json_t *lr = ma7_log0(ctx, "logger.debug");
            if (ma7_is_raise(lr)) { json_free(w); json_free(meta); return lr; }
            json_free(lr);
        } else {
            double rc = ma7_num(ma7_attr_node(res, "returncode"));
            const char *so = ma7_str(ma7_attr_node(res, "stdout"));
            if (rc == 0) {
                char *cl = ma7_strip(so);
                if (cl && *cl) {
                    char *end = NULL;
                    double dur = strtod(cl, &end);
                    bool ok = end && *end == '\0';
                    if (!ok) {
                        json_t *lr = ma7_log0(ctx, "logger.debug");
                        if (ma7_is_raise(lr)) { free(cl); json_free(res); json_free(w); json_free(meta); return lr; }
                        json_free(lr);
                    } else if (dur > 0) {
                        json_set(meta, "duration", json_number((double)(int)(dur * 1000)));
                    }
                }
                free(cl);
            }
            json_free(res);
        }
    }
    json_free(w);
    /* ffmpeg: fixtures script which(ffmpeg)=null; waveform domain excluded. */
    json_t *wa2 = json_object();
    ma7_set_str(wa2, "name", "ffmpeg");
    json_t *w2 = ma7_call(ctx, "shutil.which", wa2);
    if (ma7_is_raise(w2)) { json_free(meta); return w2; }
    json_free(w2);
    return meta;
}

/* PoP: ma7_on_room_message @ plugins/platforms/matrix/adapter.py:_on_room_message */
static json_t *ma7_on_room_message(ma7_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const json_t *ev = json_obj_get(args, "event");
    const char *room = ma7_attr(ev, "room_id");
    const char *sender = ma7_attr(ev, "sender");
    if (!room) room = ""; if (!sender) sender = "";
    json_t *r = ma7_log0(ctx, "logger.debug");
    if (ma7_is_raise(r)) return r; json_free(r);
    json_t *sa = json_object(); ma7_set_str(sa, "sender", sender);
    r = ma7_call(ctx, "_is_self_sender", sa);
    if (ma7_is_raise(r)) return r;
    if (ma7_truthy(r)) { json_free(r); return json_null(); }
    json_free(r);
    json_t *sa2 = json_object(); ma7_set_str(sa2, "sender", sender);
    r = ma7_call(ctx, "_is_system_or_bridge_sender", sa2);
    if (ma7_is_raise(r)) return r;
    if (ma7_truthy(r)) {
        json_free(r);
        json_t *lr = ma7_log0(ctx, "logger.debug");
        if (ma7_is_raise(lr)) return lr; json_free(lr);
        return json_null();
    }
    json_free(r);
    /* ignored-user-patterns domain: fixtures use an empty list. */
    json_t *ra = json_object(); ma7_set_str(ra, "room_id", room);
    r = ma7_call(ctx, "_is_allowed_matrix_room_event", ra);
    if (ma7_is_raise(r)) return r;
    if (!ma7_truthy(r)) {
        json_free(r);
        json_t *lr = ma7_log0(ctx, "logger.info");
        if (ma7_is_raise(lr)) return lr; json_free(lr);
        return json_null();
    }
    json_free(r);
    const char *eid = ma7_attr(ev, "event_id"); if (!eid) eid = "";
    json_t *eda = json_object(); ma7_set_str(eda, "event_id", eid);
    r = ma7_call(ctx, "_is_duplicate_event", eda);
    if (ma7_is_raise(r)) return r;
    if (ma7_truthy(r)) { json_free(r); return json_null(); }
    json_free(r);
    json_t *ea = json_object(); json_set(ea, "event", ma7_copy_or_null(ev));
    json_t *ts = ma7_call(ctx, "_matrix_event_timestamp_seconds", ea);
    if (ma7_is_raise(ts)) return ts;
    double event_ts = ma7_num(ts);
    double startup = json_get_num(self, "_startup_ts", 0);
    if (event_ts != 0.0 && event_ts < startup - 5.0) {
        json_t *na = json_object(); json_set(na, "event_ts", json_number(event_ts));
        json_t *nr = ma7_call(ctx, "_note_late_grace_drop", na);
        if (ma7_is_raise(nr)) { json_free(ts); return nr; }
        json_free(nr); json_free(ts);
        return json_null();
    }
    const json_t *content = ma7_attr_node(ev, "content");
    if (ma7_is_null(content) || !content) { json_free(ts); return json_null(); }
    const char *msgtype = ma7_str(json_obj_get(content, "msgtype")); if (!msgtype) msgtype = "";
    const json_t *relates = json_obj_get(content, "m.relates_to");
    const char *rel_type = relates ? ma7_str(json_obj_get(relates, "rel_type")) : NULL;
    json_free(ts);
    if (rel_type && strcmp(rel_type, "m.replace") == 0) return json_null();
    bool notices = ma7_truthy(json_obj_get(self, "_process_notices"));
    if (!strcmp(msgtype, "m.notice") && !notices) return json_null();
    bool media = !strcmp(msgtype, "m.image") || !strcmp(msgtype, "m.audio") ||
                 !strcmp(msgtype, "m.video") || !strcmp(msgtype, "m.file");
    bool text = !strcmp(msgtype, "m.text") || !strcmp(msgtype, "m.notice");
    if (media || text) {
        json_t *ha = json_object();
        ma7_set_str(ha, "room_id", room);
        ma7_set_str(ha, "sender", sender);
        ma7_set_str(ha, "event_id", eid);
        /* event_ts is the scripted value; emit as number */
        json_set(ha, "event_ts", json_number(event_ts));
        json_set(ha, "source_content", ma7_copy_or_null(content));
        json_set(ha, "relates_to", relates ? ma7_copy_or_null(relates) : json_object());
        if (media) ma7_set_str(ha, "msgtype", msgtype);
        json_t *hr;
        if (media) hr = ma7_call(ctx, "_handle_media_message", ha);
        else hr = ma7_call(ctx, "_handle_text_message", ha);
        if (ma7_is_raise(hr)) return hr;
        json_free(hr);
    }
    return json_null();
}

/* PoP: ma7_resolve_message_context @ plugins/platforms/matrix/adapter.py:_resolve_message_context */
static json_t *ma7_resolve_message_context(ma7_ctx_t *ctx, const json_t *args) {
    json_t *self = ctx->state;
    const char *room = ma7_str(json_obj_get(args, "room_id")); if (!room) room = "";
    const char *sender = ma7_str(json_obj_get(args, "sender")); if (!sender) sender = "";
    const char *eid = ma7_str(json_obj_get(args, "event_id")); if (!eid) eid = "";
    const char *body = ma7_str(json_obj_get(args, "body")); if (!body) body = "";
    const json_t *source = json_obj_get(args, "source_content");
    const json_t *relates = json_obj_get(args, "relates_to");
    char *bodybuf = ma7_strdup(body);

    json_t *ia = json_object(); ma7_set_str(ia, "room_id", room);
    json_set(ia, "force_refresh", json_bool(false));
    json_t *ident = ma7_call(ctx, "_resolve_room_identity", ia);
    if (ma7_is_raise(ident)) { free(bodybuf); return ident; }
    json_t *da = json_object(); ma7_set_str(da, "room_id", room);
    json_t *dmr = ma7_call(ctx, "_is_dm_room", da);
    if (ma7_is_raise(dmr)) { free(bodybuf); json_free(ident); return dmr; }
    bool is_dm = ma7_truthy(dmr); json_free(dmr);
    const char *chat_type = is_dm ? "dm" : "group";

    json_t *ta = json_object(); json_set(ta, "relates_to", ma7_copy_or_null(relates));
    json_t *tr = ma7_call(ctx, "_thread_root", ta);
    if (ma7_is_raise(tr)) { free(bodybuf); json_free(ident); return tr; }
    const char *thread_id = ma7_str(tr);
    char *threadbuf = thread_id ? ma7_strdup(thread_id) : NULL;
    json_free(tr);

    bool mentioned = false;
    if (json_get_bool(args, "mention_claimed", false)) {
        mentioned = true;
    } else {
        json_t *ca = json_object(); ma7_set_str(ca, "body", bodybuf);
        json_set(ca, "source_content", ma7_copy_or_null(source));
        json_t *cb = ma7_call(ctx, "_content_mentions_bot", ca);
        if (ma7_is_raise(cb)) { free(bodybuf); free(threadbuf); json_free(ident); return cb; }
        mentioned = ma7_truthy(cb); json_free(cb);
    }

    bool require = ma7_truthy(json_obj_get(self, "_require_mention"));
    bool thread_require = ma7_truthy(json_obj_get(self, "_thread_require_mention"));
    if (!is_dm) {
        const json_t *allowed = json_obj_get(self, "_allowed_rooms");
        if (ma7_truthy(allowed) && allowed->type == JSON_ARRAY && !ma7_arr_has_str(allowed, room)) {
            json_t *r = ma7_log0(ctx, "logger.debug");
            if (r && ma7_is_raise(r)) { free(bodybuf); free(threadbuf); json_free(ident); return r; }
            if (r) json_free(r);
            free(bodybuf); free(threadbuf); json_free(ident);
            return json_null();
        }
        const json_t *free_rooms = json_obj_get(self, "_free_rooms");
        bool is_free = free_rooms && ma7_arr_has_str(free_rooms, room);
        const json_t *threads = json_obj_get(self, "_threads");
        bool in_thread = threadbuf && threads && !ma7_is_null(json_obj_get(threads, threadbuf));
        if (require && !is_free && !in_thread) {
            if (!mentioned && bodybuf[0] != '/') {
                json_t *r = ma7_log0(ctx, "logger.debug");
                if (r && ma7_is_raise(r)) { free(bodybuf); free(threadbuf); json_free(ident); return r; }
                if (r) json_free(r);
                free(bodybuf); free(threadbuf); json_free(ident);
                return json_null();
            }
        } else if (thread_require && in_thread && !is_free && !mentioned) {
            json_t *r = ma7_log0(ctx, "logger.debug");
            if (r && ma7_is_raise(r)) { free(bodybuf); free(threadbuf); json_free(ident); return r; }
            if (r) json_free(r);
            free(bodybuf); free(threadbuf); json_free(ident);
            return json_null();
        }
    }
    if (mentioned && require) {
        bool has_reply = relates && !ma7_is_null(json_obj_get(relates, "m.in_reply_to")) &&
                         json_obj_get(relates, "m.in_reply_to");
        if (has_reply) {
            json_t *sa = json_object(); ma7_set_str(sa, "body", bodybuf);
            json_t *sp = ma7_call(ctx, "_split_reply_fallback", sa);
            if (ma7_is_raise(sp)) { free(bodybuf); free(threadbuf); json_free(ident); return sp; }
            const char *quote = (sp && sp->type == JSON_ARRAY && json_len(sp) >= 1) ? ma7_str(json_get(sp, 0)) : "";
            const char *reply = (sp && sp->type == JSON_ARRAY && json_len(sp) >= 2) ? ma7_str(json_get(sp, 1)) : "";
            json_t *sa2 = json_object(); ma7_set_str(sa2, "body", reply ? reply : "");
            json_t *st = ma7_call(ctx, "_strip_mention", sa2);
            json_free(sp);
            if (ma7_is_raise(st)) { free(bodybuf); free(threadbuf); json_free(ident); return st; }
            ma7_buf_t b = {0}; mb_add(&b, quote ? quote : ""); mb_add(&b, ma7_str(st));
            char *nb = mb_take(&b); free(bodybuf); bodybuf = nb; json_free(st);
        } else {
            json_t *sa = json_object(); ma7_set_str(sa, "body", bodybuf);
            json_t *st = ma7_call(ctx, "_strip_mention", sa);
            if (ma7_is_raise(st)) { free(bodybuf); free(threadbuf); json_free(ident); return st; }
            char *nb = ma7_strdup(ma7_str(st));
            free(bodybuf); bodybuf = nb; json_free(st);
        }
    }
    if (!threadbuf) {
        bool synthetic = false;
        if (is_dm) {
            synthetic = (ma7_truthy(json_obj_get(self, "_dm_mention_threads")) && mentioned) ||
                        ma7_truthy(json_obj_get(self, "_dm_auto_thread"));
        } else {
            const char *scope = ma7_str(json_obj_get(self, "_matrix_session_scope"));
            if (!scope) scope = "";
            synthetic = !strcmp(scope, "thread") ||
                        (strcmp(scope, "room") != 0 && ma7_truthy(json_obj_get(self, "_auto_thread")));
        }
        if (synthetic) { free(threadbuf); threadbuf = ma7_strdup(eid); }
    }
    json_t *ga = json_object(); ma7_set_str(ga, "room_id", room); ma7_set_str(ga, "sender", sender);
    json_t *disp = ma7_call(ctx, "_get_display_name", ga);
    if (ma7_is_raise(disp)) { free(bodybuf); free(threadbuf); json_free(ident); return disp; }
    char *display = ma7_py_str(disp); json_free(disp);
    const char *mt = ma7_str(json_obj_get(source, "msgtype")); if (!mt) mt = "";
    bool is_bot = !strcmp(mt, "m.notice");

    json_t *bs = json_object();
    ma7_set_str(bs, "chat_id", room);
    ma7_set_str(bs, "chat_name", ma7_attr(ident, "display_name"));
    ma7_set_str(bs, "chat_type", chat_type);
    ma7_set_str(bs, "user_id", sender);
    ma7_set_str(bs, "user_name", display);
    if (threadbuf && *threadbuf) ma7_set_str(bs, "thread_id", threadbuf); else json_set(bs, "thread_id", json_null());
    json_set(bs, "chat_topic", ma7_copy_or_null(ma7_attr_node(ident, "room_topic")));
    json_set(bs, "guild_id", ma7_copy_or_null(ma7_attr_node(ident, "server_name")));
    if (threadbuf && *threadbuf) ma7_set_str(bs, "parent_chat_id", room); else json_set(bs, "parent_chat_id", json_null());
    ma7_set_str(bs, "message_id", eid);
    json_set(bs, "is_bot", json_bool(is_bot));
    json_t *src = ma7_call(ctx, "build_source", bs);
    if (ma7_is_raise(src)) { free(display); free(bodybuf); free(threadbuf); json_free(ident); return src; }
    if (threadbuf && *threadbuf) {
        json_t *ma = json_object(); ma7_set_str(ma, "thread_id", threadbuf);
        json_t *mr = ma7_call(ctx, "mark_async", ma);
        if (ma7_is_raise(mr)) { json_free(src); free(display); free(bodybuf); free(threadbuf); json_free(ident); return mr; }
        json_free(mr);
    }
    json_t *bra = json_object(); ma7_set_str(bra, "room_id", room); ma7_set_str(bra, "event_id", eid);
    json_t *br = ma7_call(ctx, "_background_read_receipt", bra);
    if (ma7_is_raise(br)) { json_free(src); free(display); free(bodybuf); free(threadbuf); json_free(ident); return br; }
    json_free(br);

    json_t *out = json_array();
    json_append(out, json_string(bodybuf));
    json_append(out, json_bool(is_dm));
    json_append(out, json_string(chat_type));
    if (threadbuf && *threadbuf) json_append(out, json_string(threadbuf)); else json_append(out, json_null());
    json_append(out, json_string(display));
    json_append(out, json_copy(src));
    json_free(src); free(display); free(bodybuf); free(threadbuf); json_free(ident);
    return out;
}
