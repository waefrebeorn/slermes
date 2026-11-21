/*
 * discord_adapter_flow12.c — Discord adapter "remainders" FLOW cores of
 * plugins/platforms/discord/adapter.py (hermes-agent).
 *
 * TWELFTH remainder port beyond discord_adapter_flow.c .. flow11. Ports 16
 * genuinely unclaimed defs (dotted-name-aware claim parser confirmed none are
 * claimed by flows 1-11):
 *   _is_forum_parent, _iter_channel_and_thread_messages,
 *   _iter_missed_message_backfill_candidates, _lead_silence_bytes,
 *   _liveness_loop, _load, _load_voice_fx_config,
 *   _looks_like_nonconversational_history_message,
 *   _message_has_non_down_bot_response, _missed_message_backfill_channels,
 *   _native_slash_commands, _notify_liveness_fatal_error,
 *   _notify_unauthorized_slash, _is_length_overflow_error,
 *   _normalize_permissions, _message_reference_from_ids.
 *
 * Universal seam: every seamed call enters through ONE callback
 * da12_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting the
 * identical {"name","args","ret"} event from the SAME scripts. args is ADOPTED
 * into the trace event; the return is a NEW owned json_t (NULL == Python None;
 * {"__raise__":cls,"message":...} == a raised exception). A missing/exhausted
 * script is a fail-closed RuntimeError on both sides.
 *
 * FULL BASEEXCEPTION PASSTHROUGH: da12_is_base_exc + da12_raise_is_base name the
 * family that bypasses `except Exception`; those raises propagate verbatim.
 *
 * Representation contract (shared with tests/oracle_discord_adapter_flow12.py):
 * - NULL json_t* == Python None; JSON_NULL node is a real null value.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__":cls,"message":str(exc)}; a scripted raise is
 *   {"__script__":true,"raise":{"type":cls,"message":msg}}.
 * - bytes results are represented as {"zeros":N} (N NUL bytes); b"" -> {"zeros":0}.
 * - "sets" are represented as JSON arrays of strings; set union is a sorted
 *   de-duplicated array on both sides (oracle canon sorts Python sets).
 * - Out.state is {} — all observable behavior is ret + ordered trace.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and da12_-prefixed.
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
typedef struct da12_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} da12_ctx_t;

/* ── basic helpers ── */
static bool da12_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool da12_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool da12_is_base_exc(const char *kind) {
    return kind && (
        strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "asyncio.CancelledError") == 0);
}
static bool da12_raise_is_base(const json_t *r) {
    return da12_is_raise(r) && da12_is_base_exc(json_get_str(r, "__raise__", ""));
}
static bool da12_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *da12_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static double da12_num(const json_t *v, double d) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : d;
}
static bool da12_bool(const json_t *v, bool d) {
    return (v && v->type == JSON_BOOL) ? v->bool_val : d;
}
static const char *da12_str_or(const json_t *v, const char *d) {
    const char *s = da12_str(v);
    return s ? s : d;
}
static json_t *da12_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *da12_make_exc(const json_t *spec) {
    return da12_err(json_get_str(spec, "type", "RuntimeError"),
                    json_get_str(spec, "message", ""));
}
/* The one universal seam. args is ADOPTED; the return is a fresh owned node. */
static json_t *da12_call(da12_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = da12_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = da12_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = da12_make_exc(raise);
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
static json_t *da12_args(void) { return json_object(); }
static void da12_set_s(json_t *a, const char *k, const char *v) {
    json_set_hdr(a, k, v ? v : "");
}
static void da12_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
/* Python str.strip() for the ASCII fixture domain. */
static char *da12_strip_inplace(const char *s) {
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
/* Python str(value) for the scalar fixture domain. */
static void da12_pystr(const json_t *v, char *out, size_t cap) {
    if (!v || !cap) { if (cap) out[0] = '\0'; return; }
    switch (v->type) {
        case JSON_STRING: snprintf(out, cap, "%s", v->str_val ? v->str_val : ""); break;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val) snprintf(out, cap, "%.0f", v->num_val);
            else snprintf(out, cap, "%.17g", v->num_val);
            break;
        case JSON_BOOL: snprintf(out, cap, "%s", v->bool_val ? "True" : "False"); break;
        case JSON_NULL: snprintf(out, cap, "None"); break;
        default: out[0] = '\0'; break;
    }
}
static bool da12_list_has_str(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        char buf[256];
        da12_pystr(json_get(arr, i), buf, sizeof buf);
        if (strcmp(buf, s) == 0) return true;
    }
    return false;
}
static bool da12_list_overlap(const json_t *a, const json_t *b) {
    if (!a || a->type != JSON_ARRAY || !b || b->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(a); i++) {
        char buf[256];
        da12_pystr(json_get(a, i), buf, sizeof buf);
        if (da12_list_has_str(b, buf)) return true;
    }
    return false;
}
static json_t *da12_send_result(bool success, const char *error, const char *mid,
                                json_t *cont_ids, json_t *raw) {
    json_t *o = json_object();
    json_set(o, "success", json_bool(success));
    json_set(o, "error", error ? json_string(error) : json_null());
    json_set(o, "message_id", mid ? json_string(mid) : json_null());
    json_set(o, "continuation_message_ids", cont_ids ? cont_ids : json_null());
    json_set(o, "raw_response", raw ? raw : json_null());
    return o;
}
/* Convert a raise to the "soft" outcome of a try/except Exception: a
 * BaseException is returned via *prop; any other exception is swallowed. */
static json_t *da12_soft(json_t *r, json_t **prop) {
    *prop = NULL;
    if (da12_is_raise(r)) {
        if (da12_raise_is_base(r)) { *prop = r; return NULL; }
        json_free(r);
        return NULL;
    }
    return r;
}
/* ── flow12 extra helpers ── */
static void da12_set_num(json_t *a, const char *k, double v) {
    json_set(a, k, json_number(v));
}
static void da12_set_boolv(json_t *a, const char *k, bool v) {
    json_set(a, k, json_bool(v));
}
static json_t *da12_log_args(const char *fmt, json_t *arr) {
    json_t *o = da12_args();
    da12_set_s(o, "fmt", fmt);
    json_set(o, "args", arr ? arr : json_array());
    return o;
}
/* int(value) for the scalar fixture domain. */
static long long da12_to_int(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) {
        char *end = NULL;
        long long n = strtoll(v->str_val ? v->str_val : "", &end, 10);
        while (*end == ' ' || *end == '\t') end++;
        return (*end == '\0') ? n : 0;
    }
    return 0;
}
/* Python str.strip() truthiness for a string-ish value. */
static bool da12_scalar_strip_nonempty(const json_t *v) {
    if (!v || v->type != JSON_STRING) return false;
    const char *s = da12_strip_inplace(v->str_val);
    return s[0] != '\0';
}
/* Sorted, de-duplicated union of two string arrays. */
static json_t *da12_union_strings(const json_t *a, const json_t *b) {
    const char *items[512]; size_t n = 0;
    for (int pass = 0; pass < 2; pass++) {
        const json_t *arr = pass == 0 ? a : b;
        if (!arr || arr->type != JSON_ARRAY) continue;
        for (size_t i = 0; i < json_len(arr) && n < 512; i++) {
            char buf[256];
            da12_pystr(json_get(arr, i), buf, sizeof buf);
            bool seen = false;
            for (size_t j = 0; j < n; j++) if (strcmp(items[j], buf) == 0) { seen = true; break; }
            if (!seen) {
                items[n] = strdup(buf);
                n++;
            }
        }
    }
    /* insertion sort (copy of ptrs) */
    for (size_t i = 1; i < n; i++) {
        const char *k = items[i]; size_t j = i;
        while (j > 0 && strcmp(items[j-1], k) > 0) { items[j] = items[j-1]; j--; }
        items[j] = k;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) { json_append(out, json_string(items[i])); free((void*)items[i]); }
    return out;
}

