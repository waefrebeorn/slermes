/*
 * feishu_adapter_flow13.c — Feishu/Lark adapter "remainders" FLOW cores of
 * plugins/platforms/feishu/adapter.py (hermes-agent).
 *
 * THIRTEENTH remainder port beyond feishu_adapter_flow.c .. flow12. Ports 14
 * genuinely unclaimed defs (none is a TARGET of flows 1-12):
 *   _record_webhook_anomaly, _release_app_lock, _reschedule_batch_task,
 *   _reset_batch_buffers, _resolve_approval, _resolve_channel_prompt,
 *   _resolve_normalized_message_type, _resolve_sender_name_from_api,
 *   _resolve_sender_profile, _resolve_update_prompt,
 *   _schedule_text_batch_flush, _schedule_media_batch_flush,
 *   _EA_REASON_LABEL, _EA_SMART_DENY_LINE.
 *
 * Universal seam: every seamed call enters through ONE callback
 * fa13_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting the
 * identical {"name","args","ret"} event from the SAME scripts. args is ADOPTED
 * into the trace event; the return is a NEW owned json_t (NULL == Python None;
 * {"__raise__":cls,"message":...} == a raised exception). A missing/exhausted
 * script is a fail-closed RuntimeError on both sides.
 *
 * FULL BASEEXCEPTION PASSTHROUGH: fa13_is_base_exc + fa13_raise_is_base name the
 * family that bypasses `except Exception`; those raises propagate verbatim.
 *
 * Representation contract (shared with tests/oracle_feishu_adapter_flow13.py):
 * - NULL json_t* == Python None; JSON_NULL node is a real null value.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__":cls,"message":str(exc)}; a scripted raise is
 *   {"__script__":true,"raise":{"type":cls,"message":msg}}.
 * - Fixture domain: ASCII scalars, JSON dict/list values. State is {}.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and fa13_-prefixed.
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
typedef struct fa13_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} fa13_ctx_t;

/* ── basic helpers ── */
static bool fa13_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool fa13_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool fa13_is_base_exc(const char *kind) {
    return kind && (
        strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "asyncio.CancelledError") == 0);
}
static bool fa13_raise_is_base(const json_t *r) {
    return fa13_is_raise(r) && fa13_is_base_exc(json_get_str(r, "__raise__", ""));
}
static double fa13_as_num(const json_t *v, double d) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : d;
}
static bool fa13_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *fa13_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static const char *fa13_str_or(const json_t *v, const char *d) {
    const char *s = fa13_str(v);
    return s ? s : d;
}
static bool fa13_bool(const json_t *v, bool d) {
    return (v && v->type == JSON_BOOL) ? v->bool_val : d;
}
static json_t *fa13_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *fa13_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *e = fa13_err(type, msg);
    const char *partial = json_get_str(spec, "partial", NULL);
    if (partial) json_set_hdr(e, "partial", partial);
    return e;
}
/* The one universal seam. args is ADOPTED; the return is a fresh owned node. */
static json_t *fa13_call(fa13_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = fa13_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = fa13_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = fa13_make_exc(raise);
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
static json_t *fa13_args(void) { return json_object(); }
static void fa13_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void fa13_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void fa13_set_n(json_t *a, const char *k, double n) { json_set(a, k, json_number(n)); }
static void fa13_set_b(json_t *a, const char *k, bool b) { json_set(a, k, json_bool(b)); }
/* Python str.strip() + optional lower, ASCII fixture domain. */
static const char *fa13_snorm(const char *s, char *out, size_t cap, bool lower) {
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
static bool fa13_starts(const char *s, const char *p) {
    return s && p && strncmp(s, p, strlen(p)) == 0;
}

/* ══ PoP functions ══ */

/* PoP: fa13__record_webhook_anomaly @ plugins/platforms/feishu/adapter.py:FeishuAdapter._record_webhook_anomaly */
static json_t *fa13__record_webhook_anomaly(fa13_ctx_t *ctx, const json_t *args) {
    const char *remote_ip = fa13_str_or(json_obj_get(args, "remote_ip"), "");
    const char *status = fa13_str_or(json_obj_get(args, "status"), "");
    const json_t *prior = json_obj_get(args, "prior");
    json_t *t = fa13_call(ctx, "time_time", fa13_args());
    if (fa13_is_raise(t)) return t;
    double now = fa13_as_num(t, 0); json_free(t);
    double count = 0, first_seen = now;
    if (prior && prior->type == JSON_ARRAY && json_len(prior) >= 3) {
        count = fa13_as_num(json_get(prior, 0), 0);
        first_seen = fa13_as_num(json_get(prior, 2), now);
    }
    if (count && now - first_seen >= 21600.0) { count = 0; first_seen = now; }
    count += 1;
    json_t *a = fa13_args();
    fa13_set_s(a, "remote_ip", remote_ip);
    fa13_set_n(a, "count", count);
    fa13_set_s(a, "status", status);
    fa13_set_n(a, "first_seen", first_seen);
    json_t *r = fa13_call(ctx, "anomaly_set", a);
    if (fa13_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: fa13__release_app_lock @ plugins/platforms/feishu/adapter.py:FeishuAdapter._release_app_lock */
static json_t *fa13__release_app_lock(fa13_ctx_t *ctx, const json_t *args) {
    const char *identity = fa13_str_or(json_obj_get(args, "app_lock_identity"), "");
    if (!identity[0]) return json_null();
    json_t *a = fa13_args();
    fa13_set_s(a, "scope", "feishu-app-id");
    fa13_set_s(a, "identity", identity);
    json_t *r = fa13_call(ctx, "release_scoped_lock", a);
    json_t *pending = NULL;   /* exception to propagate after finally */
    if (fa13_is_raise(r)) {
        if (fa13_raise_is_base(r)) {
            /* finally still runs, but the BaseException propagates */
            pending = r;
        } else {
            json_free(r);   /* except Exception: log (no observable) */
        }
    } else {
        json_free(r);
    }
    /* finally: self._app_lock_identity = None */
    json_t *c = fa13_call(ctx, "clear_app_lock_identity", fa13_args());
    if (fa13_is_raise(c)) {
        if (pending) json_free(pending);
        return c;   /* a raise in finally replaces any pending exception */
    }
    json_free(c);
    return pending ? pending : json_null();
}

/* PoP: fa13__reschedule_batch_task @ plugins/platforms/feishu/adapter.py:FeishuAdapter._reschedule_batch_task */
static json_t *fa13__reschedule_batch_task(fa13_ctx_t *ctx, const json_t *args) {
    const char *key = fa13_str_or(json_obj_get(args, "key"), "");
    bool has_prior = fa13_bool(json_obj_get(args, "has_prior"), false);
    if (has_prior) {
        json_t *d = fa13_call(ctx, "task_done", fa13_args());
        if (fa13_is_raise(d)) return d;
        bool done = fa13_truthy(d); json_free(d);
        if (!done) {
            json_t *c = fa13_call(ctx, "task_cancel", fa13_args());
            if (fa13_is_raise(c)) return c;
            json_free(c);
        }
    }
    json_t *fa = fa13_args(); fa13_set_s(fa, "key", key);
    json_t *f = fa13_call(ctx, "flush_fn", fa);
    if (fa13_is_raise(f)) return f;
    json_free(f);
    json_t *cr = fa13_call(ctx, "create_task", fa13_args());
    if (fa13_is_raise(cr)) return cr;
    json_free(cr);
    return json_null();
}

/* PoP: fa13__reset_batch_buffers @ plugins/platforms/feishu/adapter.py:FeishuAdapter._reset_batch_buffers */
static json_t *fa13__reset_batch_buffers(fa13_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *r;
    r = fa13_call(ctx, "clear_text_batches", fa13_args());
    if (fa13_is_raise(r)) return r; json_free(r);
    r = fa13_call(ctx, "clear_text_counts", fa13_args());
    if (fa13_is_raise(r)) return r; json_free(r);
    r = fa13_call(ctx, "clear_media_batches", fa13_args());
    if (fa13_is_raise(r)) return r; json_free(r);
    return json_null();
}

/* helper: build pop_validated_prompt_state args (shared by approval + update) */
static json_t *fa13_popargs(const json_t *args, const char *label, const char *ident_key) {
    json_t *a = fa13_args();
    fa13_set_s(a, "ident", fa13_str_or(json_obj_get(args, ident_key), ""));
    fa13_set_s(a, "label", label);
    fa13_set_copy(a, "open_id", json_obj_get(args, "open_id"));
    fa13_set_copy(a, "chat_id", json_obj_get(args, "chat_id"));
    fa13_set_copy(a, "user_id", json_obj_get(args, "user_id"));
    fa13_set_copy(a, "union_id", json_obj_get(args, "union_id"));
    const char *op = fa13_str_or(json_obj_get(args, "open_id"), "<unknown>");
    fa13_set_s(a, "operator_repr", op);
    return a;
}

/* PoP: fa13__resolve_approval @ plugins/platforms/feishu/adapter.py:FeishuAdapter._resolve_approval */
static json_t *fa13__resolve_approval(fa13_ctx_t *ctx, const json_t *args) {
    const char *choice = fa13_str_or(json_obj_get(args, "choice"), "");
    json_t *state = fa13_call(ctx, "pop_validated_prompt_state",
                              fa13_popargs(args, "Approval", "ident"));
    if (fa13_is_raise(state)) return state;
    if (!fa13_truthy(state)) { json_free(state); return json_null(); }
    double count = 0;
    /* try: */
    json_t *a = fa13_args();
    fa13_set_copy(a, "session_key", json_obj_get(state, "session_key"));
    fa13_set_s(a, "choice", choice);
    fa13_set_copy(a, "request_id", json_obj_get(state, "request_id"));
    const char *user = fa13_str_or(json_obj_get(args, "user_id"), "");
    const char *op = user[0] ? user : fa13_str_or(json_obj_get(args, "open_id"), "");
    fa13_set_s(a, "actor_id", op);
    fa13_set_copy(a, "extra", json_obj_get(args, "extra"));
    json_t *res = fa13_call(ctx, "resolve_gateway_approval_card", a);
    if (fa13_is_raise(res)) {
        if (fa13_raise_is_base(res)) { json_free(state); return res; }
        json_free(res); json_free(state); return json_null();   /* except Exception: log */
    }
    count = fa13_as_num(res, 0); json_free(res);
    /* if not count and choice != "deny": */
    if (!(count == 0) || strcmp(choice, "deny") == 0) { json_free(state); return json_null(); }
    const char *chat_state = fa13_str_or(json_obj_get(state, "chat_id"), "");
    const char *chat_ref = chat_state[0] ? chat_state
                         : fa13_str_or(json_obj_get(args, "chat_id"), "");
    size_t clen = strlen(chat_ref);
    char *chat = malloc(clen + 1);
    memcpy(chat, chat_ref, clen + 1);
    json_free(state);
    if (!chat[0]) { free(chat); return json_null(); }
    /* inner try: send(chat, t(...)) */
    json_t *ta = fa13_args(); fa13_set_s(ta, "key", "platform.shared.approval_expired");
    json_t *msg = fa13_call(ctx, "t", ta);
    if (fa13_is_raise(msg)) {
        if (fa13_raise_is_base(msg)) { free(chat); return msg; }
        json_free(msg); free(chat); return json_null();   /* inner except Exception: log */
    }
    json_t *sa = fa13_args();
    fa13_set_s(sa, "chat_id", chat);
    free(chat);
    fa13_set_copy(sa, "message", msg);
    json_t *s = fa13_call(ctx, "send", sa);
    if (fa13_is_raise(s)) {
        if (fa13_raise_is_base(s)) return s;
        json_free(s); return json_null();
    }
    json_free(s);
    return json_null();
}

/* PoP: fa13__resolve_channel_prompt @ plugins/platforms/feishu/adapter.py:FeishuAdapter._resolve_channel_prompt */
static json_t *fa13__resolve_channel_prompt(fa13_ctx_t *ctx, const json_t *args) {
    json_t *a = fa13_args();
    fa13_set_copy(a, "extra", json_obj_get(args, "extra"));
    fa13_set_copy(a, "chat_id", json_obj_get(args, "chat_id"));
    fa13_set_copy(a, "parent_id", json_obj_get(args, "parent_id"));
    return fa13_call(ctx, "resolve_channel_prompt", a);
}

/* PoP: fa13__resolve_normalized_message_type @ plugins/platforms/feishu/adapter.py:FeishuAdapter._resolve_normalized_message_type */
static json_t *fa13__resolve_normalized_message_type(fa13_ctx_t *ctx, const json_t *args) {
    const char *preferred = fa13_str_or(json_obj_get(args, "preferred"), "");
    const json_t *media = json_obj_get(args, "media_types");
    if (strcmp(preferred, "audio") == 0) return json_string("voice");
    if (strcmp(preferred, "photo") == 0 || strcmp(preferred, "document") == 0) {
        const char *def = strcmp(preferred, "photo") == 0 ? "photo" : "document";
        const char *mt = "";
        if (media && media->type == JSON_ARRAY && json_len(media) > 0)
            mt = fa13_str_or(json_get(media, 0), "");
        json_t *a = fa13_args();
        fa13_set_s(a, "media_type", mt);
        fa13_set_s(a, "default", def);
        json_t *r = fa13_call(ctx, "resolve_media_message_type", a);
        if (fa13_is_raise(r)) return r;
        const char *s = fa13_str_or(r, "");
        json_t *out = json_string(s); json_free(r); return out;
    }
    return json_string("text");
}

/* PoP: fa13__resolve_sender_name_from_api @ plugins/platforms/feishu/adapter.py:FeishuAdapter._resolve_sender_name_from_api */
static json_t *fa13__resolve_sender_name_from_api(fa13_ctx_t *ctx, const json_t *args) {
    const char *sid = fa13_str_or(json_obj_get(args, "sender_id"), "");
    bool is_bot = fa13_bool(json_obj_get(args, "is_bot"), false);
    bool has_client = fa13_bool(json_obj_get(args, "has_client"), false);
    char trimmed[1024];
    fa13_snorm(has_client ? sid : "", trimmed, sizeof trimmed, false);
    if (!trimmed[0]) return json_null();
    json_t *t = fa13_call(ctx, "time_time", fa13_args());
    if (fa13_is_raise(t)) return t;
    double now = fa13_as_num(t, 0); json_free(t);
    json_t *ca = fa13_args(); fa13_set_s(ca, "trimmed", trimmed);
    json_t *cached = fa13_call(ctx, "get_cached_sender_name", ca);
    if (fa13_is_raise(cached)) return cached;
    if (!fa13_is_null(cached)) {
        json_t *out = fa13_truthy(cached) ? json_copy(cached) : json_null();
        json_free(cached); return out;
    }
    json_free(cached);
    if (is_bot) {
        json_t *na = fa13_args();
        json_t *ids = json_array(); json_append(ids, json_string(trimmed));
        json_set(na, "ids", ids);
        json_t *names = fa13_call(ctx, "fetch_bot_names", na);
        if (fa13_is_raise(names)) return names;
        if (fa13_is_null(names)) { json_free(names); return json_null(); }
        /* for oid, name in names.items(): cache[oid] = (name, now + 600) */
        if (names->type == JSON_OBJECT && names->c.keys) {
            for (size_t i = 0; i < names->c.count; i++) {
                const char *oid = names->c.keys[i];
                json_t *sa = fa13_args();
                fa13_set_s(sa, "oid", oid);
                fa13_set_copy(sa, "name", names->c.items[i]);
                fa13_set_n(sa, "expires", now + 600.0);
                json_t *r = fa13_call(ctx, "sender_cache_set", sa);
                if (fa13_is_raise(r)) { json_free(names); return r; }
                json_free(r);
            }
        }
        json_free(names);
        json_t *ga = fa13_args(); fa13_set_s(ga, "trimmed", trimmed);
        json_t *hit = fa13_call(ctx, "sender_cache_get", ga);
        if (fa13_is_raise(hit)) return hit;
        json_t *out = NULL;
        if (fa13_truthy(hit) && hit->type == JSON_ARRAY && json_len(hit) > 0)
            out = fa13_truthy(json_get(hit, 0)) ? json_copy(json_get(hit, 0)) : json_null();
        else out = json_null();
        json_free(hit);
        return out;
    }
    /* non-bot: whole block under try/except Exception */
    const char *id_type = "user_id";
    if (fa13_starts(trimmed, "ou_")) id_type = "open_id";
    else if (fa13_starts(trimmed, "on_")) id_type = "union_id";
    json_t *ba = fa13_args();
    fa13_set_s(ba, "trimmed", trimmed);
    fa13_set_s(ba, "id_type", id_type);
    json_t *req = fa13_call(ctx, "build_get_user_request", ba);
    json_t *pending = NULL;
    if (fa13_is_raise(req)) {
        if (fa13_raise_is_base(req)) return req;
        json_free(req); return json_null();
    }
    json_t *ra = fa13_args();
    fa13_set_copy(ra, "request", req);
    json_free(req);
    json_t *resp = fa13_call(ctx, "run_blocking", ra);
    if (fa13_is_raise(resp)) {
        if (fa13_raise_is_base(resp)) return resp;
        json_free(resp); return json_null();
    }
    bool success = false;
    json_t *user = NULL;
    if (resp && resp->type == JSON_OBJECT) {
        success = fa13_bool(json_obj_get(resp, "success"), false);
        user = json_obj_get(resp, "user");
    }
    json_t *out = json_null();
    if (success && user && user->type == JSON_OBJECT) {
        const char *keys[4] = {"name", "display_name", "nickname", "en_name"};
        const char *found = NULL;
        for (int i = 0; i < 4 && !found; i++) {
            const char *v = fa13_str_or(json_obj_get(user, keys[i]), "");
            if (v[0]) found = v;
        }
        if (found) {
            char nm[1024];
            fa13_snorm(found, nm, sizeof nm, false);
            if (nm[0]) {
                json_t *sa = fa13_args();
                fa13_set_s(sa, "oid", trimmed);
                fa13_set_s(sa, "name", nm);
                fa13_set_n(sa, "expires", now + 600.0);
                json_t *r = fa13_call(ctx, "sender_cache_set", sa);
                if (fa13_is_raise(r)) {
                    if (fa13_raise_is_base(r)) { json_free(resp); return r; }
                    json_free(r);
                } else json_free(r);
                out = json_string(nm);
            }
        }
    }
    json_free(resp);
    (void)pending;
    return out;
}

/* PoP: fa13__resolve_sender_profile @ plugins/platforms/feishu/adapter.py:FeishuAdapter._resolve_sender_profile */
static json_t *fa13__resolve_sender_profile(fa13_ctx_t *ctx, const json_t *args) {
    const json_t *sid = json_obj_get(args, "sender_id");
    bool is_bot = fa13_bool(json_obj_get(args, "is_bot"), false);
    const json_t *open_id = json_obj_get(sid, "open_id");
    const json_t *user_id = json_obj_get(sid, "user_id");
    const json_t *union_id = json_obj_get(sid, "union_id");
    if (!fa13_truthy(open_id)) open_id = NULL;
    if (!fa13_truthy(user_id)) user_id = NULL;
    if (!fa13_truthy(union_id)) union_id = NULL;
    const json_t *primary = fa13_truthy(user_id) ? user_id : open_id;
    const json_t *lookup = NULL;
    if (is_bot) lookup = open_id;
    else lookup = fa13_truthy(primary) ? primary : union_id;
    json_t *a = fa13_args();
    fa13_set_copy(a, "sender_id", lookup);
    fa13_set_b(a, "is_bot", is_bot);
    json_t *name = fa13_call(ctx, "resolve_sender_name_from_api", a);
    if (fa13_is_raise(name)) return name;
    json_t *out = json_object();
    fa13_set_copy(out, "user_id", primary);
    json_set(out, "user_name", name ? name : json_null());
    fa13_set_copy(out, "user_id_alt", union_id);
    return out;
}

/* PoP: fa13__resolve_update_prompt @ plugins/platforms/feishu/adapter.py:FeishuAdapter._resolve_update_prompt */
static json_t *fa13__resolve_update_prompt(fa13_ctx_t *ctx, const json_t *args) {
    const char *answer = fa13_str_or(json_obj_get(args, "answer"), "");
    json_t *state = fa13_call(ctx, "pop_validated_prompt_state",
                              fa13_popargs(args, "Update prompt", "ident"));
    if (fa13_is_raise(state)) return state;
    if (!fa13_truthy(state)) { json_free(state); return json_null(); }
    json_free(state);
    json_t *a = fa13_args(); fa13_set_s(a, "answer", answer);
    json_t *r = fa13_call(ctx, "write_update_prompt_response", a);
    if (fa13_is_raise(r)) {
        if (fa13_raise_is_base(r)) return r;
        json_free(r); return json_null();   /* except Exception: log */
    }
    json_free(r);
    return json_null();
}

/* PoP: fa13__schedule_text_batch_flush @ plugins/platforms/feishu/adapter.py:FeishuAdapter._schedule_text_batch_flush */
static json_t *fa13__schedule_text_batch_flush(fa13_ctx_t *ctx, const json_t *args) {
    json_t *a = fa13_args();
    fa13_set_s(a, "kind", "text");
    fa13_set_copy(a, "key", json_obj_get(args, "key"));
    json_t *r = fa13_call(ctx, "reschedule_batch_task", a);
    if (fa13_is_raise(r)) return r;
    json_free(r); return json_null();
}

/* PoP: fa13__schedule_media_batch_flush @ plugins/platforms/feishu/adapter.py:FeishuAdapter._schedule_media_batch_flush */
static json_t *fa13__schedule_media_batch_flush(fa13_ctx_t *ctx, const json_t *args) {
    json_t *a = fa13_args();
    fa13_set_s(a, "kind", "media");
    fa13_set_copy(a, "key", json_obj_get(args, "key"));
    json_t *r = fa13_call(ctx, "reschedule_batch_task", a);
    if (fa13_is_raise(r)) return r;
    json_free(r); return json_null();
}

/* PoP: fa13__EA_REASON_LABEL @ plugins/platforms/feishu/adapter.py:FeishuAdapter._EA_REASON_LABEL */
static json_t *fa13__EA_REASON_LABEL(fa13_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = fa13_args(); fa13_set_s(a, "key", "gateway.exec_approval.reason_label");
    json_t *v = fa13_call(ctx, "t", a);
    if (fa13_is_raise(v)) return v;
    char buf[8192];
    snprintf(buf, sizeof buf, "**%s:** ", fa13_str_or(v, ""));
    json_free(v);
    return json_string(buf);
}

/* PoP: fa13__EA_SMART_DENY_LINE @ plugins/platforms/feishu/adapter.py:FeishuAdapter._EA_SMART_DENY_LINE */
static json_t *fa13__EA_SMART_DENY_LINE(fa13_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = fa13_args(); fa13_set_s(a, "key", "gateway.exec_approval.smart_deny_line");
    json_t *v = fa13_call(ctx, "t", a);
    if (fa13_is_raise(v)) return v;
    const char *line = fa13_str_or(v, "");
    const char *sep = strchr(line, ':');
    char buf[8192];
    if (sep) {
        size_t llen = (size_t)(sep - line);
        char label[4096];
        snprintf(label, sizeof label, "%.*s", (int)llen, line);
        snprintf(buf, sizeof buf, "\n\n**%s:**%s", label, sep + 1);
    } else {
        snprintf(buf, sizeof buf, "\n\n%s", line);
    }
    json_free(v);
    return json_string(buf);
}
