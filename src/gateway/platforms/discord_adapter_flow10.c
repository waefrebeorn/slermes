/*
 * discord_adapter_flow10.c — Discord adapter "remainders" FLOW cores of
 * plugins/platforms/discord/adapter.py (hermes-agent).
 *
 * TENTH remainder port beyond discord_adapter_flow.c .. flow9. Ports 16
 * genuinely unclaimed defs (dotted-name-aware claim parser confirmed none are
 * claimed by flows 1-9):
 *   _create_thread, _defer_unless_expired, _define_discord_view_classes,
 *   _deliver_unauthorized_slash_alert, _desired_command_sync_fingerprint,
 *   _discord_message_admission, _discord_message_attempts_exhausted,
 *   _discord_message_has_active_claim, _discord_message_is_persistently_complete,
 *   _discord_recovery_cursor, _dispatch_discord_message,
 *   _dispatch_recovered_message, _dispatch_thread_session,
 *   _write_command_sync_state, _sleep_between_command_sync_mutations,
 *   _resolve_channel_prompt.
 *
 * Universal seam: every seamed call enters through ONE callback
 * da10_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting the
 * identical {"name","args","ret"} event from the SAME scripts. args is ADOPTED
 * into the trace event; the return is a NEW owned json_t (NULL == Python None;
 * {"__raise__":cls,"message":...} == a raised exception). A missing/exhausted
 * script is a fail-closed RuntimeError on both sides.
 *
 * FULL BASEEXCEPTION PASSTHROUGH: da10_is_base_exc + da10_raise_is_base name the
 * family that bypasses `except Exception`; those raises propagate verbatim.
 *
 * Representation contract (shared with tests/oracle_discord_adapter_flow10.py):
 * - NULL json_t* == Python None; JSON_NULL node is a real null value.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__":cls,"message":str(exc)}; a scripted raise is
 *   {"__script__":true,"raise":{"type":cls,"message":msg}}.
 * - Carrier fields are read directly from args doubles (oracle materializes
 *   equivalent attribute doubles). Fixture domain: ASCII strings, scalars,
 *   JSON dict/list values, identifier-like markers, integral-int numbers.
 *   Out.state is {} — all observable behavior is ret + ordered trace.
 * - _define_discord_view_classes is a module-global registration with NO
 *   seamed calls; the C port models it as a no-op returning None (empty trace).
 *   The oracle drives the LIVE def with benign discord stubs; it returns None.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and da10_-prefixed.
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
typedef struct da10_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} da10_ctx_t;

/* ── basic helpers ── */
static bool da10_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool da10_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool da10_is_base_exc(const char *kind) {
    return kind && (
        strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "asyncio.CancelledError") == 0);
}
static bool da10_raise_is_base(const json_t *r) {
    return da10_is_raise(r) && da10_is_base_exc(json_get_str(r, "__raise__", ""));
}
static bool da10_raise_is_timeout(const json_t *r) {
    return da10_is_raise(r) && strcmp(json_get_str(r, "__raise__", ""), "TimeoutError") == 0;
}
static double da10_as_num(const json_t *v, double d) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : d;
}
static bool da10_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *da10_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static double da10_num(const json_t *v, double d) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : d;
}
static bool da10_bool(const json_t *v, bool d) {
    return (v && v->type == JSON_BOOL) ? v->bool_val : d;
}
static const char *da10_str_or(const json_t *v, const char *d) {
    const char *s = da10_str(v);
    return s ? s : d;
}
static json_t *da10_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *da10_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return da10_err(type, msg);
}
/* The one universal seam. args is ADOPTED; the return is a fresh owned node. */
static json_t *da10_call(da10_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = da10_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = da10_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = da10_make_exc(raise);
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
static json_t *da10_args(void) { return json_object(); }
static void da10_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void da10_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
/* Python str.strip() for the ASCII fixture domain. */
static char *da10_strip_inplace(const char *s) {
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
static bool da10_in_i(int v, const int *arr, size_t n) {
    for (size_t i = 0; i < n; i++) if (arr[i] == v) return true;
    return false;
}
/* Python json.dumps(obj, sort_keys=True, separators=(",",":")) for the scalar/
 * container fixture domain (ASCII). Returns a heap string caller frees. */
static void da10_jdump(json_t *v, char *out, size_t cap, size_t *len) {
    if (!v || *len + 1 >= cap) return;
    #define PUT(ch) do { if (*len + 1 < cap) out[(*len)++] = (char)(ch); } while (0)
    #define PUTS(s) do { const char *_p=(s); while (*_p && *len+1<cap) out[(*len)++]=*_p++; } while (0)
    switch (v->type) {
        case JSON_NULL: PUTS("null"); break;
        case JSON_BOOL: PUTS(v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char nb[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(nb, sizeof nb, "%.0f", v->num_val);
            else
                snprintf(nb, sizeof nb, "%g", v->num_val);
            PUTS(nb); break;
        }
        case JSON_STRING: {
            PUT('"');
            const char *p = v->str_val ? v->str_val : "";
            for (; *p && *len + 2 < cap; ++p) {
                unsigned char c = (unsigned char)*p;
                if (c == '"' || c == '\\') { PUT('\\'); PUT(c); }
                else if (c == '\n') { PUTS("\\n"); }
                else if (c == '\r') { PUTS("\\r"); }
                else if (c == '\t') { PUTS("\\t"); }
                else PUT(c);
            }
            PUT('"'); break;
        }
        case JSON_ARRAY: {
            PUT('[');
            for (size_t i = 0; i < json_len(v); i++) {
                if (i) PUT(',');
                da10_jdump(json_get(v, i), out, cap, len);
            }
            PUT(']'); break;
        }
        case JSON_OBJECT: {
            PUT('{');
            /* sorted keys (ASCII) */
            size_t n = json_object_size(v);
            const char *keys[512];
            if (n > 512) n = 512;
            for (size_t i = 0; i < n; i++) keys[i] = json_object_get_key_at(v, i);
            for (size_t i = 0; i < n; i++) for (size_t j = i + 1; j < n; j++) {
                if (strcmp(keys[i], keys[j]) > 0) { const char *t = keys[i]; keys[i] = keys[j]; keys[j] = t; }
            }
            for (size_t i = 0; i < n; i++) {
                if (i) PUT(',');
                /* json.dumps quotes keys like strings */
                PUT('"'); PUTS(keys[i]); PUT('"'); PUT(':');
                da10_jdump(json_obj_get(v, keys[i]), out, cap, len);
            }
            PUT('}'); break;
        }
    }
    #undef PUT
    #undef PUTS
}

/* ══ PoP functions ══ */

/* PoP: da10_defer_unless_expired @ plugins/platforms/discord/adapter.py:DiscordAdapter._defer_unless_expired */
static json_t *da10_defer_unless_expired(da10_ctx_t *ctx, const json_t *args) {
    json_t *a = da10_args();
    json_set(a, "ephemeral", json_bool(true));
    json_t *r = da10_call(ctx, "response.defer", a);
    if (!da10_is_raise(r)) { json_free(r); return json_bool(true); }
    json_t *orig = json_copy(r);   /* the caught exception object */
    if (da10_raise_is_base(r)) { json_free(orig); return r; }
    json_t *a2 = da10_args();
    json_set_hdr(a2, "error", json_get_str(r, "message", ""));
    json_t *u = da10_call(ctx, "_is_discord_unknown_interaction", a2);
    json_free(r);
    if (da10_is_raise(u)) { json_free(orig); return u; }
    bool unknown = da10_truthy(u);
    json_free(u);
    if (!unknown) return orig;      /* re-raise verbatim */
    json_free(orig);
    json_t *a3 = da10_args();
    da10_set_s(a3, "fmt", json_get_str(args, "warn_fmt", ""));
    da10_set_copy(a3, "args", json_obj_get(args, "warn_args"));
    json_t *w = da10_call(ctx, "logger.warning", a3);
    json_free(w);
    return json_bool(false);
}

/* Recovery-store helper: seam the entire store call. op/default/message_id
 * are projected into the seam args; the scripted result is the DB outcome. */
static json_t *da10_recovery(da10_ctx_t *ctx, const char *op, const char *mid, json_t *default_v) {
    json_t *a = da10_args();
    da10_set_s(a, "op", op);
    da10_set_s(a, "message_id", mid);
    json_set(a, "default", default_v ? default_v : json_null());
    return da10_call(ctx, "recovery_call", a);
}

/* PoP: da10_discord_message_is_persistently_complete @ plugins/platforms/discord/adapter.py:DiscordAdapter._discord_message_is_persistently_complete */
static json_t *da10_discord_message_is_persistently_complete(da10_ctx_t *ctx, const json_t *args) {
    const char *mid = da10_str_or(json_obj_get(args, "message_id"), "");
    if (!mid[0]) return json_bool(false);
    json_t *r = da10_recovery(ctx, "persistently_complete", mid, json_bool(false));
    if (da10_is_raise(r)) return r;
    bool v = da10_truthy(r);
    json_free(r);
    return json_bool(v);
}

/* PoP: da10_discord_message_has_active_claim @ plugins/platforms/discord/adapter.py:DiscordAdapter._discord_message_has_active_claim */
static json_t *da10_discord_message_has_active_claim(da10_ctx_t *ctx, const json_t *args) {
    const char *mid = da10_str_or(json_obj_get(args, "message_id"), "");
    if (!mid[0]) return json_bool(false);
    json_t *c = da10_call(ctx, "datetime_cutoff", da10_args());
    if (da10_is_raise(c)) return c;
    json_free(c);
    json_t *r = da10_recovery(ctx, "has_active_claim", mid, json_bool(true));
    if (da10_is_raise(r)) return r;
    bool v = da10_truthy(r);
    json_free(r);
    return json_bool(v);
}

/* PoP: da10_discord_message_attempts_exhausted @ plugins/platforms/discord/adapter.py:DiscordAdapter._discord_message_attempts_exhausted */
static json_t *da10_discord_message_attempts_exhausted(da10_ctx_t *ctx, const json_t *args) {
    const char *mid = da10_str_or(json_obj_get(args, "message_id"), "");
    if (!mid[0]) return json_bool(false);
    json_t *cap = da10_call(ctx, "max_attempts", da10_args());
    if (da10_is_raise(cap)) return cap;
    double capnum = da10_num(cap, 3);
    json_free(cap);
    json_t *r = da10_recovery(ctx, "attempts_exhausted", mid, json_bool(false));
    if (da10_is_raise(r)) return r;
    bool exhausted = da10_truthy(r);
    if (exhausted) {
        json_t *a = da10_args();
        da10_set_s(a, "fmt", "[%s] Not re-dispatching Discord message %s: missed_message_backfill.max_attempts (%d) reached");
        json_t *lst = json_array();
        json_append(lst, json_string(da10_str_or(json_obj_get(args, "name"), "")));
        json_append(lst, json_string(mid));
        json_append(lst, json_number(capnum));
        json_set(a, "args", lst);
        json_t *d = da10_call(ctx, "logger.debug", a);
        json_free(d);
    } else {
        json_free(r);
        return json_bool(false);
    }
    json_free(r);
    return json_bool(true);
}

/* PoP: da10_discord_recovery_cursor @ plugins/platforms/discord/adapter.py:DiscordAdapter._discord_recovery_cursor */
static json_t *da10_discord_recovery_cursor(da10_ctx_t *ctx, const json_t *args) {
    const char *cid = da10_str_or(json_obj_get(args, "channel_id"), "");
    if (!cid[0]) return json_null();
    json_t *r = da10_recovery(ctx, "recovery_cursor", cid, json_null());
    if (da10_is_raise(r)) return r;
    return r;  /* str or None; caller frees */
}

/* PoP: da10_dispatch_discord_message @ plugins/platforms/discord/adapter.py:DiscordAdapter._dispatch_discord_message */
static json_t *da10_dispatch_discord_message(da10_ctx_t *ctx, const json_t *args) {
    bool ready = da10_bool(json_obj_get(args, "ready"), true);
    if (!ready) {
        json_t *w = da10_args();
        json_set(w, "timeout", json_number(30.0));
        json_t *r = da10_call(ctx, "wait_for_ready", w);
        if (da10_is_raise(r)) {
            if (da10_raise_is_base(r)) return r;
            if (!da10_raise_is_timeout(r)) return r; /* other errors propagate */
            json_free(r);
        } else json_free(r);
    }
    const char *mid = da10_str_or(json_obj_get(args, "message_id"), "");
    json_t *aa = da10_args();
    da10_set_s(aa, "message_id", mid);
    json_set(aa, "claim", json_bool(true));
    json_t *adm = da10_call(ctx, "admission", aa);
    if (da10_is_raise(adm)) return adm;
    bool admitted = false, role = false;
    if (adm->type == JSON_ARRAY && json_len(adm) >= 2) {
        admitted = da10_truthy(json_get(adm, 0));
        role = da10_truthy(json_get(adm, 1));
    }
    json_free(adm);
    if (!admitted) return json_bool(false);
    json_t *da = da10_args();
    da10_set_s(da, "message_id", mid);
    json_t *rec = da10_call(ctx, "record_bot_tag_debounce", da);
    if (da10_is_raise(rec)) return rec;
    json_free(rec);
    json_t *ha = da10_args();
    da10_set_s(ha, "message_id", mid);
    json_set(ha, "role_authorized", json_bool(role));
    return da10_call(ctx, "handle_message", ha);
}

/* PoP: da10_dispatch_recovered_message @ plugins/platforms/discord/adapter.py:DiscordAdapter._dispatch_recovered_message */
static json_t *da10_dispatch_recovered_message(da10_ctx_t *ctx, const json_t *args) {
    bool is_dm = da10_bool(json_obj_get(args, "is_dm"), false);
    const char *mid = da10_str_or(json_obj_get(args, "message_id"), "");
    if (!is_dm) {
        json_t *pa = da10_args();
        da10_set_s(pa, "channel_id", da10_str_or(json_obj_get(args, "channel_id"), ""));
        json_t *par = da10_call(ctx, "get_parent_channel_id", pa);
        if (da10_is_raise(par)) return par;
        const char *parent = da10_str(par);
        json_t *ka = da10_args();
        da10_set_s(ka, "message_id", mid);
        da10_set_s(ka, "parent_id", parent ? parent : "");
        json_t *keys = da10_call(ctx, "channel_keys", ka);
        if (da10_is_raise(keys)) return keys;
        json_t *fr = da10_call(ctx, "free_channels", da10_args());
        if (da10_is_raise(fr)) return fr;
        json_t *rm = da10_call(ctx, "require_mention", da10_args());
        if (da10_is_raise(rm)) return rm;
        bool req = da10_truthy(rm);
        json_free(rm);
        bool veto = false;
        if (req) {
            bool star = false;
            for (size_t i = 0; i < json_len(fr); i++)
                if (strcmp(da10_str_or(json_get(fr, i), ""), "*") == 0) star = true;
            if (!star) {
                bool overlap = false;
                for (size_t i = 0; i < json_len(fr); i++) {
                    const char *fc = da10_str_or(json_get(fr, i), "");
                    for (size_t j = 0; j < json_len(keys); j++)
                        if (strcmp(fc, da10_str_or(json_get(keys, j), "")) == 0) overlap = true;
                }
                if (!overlap) {
                    json_t *ba = da10_args();
                    da10_set_s(ba, "message_id", mid);
                    json_t *bt = da10_call(ctx, "in_bot_thread", ba);
                    if (da10_is_raise(bt)) { json_free(fr); json_free(keys); json_free(par); return bt; }
                    bool in_thread = da10_truthy(bt); json_free(bt);
                    if (!in_thread) {
                        json_t *sa = da10_args();
                        da10_set_s(sa, "message_id", mid);
                        json_t *sm = da10_call(ctx, "self_is_explicitly_mentioned", sa);
                        if (da10_is_raise(sm)) { json_free(fr); json_free(keys); json_free(par); return sm; }
                        bool mentioned = da10_truthy(sm); json_free(sm);
                        if (!mentioned) veto = true;
                    }
                }
            }
        }
        json_free(fr); json_free(keys); json_free(par);
        if (veto) return json_bool(false);
    }
    json_t *aa = da10_args();
    da10_set_s(aa, "message_id", mid);
    json_set(aa, "claim", json_bool(false));
    json_t *adm = da10_call(ctx, "admission", aa);
    if (da10_is_raise(adm)) return adm;
    bool admitted = false, role = false;
    if (adm->type == JSON_ARRAY && json_len(adm) >= 2) {
        admitted = da10_truthy(json_get(adm, 0)); role = da10_truthy(json_get(adm, 1));
    }
    json_free(adm);
    if (!admitted) return json_bool(false);
    json_t *ha = da10_args();
    da10_set_s(ha, "message_id", mid);
    json_set(ha, "role_authorized", json_bool(role));
    json_set(ha, "recovered", json_bool(true));
    return da10_call(ctx, "handle_message", ha);
}

/* PoP: da10_dispatch_thread_session @ plugins/platforms/discord/adapter.py:DiscordAdapter._dispatch_thread_session */
static json_t *da10_dispatch_thread_session(da10_ctx_t *ctx, const json_t *args) {
    const char *tid = da10_str_or(json_obj_get(args, "thread_id"), "");
    const char *uid = da10_str_or(json_obj_get(args, "user_id"), "");
    const char *uname = da10_str_or(json_obj_get(args, "user_name"), "");
    const char *text = da10_str_or(json_obj_get(args, "text"), "");
    /* source = self.build_source(...) */
    json_t *pa = da10_args(); da10_set_s(pa, "thread_id", tid);
    json_t *par = da10_call(ctx, "get_parent_channel_id", pa);
    if (da10_is_raise(par)) return par;
    const char *parent = da10_str(par);
    json_t *na = da10_args(); da10_set_s(na, "thread_id", tid);
    json_t *name = da10_call(ctx, "format_thread_chat_name", na);
    if (da10_is_raise(name)) { json_free(par); return name; }
    json_t *ta = da10_args(); da10_set_s(ta, "thread_id", tid);
    json_t *topic = da10_call(ctx, "get_effective_topic", ta);
    if (da10_is_raise(topic)) { json_free(name); json_free(par); return topic; }
    json_t *ga = da10_call(ctx, "interaction_guild_id", da10_args());
    if (da10_is_raise(ga)) { json_free(topic); json_free(name); json_free(par); return ga; }
    json_t *ra = da10_call(ctx, "slash_role_grant", da10_args());
    if (da10_is_raise(ra)) { json_free(ga); json_free(topic); json_free(name); json_free(par); return ra; }
    bool role = da10_truthy(ra); json_free(ra);
    json_t *sa = da10_args();
    da10_set_s(sa, "chat_id", tid);
    da10_set_s(sa, "chat_name", da10_str_or(name, ""));
    da10_set_s(sa, "chat_type", "thread");
    da10_set_s(sa, "user_id", uid);
    da10_set_s(sa, "user_name", uname);
    da10_set_s(sa, "thread_id", tid);
    da10_set_s(sa, "chat_topic", da10_str_or(topic, ""));
    da10_set_s(sa, "guild_id", da10_str_or(ga, ""));
    if (parent && parent[0]) da10_set_s(sa, "parent_chat_id", parent);
    else json_set(sa, "parent_chat_id", json_null());
    json_set(sa, "role_authorized", json_bool(role));
    json_t *src = da10_call(ctx, "build_source", sa);
    if (da10_is_raise(src)) { json_free(ga); json_free(topic); json_free(name); json_free(par); return src; }
    json_t *ka = da10_args();
    da10_set_s(ka, "thread_id", tid); da10_set_s(ka, "parent_id", parent ? parent : "");
    json_t *skills = da10_call(ctx, "resolve_channel_skills", ka);
    if (da10_is_raise(skills)) { json_free(src); json_free(ga); json_free(topic); json_free(name); json_free(par); return skills; }
    json_t *ka2 = da10_args();
    da10_set_s(ka2, "thread_id", tid); da10_set_s(ka2, "parent_id", parent ? parent : "");
    json_t *prompt = da10_call(ctx, "thread_channel_prompt", ka2);
    if (da10_is_raise(prompt)) { json_free(skills); json_free(src); json_free(ga); json_free(topic); json_free(name); json_free(par); return prompt; }
    json_t *ea = da10_args();
    da10_set_s(ea, "text", text);
    da10_set_s(ea, "message_type", "TEXT");
    json_set(ea, "source", json_copy(src));
    json_set(ea, "auto_skill", json_copy(skills));
    if (da10_is_null(prompt)) json_set(ea, "channel_prompt", json_null());
    else json_set(ea, "channel_prompt", json_copy(prompt));
    json_t *ev = da10_call(ctx, "MessageEvent", ea);
    if (da10_is_raise(ev)) { json_free(prompt); json_free(skills); json_free(src); json_free(ga); json_free(topic); json_free(name); json_free(par); return ev; }
    json_t *ha = da10_args(); json_set(ha, "event", json_copy(ev));
    json_t *ret = da10_call(ctx, "handle_message", ha);
    json_free(ev); json_free(prompt); json_free(skills); json_free(src);
    json_free(ga); json_free(topic); json_free(name); json_free(par);
    return ret;
}

/* PoP: da10_create_thread @ plugins/platforms/discord/adapter.py:DiscordAdapter._create_thread */
static json_t *da10_create_thread(da10_ctx_t *ctx, const json_t *args) {
    const char *name = da10_strip_inplace(da10_str_or(json_obj_get(args, "name"), ""));
    char name_buf[4096]; snprintf(name_buf, sizeof name_buf, "%s", name); name = name_buf;
    if (!name[0]) {
        json_t *a = da10_args(); da10_set_s(a, "key", "platform.discord.command.thread.error_name_required");
        json_t *msg = da10_call(ctx, "t", a); if (da10_is_raise(msg)) return msg;
        json_t *out = json_object(); json_set(out, "error", json_copy(msg)); json_free(msg); return out;
    }
    int dur = (int)da10_num(json_obj_get(args, "auto_archive_duration"), 1440);
    int valid[] = {60, 1440, 4320, 10080};
    if (!da10_in_i(dur, valid, 4)) {
        json_t *a = da10_args(); da10_set_s(a, "key", "platform.discord.command.thread.error_invalid_archive");
        json_t *allowed = json_string("60, 1440, 4320, 10080"); json_set(a, "allowed", allowed);
        json_t *msg = da10_call(ctx, "t", a); if (da10_is_raise(msg)) return msg;
        json_t *out = json_object(); json_set(out, "error", json_copy(msg)); json_free(msg); return out;
    }
    json_t *ch = da10_call(ctx, "resolve_interaction_channel", da10_args());
    if (da10_is_raise(ch)) return ch;
    if (da10_is_null(ch)) { json_free(ch);
        json_t *a = da10_args(); da10_set_s(a, "key", "platform.discord.command.thread.error_no_channel");
        json_t *msg = da10_call(ctx, "t", a); if (da10_is_raise(msg)) return msg;
        json_t *out = json_object(); json_set(out, "error", json_copy(msg)); json_free(msg); return out; }
    bool is_dm = da10_bool(json_obj_get(ch, "is_dm"), false);
    if (is_dm) { json_free(ch);
        json_t *a = da10_args(); da10_set_s(a, "key", "platform.discord.command.thread.error_dm_not_supported");
        json_t *msg = da10_call(ctx, "t", a); if (da10_is_raise(msg)) return msg;
        json_t *out = json_object(); json_set(out, "error", json_copy(msg)); json_free(msg); return out; }
    json_t *pa = da10_args(); json_set(pa, "channel", json_copy(ch));
    json_t *parent = da10_call(ctx, "thread_parent_channel", pa);
    json_free(ch);
    if (da10_is_raise(parent)) return parent;
    if (da10_is_null(parent)) { json_free(parent);
        json_t *a = da10_args(); da10_set_s(a, "key", "platform.discord.command.thread.error_no_parent");
        json_t *msg = da10_call(ctx, "t", a); if (da10_is_raise(msg)) return msg;
        json_t *out = json_object(); json_set(out, "error", json_copy(msg)); json_free(msg); return out; }
    const char *disp = da10_str_or(json_obj_get(json_obj_get(args, "interaction"), "display_name"), "unknown user");
    char reason[4200]; snprintf(reason, sizeof reason, "Requested by %s via /thread", disp);
    const char *seed = da10_strip_inplace(da10_str_or(json_obj_get(args, "message"), ""));
    char seed_buf[4096]; snprintf(seed_buf, sizeof seed_buf, "%s", seed); seed = seed_buf;
    /* direct attempt */
    json_t *da = da10_args();
    da10_set_s(da, "name", name); json_set(da, "auto_archive_duration", json_number((double)dur));
    da10_set_s(da, "reason", reason);
    json_t *thread = da10_call(ctx, "create_thread", da);
    if (!da10_is_raise(thread)) {
        if (seed[0]) { json_t *sa = da10_args(); da10_set_s(sa, "content", seed); json_t *sr = da10_call(ctx, "thread.send", sa); if (da10_is_raise(sr)) { json_free(thread); return sr; } json_free(sr); }
        json_t *ca = da10_args(); json_set(ca, "thread", json_copy(thread)); da10_set_s(ca, "name", name);
        json_t *ret = da10_call(ctx, "thread_created", ca); json_free(thread); json_free(parent); return ret;
    }
    json_free(thread);
    /* fallback attempt */
    const char *seed_content = seed[0] ? seed : "platform.discord.thread.seed_message";
    json_t *sa = da10_args(); da10_set_s(sa, "content", seed_content);
    json_t *seed_msg = da10_call(ctx, "seed.send", sa);
    if (da10_is_raise(seed_msg)) { json_free(parent); return seed_msg; }
    json_t *ca = da10_args(); json_set(ca, "message", json_copy(seed_msg));
    json_t *fb = da10_call(ctx, "seed.create_thread", ca);
    json_free(seed_msg);
    if (da10_is_raise(fb)) { json_free(parent); return fb; }
    json_t *ca2 = da10_args(); json_set(ca2, "thread", json_copy(fb)); da10_set_s(ca2, "name", name);
    json_t *ret = da10_call(ctx, "thread_created", ca2); json_free(fb); json_free(parent); return ret;
}

/* PoP: da10_deliver_unauthorized_slash_alert @ plugins/platforms/discord/adapter.py:DiscordAdapter._deliver_unauthorized_slash_alert */
static json_t *da10_deliver_unauthorized_slash_alert(da10_ctx_t *ctx, const json_t *args) {
    json_t *aa = da10_args();
    da10_set_copy(aa, "runner", json_obj_get(args, "runner"));
    da10_set_copy(aa, "profile", json_obj_get(args, "profile"));
    json_t *ac = da10_call(ctx, "alert_adapters_and_config", aa);
    if (da10_is_raise(ac)) {
        json_t *d = da10_args(); da10_set_s(d, "fmt", "[Discord] Admin notify: profile %r resolution failed: %s");
        json_t *lst = json_array(); json_append(lst, json_copy(json_obj_get(args, "profile")));
        json_append(lst, json_string(json_get_str(ac, "message", "")));
        json_set(d, "args", lst);
        json_t *lg = da10_call(ctx, "logger.debug", d); json_free(lg); json_free(ac);
        return json_null();
    }
    json_free(ac);
    const char *targets[] = {"telegram", "slack"};
    for (size_t ti = 0; ti < 2; ti++) {
        json_t *ha = da10_args(); da10_set_s(ha, "target", targets[ti]);
        json_t *home = da10_call(ctx, "get_home_channel", ha);
        if (da10_is_raise(home)) { json_t *d = da10_args(); da10_set_s(d, "fmt", "[Discord] Admin notify via %s failed: %s");
            json_t *lst = json_array(); json_append(lst, json_string(targets[ti])); json_append(lst, json_string(json_get_str(home, "message", ""))); json_set(d, "args", lst);
            json_t *lg = da10_call(ctx, "logger.debug", d); json_free(lg); json_free(home); continue; }
        if (da10_is_null(home) || !da10_truthy(json_obj_get(home, "chat_id"))) { json_free(home); continue; }
        json_t *ta = da10_args(); da10_set_s(ta, "key", "platform.discord.security.unauthorized_slash_alert");
        da10_set_copy(ta, "user_name", json_obj_get(args, "user_name"));
        da10_set_copy(ta, "user_id", json_obj_get(args, "user_id"));
        da10_set_copy(ta, "chan_id", json_obj_get(args, "chan_id"));
        da10_set_copy(ta, "guild_id", json_obj_get(args, "guild_id"));
        da10_set_copy(ta, "command_text", json_obj_get(args, "command_text"));
        da10_set_copy(ta, "reason", json_obj_get(args, "reason"));
        json_t *msg = da10_call(ctx, "t", ta);
        if (da10_is_raise(msg)) { json_free(home); return msg; }
        json_t *pa = da10_args(); da10_set_s(pa, "platform", targets[ti]); json_set(pa, "diagnostic", json_bool(true));
        json_t *ok = da10_call(ctx, "present_notification", pa);
        if (da10_is_raise(ok)) { json_free(msg); json_free(home); return ok; }
        bool present = da10_truthy(ok); json_free(ok);
        if (!present) { json_free(msg); json_free(home); return json_null(); }
        json_t *sa = da10_args(); da10_set_s(sa, "chat_id", json_get_str(home, "chat_id", ""));
        json_set(sa, "msg", json_copy(msg));
        json_t *res = da10_call(ctx, "adapter.send", sa);
        json_free(msg); json_free(home);
        if (da10_is_raise(res)) {
            json_t *d = da10_args(); da10_set_s(d, "fmt", "[Discord] Admin notify via %s failed: %s");
            json_t *lst = json_array(); json_append(lst, json_string(targets[ti])); json_append(lst, json_string(json_get_str(res, "message", ""))); json_set(d, "args", lst);
            json_t *lg = da10_call(ctx, "logger.debug", d); json_free(lg); json_free(res); continue;
        }
        if (json_obj_get(res, "success") && !da10_bool(json_obj_get(res, "success"), true)) {
            json_t *d = da10_args(); da10_set_s(d, "fmt", "[Discord] Admin notify via %s returned success=False (error=%r); falling through");
            json_t *l2 = json_array(); json_append(l2, json_string(targets[ti]));
            json_t *erv = json_obj_get(res, "error"); json_append(l2, erv ? json_copy(erv) : json_null());
            json_set(d, "args", l2);
            json_t *lg = da10_call(ctx, "logger.debug", d); json_free(lg); json_free(res); continue;
        }
        json_free(res);
        return json_null();
    }
    return json_null();
}

/* PoP: da10_desired_command_sync_fingerprint @ plugins/platforms/discord/adapter.py:DiscordAdapter._desired_command_sync_fingerprint */
static json_t *da10_desired_command_sync_fingerprint(da10_ctx_t *ctx, const json_t *args) {
    bool client = da10_bool(json_obj_get(args, "client"), false);
    json_t *desired = json_array();
    if (client) {
        json_t *tr = da10_call(ctx, "client_tree", da10_args());
        if (da10_is_raise(tr)) { json_free(desired); return tr; }
        if (!da10_is_null(tr)) {
            json_t *cmds = da10_call(ctx, "get_commands", da10_args());
            if (da10_is_raise(cmds)) { json_free(tr); json_free(desired); return cmds; }
            for (size_t i = 0; i < json_len(cmds); i++) {
                json_t *ca = da10_args(); json_set(ca, "command", json_copy(json_get(cmds, i)));
                json_t *d = da10_call(ctx, "to_dict", ca);
                if (da10_is_raise(d)) { json_free(cmds); json_free(tr); json_free(desired); return d; }
                json_t *na = da10_args(); json_set(na, "command", json_copy(d)); json_free(d);
                json_t *canon = da10_call(ctx, "canonicalize_command", na);
                if (da10_is_raise(canon)) { json_free(cmds); json_free(tr); json_free(desired); return canon; }
                json_append(desired, canon);
            }
            json_free(cmds);
        }
        json_free(tr);
    }
    /* sort by (type default 1, name default "") — stable selection sort */
    size_t n = json_len(desired);
    unsigned char used[4096];
    for (size_t i = 0; i < n && i < 4096; i++) used[i] = 0;
    json_t *sorted = json_array();
    for (size_t k = 0; k < n && k < 4096; k++) {
        size_t best = n;
        for (size_t j = 0; j < n && j < 4096; j++) {
            if (used[j]) continue;
            if (best == n) { best = j; continue; }
            json_t *a = json_get(desired, j), *b = json_get(desired, best);
            int ta = (int)da10_num(json_obj_get(a, "type"), 1), tb = (int)da10_num(json_obj_get(b, "type"), 1);
            const char *na = da10_str_or(json_obj_get(a, "name"), ""), *nb = da10_str_or(json_obj_get(b, "name"), "");
            if (ta < tb || (ta == tb && strcmp(na, nb) < 0)) best = j;
        }
        if (best != n) { used[best] = 1; json_append(sorted, json_copy(json_get(desired, best))); }
    }
    json_free(desired); desired = sorted;
    json_t *lang = da10_call(ctx, "get_language", da10_args());
    if (da10_is_raise(lang)) return lang;
    json_t *payload = json_object();
    json_set(payload, "language", json_copy(lang)); json_free(lang);
    json_set(payload, "commands", json_copy(desired)); json_free(desired);
    char buf[32768]; size_t len = 0; buf[0] = '\0';
    da10_jdump(payload, buf, sizeof buf, &len); buf[len] = '\0';
    json_free(payload);
    json_t *sha = da10_args(); da10_set_s(sha, "payload", buf);
    json_t *ret = da10_call(ctx, "sha256_hex", sha);
    return ret;
}

/* PoP: da10_write_command_sync_state @ plugins/platforms/discord/adapter.py:DiscordAdapter._write_command_sync_state */
static json_t *da10_write_command_sync_state(da10_ctx_t *ctx, const json_t *args) {
    json_t *path = da10_call(ctx, "command_sync_state_path", da10_args());
    if (da10_is_raise(path)) return path;
    json_t *a = da10_args();
    da10_set_s(a, "path", da10_str_or(path, ""));
    da10_set_copy(a, "state", json_obj_get(args, "state"));
    json_set(a, "indent", json_null());
    json_t *sep = json_array(); json_append(sep, json_string(",")); json_append(sep, json_string(":")); json_set(a, "separators", sep);
    json_free(path);
    return da10_call(ctx, "atomic_json_write", a);
}

/* PoP: da10_sleep_between_command_sync_mutations @ plugins/platforms/discord/adapter.py:DiscordAdapter._sleep_between_command_sync_mutations */
static json_t *da10_sleep_between_command_sync_mutations(da10_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *iv = da10_call(ctx, "command_sync_mutation_interval_seconds", da10_args());
    if (da10_is_raise(iv)) return iv;
    double interval = da10_num(iv, 4.5); json_free(iv);
    if (interval > 0) {
        json_t *a = da10_args(); json_set(a, "delay", json_number(interval));
        json_t *r = da10_call(ctx, "asyncio.sleep", a);
        if (da10_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* PoP: da10_resolve_channel_prompt @ plugins/platforms/discord/adapter.py:DiscordAdapter._resolve_channel_prompt */
static json_t *da10_resolve_channel_prompt(da10_ctx_t *ctx, const json_t *args) {
    json_t *a = da10_args();
    da10_set_copy(a, "extra", json_obj_get(args, "extra"));
    da10_set_s(a, "channel_id", da10_str_or(json_obj_get(args, "channel_id"), ""));
    const json_t *pid = json_obj_get(args, "parent_id");
    if (da10_is_null(pid)) json_set(a, "parent_id", json_null());
    else da10_set_s(a, "parent_id", da10_str_or(pid, ""));
    return da10_call(ctx, "resolve_channel_prompt", a);
}

/* PoP: da10_define_discord_view_classes @ plugins/platforms/discord/adapter.py:_define_discord_view_classes */
static json_t *da10_define_discord_view_classes(da10_ctx_t *ctx, const json_t *args) {
    (void)ctx; (void)args;
    return json_null();  /* module-global registration; no seamed calls */
}

/* PoP: da10_discord_message_admission @ plugins/platforms/discord/adapter.py:DiscordAdapter._discord_message_admission */
static json_t *da10_discord_message_admission(da10_ctx_t *ctx, const json_t *args) {
    const char *mid = da10_str_or(json_obj_get(args, "message_id"), "");
    bool claim = da10_bool(json_obj_get(args, "claim"), true);
    const json_t *author = json_obj_get(args, "author");
    bool author_self = da10_bool(json_obj_get(args, "author_self"),
                                  da10_bool(json_obj_get(author, "is_self"), false));
    bool is_automated = da10_bool(json_obj_get(args, "is_automated"), false);
    int msg_type = (int)da10_num(json_obj_get(args, "type"), 0);
    bool type_ok = da10_in_i(msg_type, (int[]){0, 19}, 2);

    /* dedup gate */
    json_t *da = da10_args(); da10_set_s(da, "message_id", mid);
    json_t *dup = claim
        ? da10_call(ctx, "dedup.is_duplicate", da)
        : da10_call(ctx, "dedup.contains", da);
    if (da10_is_raise(dup)) return dup;
    if (da10_truthy(dup)) { json_free(dup); json_t *o = json_array(); json_append(o, json_bool(false)); json_append(o, json_bool(false)); return o; }
    json_free(dup);
    /* self author gate (live: message.author == self._client.user) */
    if (author_self) { json_t *o = json_array(); json_append(o, json_bool(false)); json_append(o, json_bool(false)); return o; }
    if (!type_ok) { json_t *o = json_array(); json_append(o, json_bool(false)); json_append(o, json_bool(false)); return o; }
    json_t *rf = da10_call(ctx, "refresh_live_gates", da10_args());
    if (da10_is_raise(rf)) return rf; json_free(rf);
    bool role = false;
    if (is_automated) {
        json_t *ab = da10_call(ctx, "get_allow_bots", da10_args());
        if (da10_is_raise(ab)) return ab;
        char allow_buf[256];
        snprintf(allow_buf, sizeof allow_buf, "%s", da10_str_or(ab, ""));
        json_free(ab);
        const char *allow = allow_buf;
        json_t *bt = da10_call(ctx, "is_bot_tag_continuation", da10_args());
        if (da10_is_raise(bt)) return bt; bool cont = da10_truthy(bt); json_free(bt);
        if (strcmp(allow, "none") == 0) { json_t *o = json_array(); json_append(o, json_bool(false)); json_append(o, json_bool(false)); return o; }
        if (strcmp(allow, "mentions") == 0) {
            json_t *sa = da10_args(); da10_set_s(sa, "message_id", mid);
            json_t *sm = da10_call(ctx, "self_is_explicitly_mentioned", sa);
            if (da10_is_raise(sm)) return sm; bool mentioned = da10_truthy(sm); json_free(sm);
            if (!mentioned && !cont) { json_t *o = json_array(); json_append(o, json_bool(false)); json_append(o, json_bool(false)); return o; }
        }
        json_t *rb = da10_call(ctx, "bots_require_inline_mention", da10_args());
        if (da10_is_raise(rb)) return rb; bool req = da10_truthy(rb); json_free(rb);
        if (req) {
            json_t *sa = da10_args(); da10_set_s(sa, "message_id", mid);
            json_t *raw = da10_call(ctx, "self_is_raw_mentioned", sa);
            if (da10_is_raise(raw)) return raw; bool rm = da10_truthy(raw); json_free(raw);
            if (!rm && !cont) { json_t *o = json_array(); json_append(o, json_bool(false)); json_append(o, json_bool(false)); return o; }
        }
    } else {
        bool is_dm = da10_bool(json_obj_get(args, "is_dm"), false);
        bool has_guild = !da10_is_null(json_obj_get(args, "guild"));
        is_dm = is_dm || !has_guild;
        json_t *keys = json_array();
        if (!is_dm) {
            const json_t *cvid = json_obj_get(args, "channel_id");
            if (da10_is_null(cvid)) cvid = json_obj_get(json_obj_get(args, "channel"), "id");
            json_append(keys, json_copy(cvid));
            json_t *pa = da10_args(); da10_set_s(pa, "channel_id",
                da10_str_or(json_obj_get(args, "channel_id"),
                            da10_str_or(json_obj_get(json_obj_get(args, "channel"), "id"), "")));
            json_t *parent = da10_call(ctx, "get_parent_channel_id", pa);
            if (da10_is_raise(parent)) { json_free(keys); return parent; }
            const char *pv = da10_str(parent);
            if (pv && pv[0]) json_append(keys, json_string(pv));
            json_free(parent);
        }
        json_t *ua = da10_args();
        da10_set_s(ua, "author_id",
            da10_str_or(json_obj_get(args, "author_id"), da10_str_or(json_obj_get(author, "id"), "")));
        json_set(ua, "is_dm", json_bool(is_dm));
        json_set(ua, "channel_ids", json_copy(keys));
        json_set(ua, "guild", json_copy(json_obj_get(args, "guild")));
        json_t *allowed = da10_call(ctx, "is_allowed_user", ua);
        if (da10_is_raise(allowed)) { json_free(keys); return allowed; }
        bool ok = da10_truthy(allowed); json_free(allowed); json_free(keys);
        if (!ok) {
            json_t *w = da10_call(ctx, "warn_if_fail_closed_default", da10_args());
            if (da10_is_raise(w)) return w; json_free(w);
            json_t *o = json_array(); json_append(o, json_bool(false)); json_append(o, json_bool(false)); return o;
        }
        role = da10_truthy(json_obj_get(args, "has_allowed_roles"));
    }
    /* raw self mention + other-bots gate (non-DM) */
    json_t *sa = da10_args(); da10_set_s(sa, "message_id", mid);
    json_t *raw = da10_call(ctx, "self_is_explicitly_mentioned", sa);
    if (da10_is_raise(raw)) return raw; bool raw_self = da10_truthy(raw); json_free(raw);
    bool is_dm_ch = da10_bool(json_obj_get(json_obj_get(args, "channel"), "is_dm"), false);
    const json_t *mentions = json_obj_get(args, "mentions");
    if (!is_dm_ch && (json_len(mentions) > 0 || raw_self)) {
        bool other_bots = false;
        for (size_t i = 0; i < json_len(mentions); i++) {
            const json_t *m = json_get(mentions, i);
            if (da10_bool(json_obj_get(m, "bot"), false) && !da10_bool(json_obj_get(m, "is_self"), false)) other_bots = true;
        }
        if (other_bots && !raw_self) { json_t *o = json_array(); json_append(o, json_bool(false)); json_append(o, json_bool(false)); return o; }
        json_t *ea = da10_args(); da10_set_s(ea, "key", "DISCORD_IGNORE_NO_MENTION"); da10_set_s(ea, "default", "true");
        json_t *env = da10_call(ctx, "scoped_gate_env", ea);
        if (da10_is_raise(env)) return env;
        char low[64]; snprintf(low, sizeof low, "%s", da10_str_or(env, ""));
        json_free(env);
        for (char *p = low; *p; ++p) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
        bool ignore = strcmp(low, "true") == 0 || strcmp(low, "1") == 0 || strcmp(low, "yes") == 0;
        if (ignore && !raw_self && !other_bots) {
            json_t *ba = da10_args(); da10_set_s(ba, "message_id", mid);
            json_t *bt = da10_call(ctx, "in_bot_thread", ba);
            if (da10_is_raise(bt)) return bt; bool in_thread = da10_truthy(bt); json_free(bt);
            if (!in_thread) {
                const char *parent_id = da10_str_or(json_obj_get(json_obj_get(args, "channel"), "parent_id"), "");
                json_t *fa = da10_args(); da10_set_s(fa, "parent_id", parent_id);
                json_t *free = da10_call(ctx, "free_response_channels", fa);
                if (da10_is_raise(free)) return free;
                json_t *ka = da10_args(); da10_set_s(ka, "message_id", mid); da10_set_s(ka, "parent_id", parent_id);
                json_t *keys = da10_call(ctx, "channel_keys", ka);
                if (da10_is_raise(keys)) { json_free(free); return keys; }
                bool star = false;
                for (size_t i = 0; i < json_len(free); i++) if (strcmp(da10_str_or(json_get(free, i), ""), "*") == 0) star = true;
                bool overlap = false;
                for (size_t i = 0; i < json_len(free); i++) for (size_t j = 0; j < json_len(keys); j++)
                    if (strcmp(da10_str_or(json_get(free, i), ""), da10_str_or(json_get(keys, j), "")) == 0) overlap = true;
                json_free(free); json_free(keys);
                if (!star && !overlap) {
                    json_t *d = da10_args(); da10_set_s(d, "fmt", "[%s] admission: dropping message %s — mentions others, not self, not a bot thread, channel not free-response");
                    json_t *lst = json_array(); json_append(lst, json_string(da10_str_or(json_obj_get(args, "name"), ""))); json_append(lst, json_string(mid)); json_set(d, "args", lst);
                    json_t *lg = da10_call(ctx, "logger.debug", d); json_free(lg);
                    json_t *o = json_array(); json_append(o, json_bool(false)); json_append(o, json_bool(false)); return o;
                }
            }
        }
    }
    json_t *out = json_array(); json_append(out, json_bool(true)); json_append(out, json_bool(role)); return out;
}