/* ══ PoP functions (stage 1) ══ */

/* PoP: da12_is_forum_parent @ plugins/platforms/discord/adapter.py:DiscordAdapter._is_forum_parent */
static json_t *da12_is_forum_parent(da12_ctx_t *ctx, const json_t *args) {
    const json_t *ch = json_obj_get(args, "channel");
    if (da12_is_null(ch)) return json_bool(false);
    json_t *fc = da12_call(ctx, "ForumChannel", da12_args());
    if (da12_is_raise(fc)) return fc;
    json_t *ia = da12_args();
    da12_set_copy(ia, "channel", ch);
    da12_set_copy(ia, "cls", fc);
    json_t *iv = da12_call(ctx, "isinstance_forum", ia);
    if (da12_is_raise(iv)) { json_free(fc); return iv; }
    bool isf = da12_truthy(iv);
    json_free(fc); json_free(iv);
    if (isf) return json_bool(true);
    json_t *ct = da12_call(ctx, "channel_type", da12_args());
    if (da12_is_raise(ct)) return ct;
    if (!da12_is_null(ct)) {
        json_t *ta = da12_args();
        da12_set_copy(ta, "channel_type", ct);
        json_t *tv = da12_call(ctx, "type_value", ta);
        json_free(ct);
        if (da12_is_raise(tv)) return tv;
        bool eq = (tv->type == JSON_NUMBER && tv->num_val == 15.0);
        json_free(tv);
        if (eq) return json_bool(true);
    } else if (ct) json_free(ct);
    return json_bool(false);
}

/* PoP: da12_load @ plugins/platforms/discord/adapter.py:_DiscordNonConversationalMessageTracker._load */
static json_t *da12_load(da12_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *p = da12_call(ctx, "state_path", da12_args());
    if (da12_is_raise(p)) return p;
    json_t *ea = da12_args();
    da12_set_copy(ea, "path", p);
    json_free(p);
    json_t *ex = da12_call(ctx, "exists", ea);
    if (da12_is_raise(ex)) return ex;
    if (!da12_truthy(ex)) { json_free(ex); return json_array(); }
    json_free(ex);
    json_t *ra = da12_args();
    da12_set_s(ra, "encoding", "utf-8-sig");
    json_t *txt = da12_call(ctx, "read_text", ra);
    if (da12_is_raise(txt)) {
        if (da12_raise_is_base(txt)) return txt;
        json_free(txt);
        json_t *lg = da12_call(ctx, "logger.debug", da12_log_args(
            "[%s] Failed to load non-conversational Discord IDs", (json_t*)NULL));
        if (da12_is_raise(lg)) return lg;
        json_free(lg);
        return json_array();
    }
    const char *s = da12_str(txt);
    json_t *data = s ? json_parse(s, NULL) : NULL;
    json_free(txt);
    json_t *out = json_array();
    if (!data) {
        /* json.loads raised -> except Exception -> logger.debug */
        json_t *lg = da12_call(ctx, "logger.debug", da12_log_args(
            "[%s] Failed to load non-conversational Discord IDs", (json_t*)NULL));
        if (da12_is_raise(lg)) return lg;
        json_free(lg);
        return out;
    }
    if (data->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(data); i++) {
            char buf[512];
            da12_pystr(json_get(data, i), buf, sizeof buf);
            char *t = da12_strip_inplace(buf);
            if (t[0]) json_append(out, json_string(t));
        }
    }
    if (data) json_free(data);
    return out;
}

/* PoP: da12_load_voice_fx_config @ plugins/platforms/discord/adapter.py:DiscordAdapter._load_voice_fx_config */
static json_t *da12_load_voice_fx_config(da12_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *d = json_object();
    json_set(d, "enabled", json_bool(false));
    json_set(d, "ambient_enabled", json_bool(true));
    json_set(d, "ambient_path", json_string(""));
    json_set(d, "ambient_gain", json_number(0.18));
    json_set(d, "duck_gain", json_number(0.06));
    json_set(d, "speech_gain", json_number(1.0));
    json_set(d, "lead_silence_ms", json_number(200));
    json_set(d, "ack_enabled", json_bool(true));
    json_set(d, "ack_phrases", json_null());
    json_t *cfg = da12_call(ctx, "read_raw_config", da12_args());
    if (da12_is_raise(cfg)) {
        if (da12_raise_is_base(cfg)) { json_free(d); return cfg; }
        json_free(cfg);
        json_t *lg = da12_call(ctx, "logger.debug", da12_log_args(
            "Could not load discord.voice_fx config: %s", (json_t*)NULL));
        if (da12_is_raise(lg)) { json_free(d); return lg; }
        json_free(lg);
        return d;
    }
    const json_t *discord = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "discord") : NULL;
    const json_t *fx = (discord && discord->type == JSON_OBJECT) ? json_obj_get(discord, "voice_fx") : NULL;
    if (fx && fx->type == JSON_OBJECT) {
        for (size_t i = 0; i < fx->c.count; i++) {
            const char *k = json_object_get_key_at(fx, i);
            json_t *v = json_object_get_at(fx, i);
            if (!k || !v || da12_is_null(v)) continue;
            /* only keys already present in defaults may be overridden */
            if (json_obj_get(d, k)) { json_set(d, k, json_copy(v)); }
        }
    }
    json_free(cfg);
    return d;
}

/* PoP: da12_looks_like_nonconversational_history_message @ plugins/platforms/discord/adapter.py:_looks_like_nonconversational_history_message */
static json_t *da12_looks_like_nonconversational_history_message(da12_ctx_t *ctx, const json_t *args) {
    const json_t *cv = json_obj_get(args, "content");
    const char *text = da12_str_or(cv, "");
    json_t *pats = da12_call(ctx, "history_patterns", da12_args());
    if (da12_is_raise(pats)) return pats;
    bool any = false;
    if (pats && pats->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(pats); i++) {
            json_t *pa = da12_args();
            char pbuf[256];
            da12_pystr(json_get(pats, i), pbuf, sizeof pbuf);
            da12_set_s(pa, "pattern", pbuf);
            da12_set_s(pa, "text", text);
            json_t *m = da12_call(ctx, "pattern_match", pa);
            if (da12_is_raise(m)) { json_free(pats); return m; }
            if (da12_truthy(m)) { any = true; json_free(m); break; }
            json_free(m);
        }
    }
    json_free(pats);
    return json_bool(any);
}

/* PoP: da12_is_length_overflow_error @ plugins/platforms/discord/adapter.py:DiscordAdapter._is_length_overflow_error */
static json_t *da12_is_length_overflow_error(da12_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char buf[1024];
    da12_pystr(json_obj_get(args, "err"), buf, sizeof buf);
    /* lowercase in place */
    for (char *p = buf; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    bool c1 = strstr(buf, "error code: 50035") != NULL;
    bool c2 = strstr(buf, "2000 or fewer") != NULL || strstr(buf, "fewer in length") != NULL;
    return json_bool(c1 && c2);
}

/* PoP: da12_normalize_permissions @ plugins/platforms/discord/adapter.py:DiscordAdapter._normalize_permissions */
static json_t *da12_normalize_permissions(da12_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *v = json_obj_get(args, "value");
    if (da12_is_null(v)) return json_null();
    char buf[512];
    da12_pystr(v, buf, sizeof buf);
    return json_string(buf);
}

/* PoP: da12_message_reference_from_ids @ plugins/platforms/discord/adapter.py:DiscordAdapter._message_reference_from_ids */
static json_t *da12_message_reference_from_ids(da12_ctx_t *ctx, const json_t *args) {
    const json_t *ch = json_obj_get(args, "channel");
    json_t *ca = da12_args();
    da12_set_copy(ca, "channel", ch);
    json_t *cid = da12_call(ctx, "channel_id", ca);
    if (da12_is_raise(cid)) return cid;
    json_t *ga = da12_args();
    da12_set_copy(ga, "channel", ch);
    json_t *gld = da12_call(ctx, "guild", ga);
    if (da12_is_raise(gld)) { json_free(cid); return gld; }
    json_t *gia = da12_args();
    da12_set_copy(gia, "guild", gld);
    json_t *gid = da12_call(ctx, "guild_id", gia);
    json_free(gld);
    if (da12_is_raise(gid)) { json_free(cid); return gid; }
    json_t *ma = da12_args();
    da12_set_num(ma, "message_id", (double)da12_to_int(json_obj_get(args, "message_id")));
    json_set(ma, "channel_id", cid ? json_copy(cid) : json_null());
    json_set(ma, "guild_id", gid ? json_copy(gid) : json_null());
    da12_set_boolv(ma, "fail_if_not_exists", false);
    json_free(cid); json_free(gid);
    return da12_call(ctx, "MessageReference", ma);
}

/* PoP: da12_missed_message_backfill_channels @ plugins/platforms/discord/adapter.py:DiscordAdapter._missed_message_backfill_channels */
static json_t *da12_missed_message_backfill_channels(da12_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *cfg = da12_call(ctx, "backfill_config", da12_args());
    if (da12_is_raise(cfg)) return cfg;
    if (cfg && cfg->type == JSON_OBJECT && json_obj_get(cfg, "channels")) {
        json_t *raw = json_obj_get(cfg, "channels");
        json_t *ca = da12_args();
        da12_set_copy(ca, "raw", raw);
        json_t *chans = da12_call(ctx, "gate_csv_set", ca);
        if (da12_is_raise(chans)) { json_free(cfg); return chans; }
        bool nonempty = chans && chans->type == JSON_ARRAY && json_len(chans) > 0;
        if (!nonempty) {
            json_t *da = da12_args();
            da12_set_copy(da, "raw", raw);
            json_t *dec = da12_call(ctx, "decode_json_list_literal", da);
            if (da12_is_raise(dec)) { json_free(cfg); json_free(chans); return dec; }
            bool islist = dec && dec->type == JSON_ARRAY;
            json_free(dec);
            if (!islist) { json_free(cfg); json_free(chans); chans = NULL; }
        }
        json_free(cfg);
        if (chans) return chans;
    }
    if (cfg) json_free(cfg);
    json_t *ea = da12_args();
    da12_set_s(ea, "key", "DISCORD_MISSED_MESSAGE_BACKFILL_CHANNELS");
    json_t *raw = da12_call(ctx, "gate_env", ea);
    if (da12_is_raise(raw)) return raw;
    if (!da12_scalar_strip_nonempty(raw)) {
        json_free(raw);
        json_t *allowed = da12_call(ctx, "get_allowed_channels", da12_args());
        if (da12_is_raise(allowed)) return allowed;
        json_t *free = da12_call(ctx, "free_response_channels", da12_args());
        if (da12_is_raise(free)) { json_free(allowed); return free; }
        json_t *u = da12_union_strings(allowed, free);
        json_free(allowed); json_free(free);
        return u;
    }
    json_t *ca = da12_args();
    da12_set_copy(ca, "raw", raw);
    json_free(raw);
    return da12_call(ctx, "gate_csv_set", ca);
}

/* PoP: da12_notify_unauthorized_slash @ plugins/platforms/discord/adapter.py:DiscordAdapter._notify_unauthorized_slash */
static json_t *da12_notify_unauthorized_slash(da12_ctx_t *ctx, const json_t *args) {
    json_t *runner = da12_call(ctx, "gateway_runner", da12_args());
    if (da12_is_raise(runner)) return runner;
    if (!da12_truthy(runner)) { json_free(runner); return json_null(); }
    json_t *prof = da12_call(ctx, "owner_profile", da12_args());
    if (da12_is_raise(prof)) { json_free(runner); return prof; }
    /* deliver args */
    json_t *da = da12_args();
    json_set(da, "runner", json_copy(runner));
    json_set(da, "profile", prof ? json_copy(prof) : json_null());
    da12_set_copy(da, "user_name", json_obj_get(args, "user_name"));
    da12_set_copy(da, "user_id", json_obj_get(args, "user_id"));
    da12_set_copy(da, "chan_id", json_obj_get(args, "chan_id"));
    da12_set_copy(da, "guild_id", json_obj_get(args, "guild_id"));
    da12_set_copy(da, "command_text", json_obj_get(args, "command_text"));
    da12_set_copy(da, "reason", json_obj_get(args, "reason"));
    json_t *result = NULL;
    if (da12_truthy(prof)) {
        json_t *pa = da12_args();
        da12_set_copy(pa, "profile", prof);
        json_t *pdir = da12_call(ctx, "profile_dir", pa);
        if (da12_is_raise(pdir)) {
            result = pdir;
        } else {
            json_t *sa = da12_args();
            json_set(sa, "profile_dir", json_copy(pdir));
            json_free(pdir);
            json_t *scope = da12_call(ctx, "profile_scope", sa);
            if (da12_is_raise(scope)) result = scope;
            else { json_free(scope); result = da12_call(ctx, "deliver_alert", da); }
        }
    } else {
        result = da12_call(ctx, "deliver_alert", da);
    }
    if (da12_is_raise(result)) {
        if (da12_raise_is_base(result)) { json_free(runner); json_free(prof); return result; }
        /* except Exception: logger.debug; the raise is swallowed */
        const char *msg = json_get_str(result, "message", "");
        json_t *la = da12_args();
        da12_set_s(la, "fmt", "[Discord] Admin notify: profile %r scope failed: %s");
        json_t *arr = json_array();
        json_append(arr, prof ? json_copy(prof) : json_null());
        json_append(arr, json_string(msg));
        json_set(la, "args", arr);
        json_free(result);
        json_t *lg = da12_call(ctx, "logger.debug", la);
        if (da12_is_raise(lg)) { json_free(runner); json_free(prof); return lg; }
        json_free(lg);
    }
    json_free(runner); json_free(prof);
    return json_null();
}

/* PoP: da12_lead_silence_bytes @ plugins/platforms/discord/adapter.py:DiscordAdapter._lead_silence_bytes */
static json_t *da12_lead_silence_bytes(da12_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *cfg = da12_call(ctx, "voice_fx_cfg", da12_args());
    if (da12_is_raise(cfg)) return cfg;
    json_t *out = json_object();
    da12_set_num(out, "zeros", 0);
    const json_t *lmv = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "lead_silence_ms") : NULL;
    if (!lmv || !da12_truthy(lmv)) { json_free(cfg); return out; }
    char buf[256];
    da12_pystr(lmv, buf, sizeof buf);
    char *end = NULL;
    long long lead = strtoll(buf, &end, 10);
    while (*end == ' ' || *end == '\t') end++;
    json_free(cfg);
    if (*end != '\0' || lead <= 0) return out;
    json_t *mod = da12_call(ctx, "voice_mixer_module", da12_args());
    if (da12_is_raise(mod)) { json_free(out); return mod; }
    json_t *ba = da12_args();
    json_set(ba, "module", json_copy(mod));
    json_free(mod);
    json_t *bps = da12_call(ctx, "BYTES_PER_MS", ba);
    if (da12_is_raise(bps)) { json_free(out); return bps; }
    double b = da12_num(bps, 0.0);
    json_free(bps);
    if (b < 0) b = 0;
    da12_set_num(out, "zeros", b * (double)lead);
    return out;
}

/* PoP: da12_native_slash_commands @ plugins/platforms/discord/adapter.py:_native_slash_commands */
static json_t *da12_native_slash_commands(da12_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *lim = da12_call(ctx, "text_limit", da12_args());
    if (da12_is_raise(lim)) return lim;
    double limit = da12_num(lim, 0.0);
    json_free(lim);
    json_t *specs = da12_call(ctx, "specs", da12_args());
    if (da12_is_raise(specs)) return specs;
    json_t *out = json_array();
    if (specs && specs->type == JSON_ARRAY) {
        for (size_t si = 0; si < json_len(specs); si++) {
            const json_t *sp = json_get(specs, si);
            if (!sp || sp->type != JSON_ARRAY || json_len(sp) < 5) continue;
            const json_t *name = json_get(sp, 0), *desc_key = json_get(sp, 1);
            const json_t *sargs = json_get(sp, 2), *tmpl = json_get(sp, 3), *follow = json_get(sp, 4);
            /* _text(description_key) */
            json_t *ta = da12_args();
            da12_set_copy(ta, "key", desc_key);
            da12_set_num(ta, "limit", limit);
            json_t *desc = da12_call(ctx, "t_discord", ta);
            if (da12_is_raise(desc)) { json_free(specs); json_free(out); return desc; }
            json_t *largs = json_array();
            if (sargs && sargs->type == JSON_ARRAY) {
                for (size_t ai = 0; ai < json_len(sargs); ai++) {
                    const json_t *ar = json_get(sargs, ai);
                    if (!ar || ar->type != JSON_ARRAY || json_len(ar) < 5) continue;
                    const json_t *an = json_get(ar, 0), *at = json_get(ar, 1), *dv = json_get(ar, 2);
                    const json_t *dkey = json_get(ar, 3), *choices = json_get(ar, 4);
                    json_t *aa = da12_args();
                    da12_set_copy(aa, "key", dkey);
                    da12_set_num(aa, "limit", limit);
                    json_t *atext = da12_call(ctx, "t_discord", aa);
                    if (da12_is_raise(atext)) { json_free(desc); json_free(specs); json_free(out); return atext; }
                    json_t *chlist = json_null();
                    if (choices && choices->type == JSON_ARRAY) {
                        chlist = json_array();
                        for (size_t ci = 0; ci < json_len(choices); ci++) {
                            const json_t *cr = json_get(choices, ci);
                            if (!cr || cr->type != JSON_ARRAY || json_len(cr) < 2) continue;
                            const json_t *lbl = json_get(cr, 0), *val = json_get(cr, 1);
                            const char *ls = da12_str_or(lbl, "");
                            const json_t *chosen = lbl;
                            if (strchr(ls, '.') != NULL) {
                                json_t *la = da12_args();
                                da12_set_s(la, "key", ls);
                                da12_set_num(la, "limit", limit);
                                json_t *tr = da12_call(ctx, "t_discord", la);
                                if (da12_is_raise(tr)) {
                                    json_free(atext); json_free(chlist); json_free(desc); json_free(specs); json_free(out); return tr;
                                }
                                chosen = json_copy(tr);
                                json_free(tr);
                            } else chosen = json_copy(lbl);
                            json_t *pair = json_array();
                            json_append(pair, (json_t*)chosen);
                            json_append(pair, json_copy(val));
                            json_append(chlist, pair);
                        }
                    }
                    json_t *argt = json_array();
                    json_append(argt, an ? json_copy(an) : json_null());
                    json_append(argt, at ? json_copy(at) : json_null());
                    json_append(argt, dv ? json_copy(dv) : json_null());
                    json_append(argt, atext);
                    json_append(argt, chlist && chlist->type == JSON_ARRAY ? chlist : json_null());
                    json_append(largs, argt);
                }
            }
            json_t *row = json_array();
            json_append(row, name ? json_copy(name) : json_null());
            json_append(row, desc);
            json_append(row, largs);
            json_append(row, tmpl ? json_copy(tmpl) : json_null());
            json_append(row, follow ? json_copy(follow) : json_null());
            json_append(out, row);
        }
    }
    json_free(specs);
    return out;
}

/* ══ PoP functions (stage 2: async cores modeled as ordered-seam flows) ══ */

/* Python int() validation helper shared by the flow-12 async cores. */
static bool da12_int_of(const json_t *v, long long *out) {
    if (!v) return false;
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return true; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return true; }
    if (v->type == JSON_STRING) {
        char *end = NULL;
        long long n = strtoll(v->str_val ? v->str_val : "", &end, 10);
        while (*end == ' ' || *end == '\t') end++;
        if (*end == '\0') { *out = n; return true; }
    }
    return false;
}
/* Equality for Python `==` on the scalar fixture domain. */
static bool da12_eq(const json_t *a, const json_t *b) {
    bool an = a && (a->type == JSON_NUMBER || a->type == JSON_BOOL);
    bool bn = b && (b->type == JSON_NUMBER || b->type == JSON_BOOL);
    if (an && bn) return da12_num(a, 0) == da12_num(b, 0);
    if (a && a->type == JSON_STRING && b && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (da12_is_null(a)) return da12_is_null(b);
    return false;
}

/* Recursive core of _iter_channel_and_thread_messages. Returns NULL on success
 * (yields appended to out) or an owned raise spec to propagate. */
static json_t *da12_iter_msgs(da12_ctx_t *ctx, const json_t *channel, double limit,
                              const json_t *after, json_t *seen, json_t *out, int depth) {
    if (depth > 40) return NULL;
    json_t *ca = da12_args();
    json_set(ca, "channel", channel ? json_copy(channel) : json_null());
    json_t *cv = da12_call(ctx, "channel_id", ca);
    if (da12_is_raise(cv)) return cv;
    char key[256];
    da12_pystr(cv, key, sizeof key);
    json_free(cv);
    if (key[0] == '\0' || da12_list_has_str(seen, key)) return NULL;
    json_append(seen, json_string(key));

    json_t *channel_after = after ? json_copy(after) : json_null();
    json_t *ra = da12_args();
    da12_set_s(ra, "channel_key", key);
    json_t *cur = da12_call(ctx, "recovery_cursor", ra);
    if (da12_is_raise(cur)) { json_free(channel_after); return cur; }
    if (da12_truthy(cur)) {
        long long cid = 0;
        if (da12_int_of(cur, &cid)) {
            bool set_after = false;
            bool after_is_dt = (after && after->type == JSON_OBJECT && json_obj_get(after, "datetime") != NULL);
            if (!after_is_dt) {
                set_after = true;
            } else {
                json_t *sa = da12_args();
                da12_set_num(sa, "cursor_id", (double)cid);
                json_t *ts = da12_call(ctx, "snowflake_time", sa);
                if (da12_is_raise(ts)) { json_free(cur); json_free(channel_after); return ts; }
                double ats = json_get_num(after, "datetime", 0);
                if (ts->type == JSON_NUMBER && ts->num_val > ats) set_after = true;
                json_free(ts);
            }
            if (set_after) {
                json_t *oa = da12_args();
                da12_set_num(oa, "id", (double)cid);
                json_t *obj = da12_call(ctx, "discord_object", oa);
                if (da12_is_raise(obj)) { json_free(cur); json_free(channel_after); return obj; }
                json_free(channel_after);
                channel_after = obj;
            }
        }
    }
    json_free(cur);

    json_t *ha = da12_args();
    json_set(ha, "channel", channel ? json_copy(channel) : json_null());
    json_t *hc = da12_call(ctx, "history", ha);
    if (da12_is_raise(hc)) { json_free(channel_after); return hc; }
    bool callable = da12_truthy(hc);
    json_free(hc);
    if (callable) {
        json_t *hca = da12_args();
        da12_set_num(hca, "limit", limit);
        json_set(hca, "after", channel_after ? json_copy(channel_after) : json_null());
        da12_set_boolv(hca, "oldest_first", false);
        json_t *msgs = da12_call(ctx, "history_call", hca);
        if (da12_is_raise(msgs)) {
            if (da12_raise_is_base(msgs)) { json_free(channel_after); return msgs; }
            json_free(msgs);
            json_t *lg = da12_call(ctx, "logger.debug", da12_log_args(
                "[%s] Cannot read history for %s: %s", (json_t*)NULL));
            if (da12_is_raise(lg)) { json_free(channel_after); return lg; }
            json_free(lg);
        } else {
            if (msgs && msgs->type == JSON_ARRAY)
                for (size_t i = json_len(msgs); i > 0; i--) json_append(out, json_copy(json_get(msgs, i - 1)));
            json_free(msgs);
        }
    }
    json_t *ta = da12_args();
    json_set(ta, "channel", channel ? json_copy(channel) : json_null());
    json_t *ths = da12_call(ctx, "threads", ta);
    if (da12_is_raise(ths)) { json_free(channel_after); return ths; }
    json_t *children = json_array();
    if (ths && ths->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(ths); i++) json_append(children, json_copy(json_get(ths, i)));
    json_free(ths);
    json_t *aa = da12_args();
    json_set(aa, "channel", channel ? json_copy(channel) : json_null());
    json_t *arch = da12_call(ctx, "archived_threads", aa);
    if (da12_is_raise(arch)) { json_free(children); json_free(channel_after); return arch; }
    bool acall = da12_truthy(arch);
    json_free(arch);
    if (acall) {
        json_t *aca = da12_args();
        da12_set_num(aca, "limit", limit);
        json_t *al = da12_call(ctx, "archived_call", aca);
        if (da12_is_raise(al)) {
            if (da12_raise_is_base(al)) { json_free(children); json_free(channel_after); return al; }
            json_free(al);
            json_t *lg = da12_call(ctx, "logger.debug", da12_log_args(
                "[%s] Cannot list archived threads for %s: %s", (json_t*)NULL));
            if (da12_is_raise(lg)) { json_free(children); json_free(channel_after); return lg; }
            json_free(lg);
        } else {
            if (al && al->type == JSON_ARRAY)
                for (size_t i = 0; i < json_len(al); i++) json_append(children, json_copy(json_get(al, i)));
            json_free(al);
        }
    }
    json_free(channel_after);
    for (size_t i = 0; i < json_len(children); i++) {
        const json_t *th = json_get(children, i);
        json_t *tia = da12_args();
        json_set(tia, "thread", json_copy(th));
        json_t *tv = da12_call(ctx, "thread_id", tia);
        if (da12_is_raise(tv)) { json_free(children); return tv; }
        char tkey[256];
        da12_pystr(tv, tkey, sizeof tkey);
        json_free(tv);
        if (tkey[0] == '\0' || da12_list_has_str(seen, tkey)) continue;
        json_t *r = da12_iter_msgs(ctx, th, limit, after, seen, out, depth + 1);
        if (r) { json_free(children); return r; }
    }
    json_free(children);
    return NULL;
}

/* PoP: da12_iter_channel_and_thread_messages @ plugins/platforms/discord/adapter.py:DiscordAdapter._iter_channel_and_thread_messages */
static json_t *da12_iter_channel_and_thread_messages(da12_ctx_t *ctx, const json_t *args) {
    const json_t *ch = json_obj_get(args, "channel");
    double limit = da12_num(json_obj_get(args, "limit"), 0.0);
    const json_t *after = json_obj_get(args, "after");
    json_t *seen = json_array();
    const json_t *sv = json_obj_get(args, "seen_channels");
    if (sv && sv->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(sv); i++) json_append(seen, json_copy(json_get(sv, i)));
    json_t *out = json_array();
    json_t *r = da12_iter_msgs(ctx, ch, limit, after ? after : json_null(), seen, out, 0);
    json_free(seen);
    if (r) return r;
    return out;
}

/* PoP: da12_iter_missed_message_backfill_candidates @ plugins/platforms/discord/adapter.py:DiscordAdapter._iter_missed_message_backfill_candidates */
static json_t *da12_iter_missed_message_backfill_candidates(da12_ctx_t *ctx, const json_t *args) {
    json_t *cli = da12_call(ctx, "client", da12_args());
    if (da12_is_raise(cli)) return cli;
    if (!da12_truthy(cli)) { json_free(cli); return json_array(); }
    json_t *na = da12_call(ctx, "now", da12_args());
    if (da12_is_raise(na)) { json_free(cli); return na; }
    json_t *wa = da12_call(ctx, "backfill_window_seconds", da12_args());
    if (da12_is_raise(wa)) { json_free(cli); json_free(na); return wa; }
    double after_ts = da12_num(na, 0.0) - da12_num(wa, 0.0);
    json_free(na); json_free(wa);
    json_t *lim = da12_call(ctx, "backfill_limit", da12_args());
    if (da12_is_raise(lim)) { json_free(cli); return lim; }
    double limit = da12_num(lim, 0.0);
    json_free(lim);

    json_t *cands = json_array();
    const json_t *ids = json_obj_get(args, "channel_ids");
    if (da12_list_has_str(ids, "*")) {
        json_t *ga = da12_args();
        json_set(ga, "client", json_copy(cli));
        json_t *guilds = da12_call(ctx, "guilds", ga);
        if (da12_is_raise(guilds)) { json_free(cli); return guilds; }
        if (guilds && guilds->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(guilds); i++) {
                const json_t *g = json_get(guilds, i);
                json_t *tca = da12_args();
                json_set(tca, "guild", json_copy(g));
                json_t *tcs = da12_call(ctx, "text_channels", tca);
                if (da12_is_raise(tcs)) { json_free(guilds); json_free(cli); return tcs; }
                if (tcs && tcs->type == JSON_ARRAY)
                    for (size_t j = 0; j < json_len(tcs); j++) json_append(cands, json_copy(json_get(tcs, j)));
                json_free(tcs);
            }
        }
        json_free(guilds);
    } else {
        /* sorted(channel_ids) */
        const char *idbuf[256]; size_t nid = 0;
        if (ids && ids->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(ids) && nid < 256; i++) {
                char b[128]; da12_pystr(json_get(ids, i), b, sizeof b);
                idbuf[nid++] = strdup(b);
            }
        }
        for (size_t i = 1; i < nid; i++) {
            const char *k = idbuf[i]; size_t j = i;
            while (j > 0 && strcmp(idbuf[j-1], k) > 0) { idbuf[j] = idbuf[j-1]; j--; }
            idbuf[j] = k;
        }
        for (size_t i = 0; i < nid; i++) {
            long long ci = 0;
            (void)da12_int_of(json_string(idbuf[i]), &ci);
            json_t *ga = da12_args();
            da12_set_num(ga, "channel_id", (double)ci);
            json_t *ch = da12_call(ctx, "get_channel", ga);
            if (da12_is_raise(ch)) {
                if (da12_raise_is_base(ch)) { for (size_t q=0;q<nid;q++) free((void*)idbuf[q]); json_free(cands); json_free(cli); return ch; }
                json_free(ch); ch = NULL;
            }
            if (!ch || da12_is_null(ch)) {
                if (ch) json_free(ch);
                json_t *fa = da12_args();
                da12_set_num(fa, "channel_id", (double)ci);
                json_t *fch = da12_call(ctx, "fetch_channel", fa);
                if (da12_is_raise(fch)) {
                    if (da12_raise_is_base(fch)) { for (size_t q=0;q<nid;q++) free((void*)idbuf[q]); json_free(cands); json_free(cli); return fch; }
                    json_free(fch);
                    json_t *lg = da12_call(ctx, "logger.debug", da12_log_args(
                        "[%s] Cannot fetch backfill channel %s: %s", (json_t*)NULL));
                    if (da12_is_raise(lg)) { for (size_t q=0;q<nid;q++) free((void*)idbuf[q]); json_free(cands); json_free(cli); return lg; }
                    json_free(lg);
                    continue;
                }
                ch = fch;
            }
            if (ch) json_append(cands, ch);
        }
        for (size_t q = 0; q < nid; q++) free((void*)idbuf[q]);
    }
    json_free(cli);

    /* filter obfuscated placeholders */
    json_t *filtered = json_array();
    for (size_t i = 0; i < json_len(cands); i++) {
        json_t *oa = da12_args();
        json_set(oa, "channel", json_copy(json_get(cands, i)));
        json_t *ob = da12_call(ctx, "obfuscated", oa);
        if (da12_is_raise(ob)) { json_free(cands); return ob; }
        if (!da12_truthy(ob)) json_append(filtered, json_copy(json_get(cands, i)));
        json_free(ob);
    }
    json_free(cands);

    /* one iterator per candidate channel: scripted yields list + cursor */
    json_t *iters = json_array();
    for (size_t i = 0; i < json_len(filtered); i++) {
        json_t *ia = da12_args();
        json_set(ia, "channel", json_copy(json_get(filtered, i)));
        da12_set_num(ia, "limit", limit);
        da12_set_num(ia, "after", after_ts);
        json_t *msgs = da12_call(ctx, "iter_messages", ia);
        if (da12_is_raise(msgs)) { json_free(filtered); return msgs; }
        json_t *it = json_object();
        json_set(it, "msgs", msgs ? msgs : json_array());
        json_set(it, "idx", json_number(0));
        json_append(iters, it);
    }
    json_free(filtered);

    json_t *out = json_array();
    double yielded = 0;
    while (json_len(iters) > 0 && yielded < limit) {
        json_t *next_round = json_array();
        for (size_t i = 0; i < json_len(iters); i++) {
            json_t *it = json_get(iters, i);
            json_t *msgs = json_obj_get(it, "msgs");
            size_t idx = (size_t)json_get_num(it, "idx", 0);
            if (msgs && msgs->type == JSON_ARRAY && idx < json_len(msgs)) {
                json_append(out, json_copy(json_get(msgs, idx)));
                json_set(it, "idx", json_number((double)(idx + 1)));
                yielded += 1.0;
                json_append(next_round, json_copy(it));
                if (yielded >= limit) break;
            }
        }
        json_free(iters);
        iters = next_round;
    }
    json_free(iters);
    return out;
}

/* scan helper for _message_has_non_down_bot_response.
 * returns 1 (found), 0 (not found), or -1 with *prop set to a raise spec. */
static int da12_scan_history(da12_ctx_t *ctx, const json_t *message, const json_t *channel,
                             const json_t *bot_id, json_t **prop) {
    *prop = NULL;
    json_t *ha = da12_args();
    json_set(ha, "channel", channel ? json_copy(channel) : json_null());
    json_t *h = da12_call(ctx, "history", ha);
    if (da12_is_raise(h)) { if (da12_raise_is_base(h)) { *prop = h; return -1; } json_free(h); return 0; }
    if (!da12_truthy(h)) { json_free(h); return 0; }
    json_free(h);
    json_t *ca = da12_args();
    json_set(ca, "message", message ? json_copy(message) : json_null());
    json_t *created = da12_call(ctx, "created_at", ca);
    if (da12_is_raise(created)) { if (da12_raise_is_base(created)) { *prop = created; return -1; } json_free(created); return 0; }
    json_t *hca = da12_args();
    da12_set_num(hca, "limit", 25);
    json_set(hca, "after", created ? created : json_null());
    da12_set_boolv(hca, "oldest_first", true);
    json_t *cands = da12_call(ctx, "history_call", hca);
    if (da12_is_raise(cands)) { if (da12_raise_is_base(cands)) { *prop = cands; return -1; } json_free(cands); return 0; }
    int found = 0;
    if (cands && cands->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(cands) && !found; i++) {
            const json_t *cand = json_get(cands, i);
            json_t *aua = da12_args(); json_set(aua, "candidate", json_copy(cand));
            json_t *author = da12_call(ctx, "author", aua);
            if (da12_is_raise(author)) { if (da12_raise_is_base(author)) { *prop = author; json_free(cands); return -1; } json_free(author); break; }
            json_t *aida = da12_args(); json_set(aida, "author", json_copy(author));
            json_free(author);
            json_t *aid = da12_call(ctx, "author_id", aida);
            if (da12_is_raise(aid)) { if (da12_raise_is_base(aid)) { *prop = aid; json_free(cands); return -1; } json_free(aid); break; }
            bool same = da12_eq(aid, bot_id);
            json_free(aid);
            if (!same) continue;
            json_t *cta = da12_args(); json_set(cta, "candidate", json_copy(cand));
            json_t *content = da12_call(ctx, "content", cta);
            if (da12_is_raise(content)) { if (da12_raise_is_base(content)) { *prop = content; json_free(cands); return -1; } json_free(content); break; }
            json_t *nda = da12_args(); json_set(nda, "content", content ? content : json_null());
            json_t *down = da12_call(ctx, "is_down_notice", nda);
            if (da12_is_raise(down)) { if (da12_raise_is_base(down)) { *prop = down; json_free(cands); return -1; } json_free(down); break; }
            bool isdown = da12_truthy(down);
            json_free(down);
            if (isdown) continue;
            json_t *ra = da12_args(); json_set(ra, "candidate", json_copy(cand));
            json_t *ref = da12_call(ctx, "reference", ra);
            if (da12_is_raise(ref)) { if (da12_raise_is_base(ref)) { *prop = ref; json_free(cands); return -1; } json_free(ref); break; }
            json_t *rmia = da12_args(); json_set(rmia, "reference", ref ? json_copy(ref) : json_null());
            json_free(ref);
            json_t *rmi = da12_call(ctx, "ref_message_id", rmia);
            if (da12_is_raise(rmi)) { if (da12_raise_is_base(rmi)) { *prop = rmi; json_free(cands); return -1; } json_free(rmi); break; }
            char refbuf[256];
            if (da12_truthy(rmi)) da12_pystr(rmi, refbuf, sizeof refbuf); else refbuf[0] = '\0';
            json_free(rmi);
            json_t *mia = da12_args(); json_set(mia, "message", message ? json_copy(message) : json_null());
            json_t *mid = da12_call(ctx, "message_id", mia);
            if (da12_is_raise(mid)) { if (da12_raise_is_base(mid)) { *prop = mid; json_free(cands); return -1; } json_free(mid); break; }
            char midbuf[256]; da12_pystr(mid, midbuf, sizeof midbuf); json_free(mid);
            if (strcmp(refbuf, midbuf) == 0) found = 1;
        }
    }
    json_free(cands);
    return found;
}

/* PoP: da12_message_has_non_down_bot_response @ plugins/platforms/discord/adapter.py:DiscordAdapter._message_has_non_down_bot_response */
static json_t *da12_message_has_non_down_bot_response(da12_ctx_t *ctx, const json_t *args) {
    const json_t *message = json_obj_get(args, "message");
    json_t *cli = da12_call(ctx, "client", da12_args());
    if (da12_is_raise(cli)) return cli;
    if (!da12_truthy(cli)) { json_free(cli); return json_bool(false); }
    json_t *bla = da12_args(); json_set(bla, "client", json_copy(cli));
    json_t *bu = da12_call(ctx, "bot_user", bla);
    json_free(cli);
    if (da12_is_raise(bu)) return bu;
    json_t *bia = da12_args(); json_set(bia, "bot_user", bu ? json_copy(bu) : json_null());
    json_t *bid = da12_call(ctx, "bot_id", bia);
    json_free(bu);
    if (da12_is_raise(bid)) return bid;
    if (da12_is_null(bid)) { json_free(bid); return json_bool(false); }
    json_t *prop = NULL;
    json_t *mca = da12_args(); json_set(mca, "message", message ? json_copy(message) : json_null());
    json_t *chan = da12_call(ctx, "message_channel", mca);
    if (da12_is_raise(chan)) { json_free(bid); return chan; }
    int r = da12_scan_history(ctx, message, chan, bid, &prop);
    json_free(chan);
    if (r < 0) { json_free(bid); return prop; }
    if (r == 1) { json_free(bid); return json_bool(true); }
    json_t *ta = da12_args(); json_set(ta, "message", message ? json_copy(message) : json_null());
    json_t *thread = da12_call(ctx, "thread", ta);
    if (da12_is_raise(thread)) { json_free(bid); return thread; }
    if (!da12_is_null(thread)) {
        r = da12_scan_history(ctx, message, thread, bid, &prop);
        if (r < 0) { json_free(thread); json_free(bid); return prop; }
    }
    json_free(thread); json_free(bid);
    return json_bool(r == 1);
}

/* PoP: da12_liveness_loop @ plugins/platforms/discord/adapter.py:DiscordAdapter._liveness_loop */
static json_t *da12_liveness_loop(da12_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *iv = da12_call(ctx, "interval", da12_args());
    if (da12_is_raise(iv)) return iv;
    double interval = da12_num(iv, 1.0); json_free(iv);
    json_t *th = da12_call(ctx, "threshold", da12_args());
    if (da12_is_raise(th)) return th;
    double threshold = da12_num(th, 1.0); json_free(th);
    long long failures = 0;
    for (int guard = 0; guard < 256; guard++) {
        json_t *rv = da12_call(ctx, "running", da12_args());
        if (da12_is_raise(rv)) return rv;
        bool run = da12_truthy(rv); json_free(rv);
        if (!run) return json_null();
        json_t *sa = da12_args(); da12_set_num(sa, "interval", interval);
        json_t *sl = da12_call(ctx, "sleep", sa);
        if (da12_is_raise(sl)) return sl;   /* CancelledError (BaseException) propagates */
        json_free(sl);
        json_t *cli = da12_call(ctx, "client", da12_args());
        if (da12_is_raise(cli)) return cli;
        json_t *rv2 = da12_call(ctx, "running", da12_args());
        if (da12_is_raise(rv2)) { json_free(cli); return rv2; }
        bool run2 = da12_truthy(rv2); json_free(rv2);
        json_t *dv = da12_call(ctx, "disconnecting", da12_args());
        if (da12_is_raise(dv)) { json_free(cli); return dv; }
        bool disc = da12_truthy(dv); json_free(dv);
        if (!run2 || da12_is_null(cli) || disc) {
            json_t *lg = da12_call(ctx, "logger.info", da12_log_args(
                "[%s] Discord liveness probe exiting (running=%s, client=%s, disconnecting=%s)", (json_t*)NULL));
            json_free(cli);
            if (da12_is_raise(lg)) return lg;
            json_free(lg);
            return json_null();
        }
        json_t *hca = da12_args(); json_set(hca, "client", json_copy(cli));
        json_t *h = da12_call(ctx, "health", hca);
        json_free(cli);
        bool healthy = false;
        char reason[256] = "";
        if (da12_is_raise(h)) {
            if (da12_raise_is_base(h)) return h;
            json_free(h);
            healthy = false;
            snprintf(reason, sizeof reason, "health_check_error");
        } else {
            if (h && h->type == JSON_ARRAY && json_len(h) >= 2) {
                healthy = da12_truthy(json_get(h, 0));
                const char *rs = da12_str(json_get(h, 1));
                if (rs) snprintf(reason, sizeof reason, "%s", rs);
            }
            json_free(h);
        }
        if (healthy) {
            if (failures > 0) {
                json_t *lg = da12_call(ctx, "logger.info", da12_log_args(
                    "[%s] Discord Gateway WebSocket healthy again after %d unhealthy sample(s)", (json_t*)NULL));
                if (da12_is_raise(lg)) return lg;
                json_free(lg);
            }
            failures = 0;
            continue;
        }
        failures += 1;
        json_t *wa = da12_args();
        da12_set_s(wa, "reason", reason);
        da12_set_num(wa, "failures", (double)failures);
        da12_set_num(wa, "threshold", threshold);
        json_t *lg = da12_call(ctx, "logger.warning", wa);
        if (da12_is_raise(lg)) return lg;
        json_free(lg);
        json_t *treasons = da12_call(ctx, "terminal_reasons", da12_args());
        if (da12_is_raise(treasons)) return treasons;
        bool terminal = da12_list_has_str(treasons, reason);
        json_free(treasons);
        if (failures < threshold && !terminal) continue;
        json_t *sd = da12_args(); da12_set_boolv(sd, "value", true);
        json_t *sdr = da12_call(ctx, "set_disconnecting", sd);
        if (da12_is_raise(sdr)) return sdr; json_free(sdr);
        json_t *er = da12_call(ctx, "logger.error", da12_log_args(
            "[%s] Discord Gateway WebSocket %s (%s, %d/%d); forcing reconnect", (json_t*)NULL));
        if (da12_is_raise(er)) return er; json_free(er);
        json_t *fea = da12_args();
        da12_set_s(fea, "kind", "discord_websocket_health_stale");
        char msg[400];
        snprintf(msg, sizeof msg, "Discord Gateway WebSocket health check failed: %s", reason);
        da12_set_s(fea, "message", msg);
        da12_set_boolv(fea, "retryable", true);
        json_t *fer = da12_call(ctx, "set_fatal_error", fea);
        if (da12_is_raise(fer)) return fer; json_free(fer);
        json_t *cta = da12_args(); da12_set_s(cta, "coro", "_notify_liveness_fatal_error");
        json_t *ct = da12_call(ctx, "create_task", cta);
        if (da12_is_raise(ct)) return ct; json_free(ct);
        return json_null();
    }
    return json_null();
}

/* PoP: da12_notify_liveness_fatal_error @ plugins/platforms/discord/adapter.py:DiscordAdapter._notify_liveness_fatal_error */
static json_t *da12_notify_liveness_fatal_error(da12_ctx_t *ctx, const json_t *args) {
    const json_t *client = json_obj_get(args, "client");
    json_t *wa = da12_args(); json_set(wa, "client", client ? json_copy(client) : json_null());
    json_t *ws = da12_call(ctx, "ws", wa);
    if (da12_is_raise(ws)) return ws;

    /* close = create_task(client.close()) */
    json_t *ca = da12_args(); json_set(ca, "client", client ? json_copy(client) : json_null());
    json_t *close = da12_call(ctx, "close", ca);
    if (da12_is_raise(close)) {
        if (da12_raise_is_base(close)) { json_free(ws); return close; }
        json_free(close);
        json_t *lg = da12_call(ctx, "logger.debug", da12_log_args(
            "[%s] Error closing unhealthy Discord client", (json_t*)NULL));
        json_free(ws);
        if (da12_is_raise(lg)) return lg; json_free(lg);
        return json_null();
    }
    int state = 0; /* 0 ok, 1 timeout, 2 close-error */
    json_t *wa2 = da12_args();
    json_set(wa2, "task", json_copy(close));
    da12_set_num(wa2, "timeout", 1.0);
    json_t *w = da12_call(ctx, "wait", wa2);
    if (da12_is_raise(w)) {
        if (da12_raise_is_base(w)) { json_free(close); json_free(ws); return w; }
        json_free(w);
        json_t *lg = da12_call(ctx, "logger.debug", da12_log_args(
            "[%s] Error closing unhealthy Discord client", (json_t*)NULL));
        if (da12_is_raise(lg)) { json_free(close); json_free(ws); return lg; } json_free(lg);
        state = 2;
    } else {
        bool done = da12_truthy(json_obj_get(w, "done"));
        json_free(w);
        if (!done) state = 1;
        else {
            json_t *aca = da12_args(); json_set(aca, "task", json_copy(close));
            json_t *aw = da12_call(ctx, "await_close", aca);
            if (da12_is_raise(aw)) {
                if (da12_raise_is_base(aw)) { json_free(close); json_free(ws); return aw; }
                json_free(aw);
                json_t *lg = da12_call(ctx, "logger.debug", da12_log_args(
                    "[%s] Error closing unhealthy Discord client", (json_t*)NULL));
                if (da12_is_raise(lg)) { json_free(close); json_free(ws); return lg; } json_free(lg);
                state = 2;
            }
        }
    }
    if (state == 1) {
        json_t *lg = da12_call(ctx, "logger.warning", da12_log_args(
            "[%s] Timed out closing unhealthy Discord client", (json_t*)NULL));
        if (da12_is_raise(lg)) { json_free(close); json_free(ws); return lg; } json_free(lg);
        json_t *cna = da12_args(); json_set(cna, "task", json_copy(close));
        json_t *cn = da12_call(ctx, "cancel", cna);
        if (da12_is_raise(cn)) { json_free(close); json_free(ws); return cn; } json_free(cn);
        json_t *cba = da12_args(); json_set(cba, "task", json_copy(close));
        json_t *cb = da12_call(ctx, "add_done_callback", cba);
        if (da12_is_raise(cb)) { json_free(close); json_free(ws); return cb; } json_free(cb);
        json_t *cta = da12_args(); json_set(cta, "client", client ? json_copy(client) : json_null());
        json_t *ct = da12_call(ctx, "closing_task", cta);
        if (da12_is_raise(ct)) { json_free(close); json_free(ws); return ct; }
        if (ct && ct->type == JSON_OBJECT) {
            json_t *cna2 = da12_args(); json_set(cna2, "task", json_copy(ct));
            json_t *cn2 = da12_call(ctx, "cancel", cna2);
            if (da12_is_raise(cn2)) { json_free(ct); json_free(close); json_free(ws); return cn2; } json_free(cn2);
            json_t *cba2 = da12_args(); json_set(cba2, "task", json_copy(ct));
            json_t *cb2 = da12_call(ctx, "add_done_callback", cba2);
            if (da12_is_raise(cb2)) { json_free(ct); json_free(close); json_free(ws); return cb2; } json_free(cb2);
            json_t *sca = da12_args(); json_set(sca, "client", client ? json_copy(client) : json_null());
            json_set(sca, "value", json_null());
            json_t *sc = da12_call(ctx, "set_closing_task", sca);
            if (da12_is_raise(sc)) { json_free(ct); json_free(close); json_free(ws); return sc; } json_free(sc);
        }
        if (ct) json_free(ct);
        json_t *aba = da12_args(); json_set(aba, "ws", ws ? json_copy(ws) : json_null());
        json_t *ab = da12_call(ctx, "abort_transport", aba);
        if (da12_is_raise(ab)) {
            if (da12_raise_is_base(ab)) { json_free(close); json_free(ws); return ab; }
            json_free(ab);
            json_t *lg = da12_call(ctx, "logger.debug", da12_log_args(
                "[%s] Error aborting unhealthy Discord WebSocket transport", (json_t*)NULL));
            if (da12_is_raise(lg)) { json_free(close); json_free(ws); return lg; } json_free(lg);
        } else {
            if (da12_truthy(ab)) {
                json_t *lg = da12_call(ctx, "logger.warning", da12_log_args(
                    "[%s] Aborted unresponsive Discord WebSocket transport", (json_t*)NULL));
                if (da12_is_raise(lg)) { json_free(ab); json_free(close); json_free(ws); return lg; } json_free(lg);
            }
            json_free(ab);
        }
    }
    json_free(close);
    /* identity self._liveness_notification_task is asyncio.current_task() -> marker equality */
    json_t *nt = da12_call(ctx, "notif_task", da12_args());
    if (da12_is_raise(nt)) { json_free(ws); return nt; }
    json_t *ct2 = da12_call(ctx, "current_task", da12_args());
    if (da12_is_raise(ct2)) { json_free(nt); json_free(ws); return ct2; }
    bool same = da12_eq(nt, ct2);
    json_free(nt); json_free(ct2);
    if (same) {
        json_t *sa = da12_args(); json_set(sa, "value", json_null());
        json_t *sr = da12_call(ctx, "set_notif_task", sa);
        if (da12_is_raise(sr)) { json_free(ws); return sr; } json_free(sr);
    }
    json_free(ws);
    json_t *nf = da12_call(ctx, "notify_fatal_error", da12_args());
    if (da12_is_raise(nf)) {
        if (da12_raise_is_base(nf)) return nf;
        json_free(nf);
        json_t *lg = da12_call(ctx, "logger.debug", da12_log_args(
            "[%s] Fatal-error handler raised", (json_t*)NULL));
        if (da12_is_raise(lg)) return lg; json_free(lg);
    }
    return json_null();
}
