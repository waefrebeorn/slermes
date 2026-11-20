/*
 * feishu_adapter_flow2.c — Feishu/Lark adapter inbound-event + card-action FLOW
 * cores of plugins/platforms/feishu/adapter.py (hermes-agent). REMAINDER port:
 * the sibling feishu_adapter_flow.c owns the message-normalization tree (44
 * funcs); this file ports the UNCLAIMED synchronous event-handler / card-action
 * decision cluster (L2026-2443):
 *   format_message, _on_message_read_event, _on_p2p_chat_entered,
 *   _on_message_recalled, _on_bot_membership_change, _on_bot_added_to_chat,
 *   _on_bot_removed_from_chat, _loop_accepts_callbacks,
 *   _is_interactive_operator_authorized, _card_response, _card_toast,
 *   _validate_card_action, _handle_approval_card_action,
 *   _handle_update_prompt_card_action, _pop_validated_prompt_state,
 *   _is_card_action_duplicate, _on_reaction_event, _on_card_action_trigger.
 *
 * Universal seam: every SEAMED sibling / foreign call enters through ONE
 * callback fa2_call(ctx, name, args). The harness wires ctx->io to a recording
 * stub fed the case's scripted results; the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts. args is ADOPTED into the trace event; the return is a NEW owned
 * json_t (NULL == Python None; {"__raise__":cls,"message":str(exc)} == a raised
 * exception). A missing/exhausted script is a fail-closed RuntimeError on both
 * sides. BaseException kinds (SystemExit / KeyboardInterrupt / CancelledError /
 * GeneratorExit) pass through every catch site — this cluster has NO try/except
 * in the ported bodies, so every scripted raise (incl. BaseException) from a
 * seam propagates straight out.
 *
 * Mode table: every ported function runs its REAL body when it is the case
 * target; all of its sibling / foreign calls are SEAMED (fa2_call, scripted).
 * A ported sibling called BY the target is scripted everywhere except when it
 * is itself the target. Seamed-only collaborators (not ported here):
 *   _submit_on_loop, _get_cached_sender_name, _is_sender_authorized,
 *   _handle_reaction_event, _handle_card_action_event, _resolve_approval,
 *   _resolve_update_prompt, _build_resolved_approval_card,
 *   _build_resolved_update_prompt_card, _handle_clarify_card_action,
 *   _handle_choice_picker_card_action.
 * Foreign seams: approval_refusal, refusal_text, update_prompt_refusal
 * (gateway/approval_actor.py), time_time (time.time), and the logger levels
 * logger_debug / logger_info / logger_warning / logger_error.
 *
 * Logger fidelity: each logger.<level>(fmt, *args) fires a seam
 * {"fmt":fmt, "args":[...], "kwargs":{}} (raw fmt + positional args, NOT the
 * rendered message); the level is in the seam name. None of the ported bodies
 * pass exc_info, so kwargs is always {}. logger returns None; a scripted raise
 * (or missing script) propagates.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and fa2_-prefixed.
 *
 * Representation contract (shared with tests/oracle_feishu_adapter_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__": cls, "message": str(exc)}.
 * - `self` travels as args.self (a JSON object whose keys are the live
 *   attributes the body reads): _loop (null or {is_closed:bool}),
 *   _chat_info_cache / _card_action_tokens / _approval_state /
 *   _update_prompt_state (objects), _admins / _allowed_group_users (arrays of
 *   id strings, modelled as sets), config ({extra:{...}}).
 * - Attribute doubles (data / event / operator / context / reaction_type) are
 *   JSON objects whose keys are the live attributes; a missing key models a
 *   missing attribute (getattr default). action.value stays a plain OBJECT
 *   (the live body does isinstance(dict) + .get on it).
 * - loop double: null == None; {is_closed:b} models loop.is_closed()==b; an
 *   absent is_closed key models the getattr default lambda:False.
 * - SDK availability for _card_response / _card_toast travels as args.sdk
 *   {response:bool, card:bool, toast:bool} (P2CardActionTriggerResponse /
 *   CallBackCard / CallBackToast present vs None). The response projects to
 *   {card: null | {type:"raw", data:<card_data>}}; _card_toast adds
 *   {toast:{type:"warning", content:text}}.
 * - Coroutine-producing seams (_resolve_approval / _resolve_update_prompt /
 *   _handle_reaction_event / _handle_card_action_event) return an opaque marker
 *   value that is forwarded verbatim as _submit_on_loop's `coro` arg.
 * - _validate_card_action returns the 5-tuple as a 5-element array (or null).
 * - Approval / update-prompt ids travel as STRINGS on both sides (action_value
 *   id and the state-map key), so .get/.pop lookups agree; the fixture corpus
 *   never mixes int and str keys.
 * - Integral floats == ints (libjson %.0f; oracle canon folds integral floats).
 *
 * State emission (out.state): _on_bot_membership_change emits
 * {_chat_info_cache}; _is_card_action_duplicate emits {_card_action_tokens};
 * _pop_validated_prompt_state emits {states} (the popped map, args.states);
 * every other func emits {} (its observable output is entirely in ret/trace).
 *
 * Fixture corpus restrictions (documented; generator honours them):
 * - str()-computed slots (chat_id / message_id / operator ids / verb / label /
 *   ident / answer) carry str / int / bool / None only — no containers, no
 *   non-integral floats. .strip()/.lower()/.capitalize() corpus is ASCII.
 * - time_time scripts integral timestamps; _card_action_tokens values are
 *   integral (canon folds). TTL constant _FEISHU_CARD_ACTION_DEDUP_TTL_SECONDS
 *   = 900 is seeded verbatim.
 * - _CARD_ACTION_ROUTES is the fixed 4-entry ordered table from
 *   adapter_choice_cards.py (first truthy value key routes).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct fa2_ctx {
    json_t *state;      /* owned; emitted as out.state (mutating funcs) */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} fa2_ctx_t;

/* ── basic helpers ── */
static bool fa2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool fa2_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool fa2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *fa2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *fa2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *fa2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return fa2_err(type, msg);
}
/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *fa2_call(fa2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = fa2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = fa2_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = fa2_make_exc(raise);
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
/* Convenience args builders. */
static json_t *fa2_args(void) { return json_object(); }
static void fa2_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void fa2_set_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void fa2_set_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void fa2_set_null(json_t *a, const char *k) { json_set(a, k, json_null()); }
static void fa2_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ── string helpers ── */
static char *fa2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static uint32_t *fa2_utf8_decode(const char *s, size_t *n) {
    if (!s) s = "";
    size_t len = strlen(s);
    uint32_t *cps = malloc((len + 1) * sizeof(uint32_t));
    size_t k = 0;
    for (size_t i = 0; i < len;) {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp; size_t adv;
        if (c < 0x80) { cp = c; adv = 1; }
        else if ((c & 0xE0) == 0xC0 && i + 1 < len) {
            cp = ((c & 0x1Fu) << 6) | ((unsigned char)s[i+1] & 0x3Fu); adv = 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < len) {
            cp = ((c & 0x0Fu) << 12) | (((unsigned char)s[i+1] & 0x3Fu) << 6)
               | ((unsigned char)s[i+2] & 0x3Fu); adv = 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < len) {
            cp = ((c & 0x07u) << 18) | (((unsigned char)s[i+1] & 0x3Fu) << 12)
               | (((unsigned char)s[i+2] & 0x3Fu) << 6) | ((unsigned char)s[i+3] & 0x3Fu); adv = 4;
        } else { cp = 0xFFFD; adv = 1; }
        cps[k++] = cp; i += adv;
    }
    *n = k;
    return cps;
}
static void fa2_utf8_putcp(char **q, uint32_t cp) {
    char *o = *q;
    if (cp < 0x80) { *o++ = (char)cp; }
    else if (cp < 0x800) {
        *o++ = (char)(0xC0 | (cp >> 6)); *o++ = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        *o++ = (char)(0xE0 | (cp >> 12)); *o++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *o++ = (char)(0x80 | (cp & 0x3F));
    } else {
        *o++ = (char)(0xF0 | (cp >> 18)); *o++ = (char)(0x80 | ((cp >> 12) & 0x3F));
        *o++ = (char)(0x80 | ((cp >> 6) & 0x3F)); *o++ = (char)(0x80 | (cp & 0x3F));
    }
    *q = o;
}
static char *fa2_utf8_encode(const uint32_t *cps, size_t n) {
    char *o = malloc(n * 4 + 1);
    if (!o) return NULL;
    char *q = o;
    for (size_t i = 0; i < n; i++) fa2_utf8_putcp(&q, cps[i]);
    *q = '\0';
    return o;
}
/* Python str.strip() White_Space set (ASCII corpus). */
static bool fa2_is_ws_cp(uint32_t c) {
    if (c == 0x20 || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F)) return true;
    if (c == 0x85 || c == 0xA0 || c == 0x1680) return true;
    if (c >= 0x2000 && c <= 0x200A) return true;
    if (c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000) return true;
    return false;
}
static char *fa2_strip(const char *s) {
    size_t n = 0;
    uint32_t *cps = fa2_utf8_decode(s, &n);
    size_t a = 0, b = n;
    while (a < b && fa2_is_ws_cp(cps[a])) a++;
    while (b > a && fa2_is_ws_cp(cps[b-1])) b--;
    char *o = fa2_utf8_encode(cps + a, b - a);
    free(cps);
    return o;
}
/* ASCII lower / capitalize (corpus is ASCII). */
static char *fa2_lower_ascii(const char *s) {
    char *o = fa2_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *fa2_capitalize_ascii(const char *s) {
    char *o = fa2_strdup(s);
    if (o && o[0]) {
        o[0] = (char)toupper((unsigned char)o[0]);
        for (char *p = o + 1; *p; p++) *p = (char)tolower((unsigned char)*p);
    }
    return o;
}
/* Python str() of a JSON scalar in the fixture corpus (str/int/bool/None). */
static char *fa2_py_str_scalar(const json_t *v) {
    if (!v || v->type == JSON_NULL) return fa2_strdup("None");
    if (v->type == JSON_STRING) return fa2_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return fa2_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val); /* out-of-domain */
        return fa2_strdup(buf);
    }
    return fa2_strdup(""); /* containers out of str() domain */
}
/* str(x or "") for the id/text slots: falsy -> "", else str(scalar). */
static char *fa2_str_or_empty(const json_t *v) {
    if (!fa2_truthy(v)) return fa2_strdup("");
    return fa2_py_str_scalar(v);
}
/* str(getattr(obj, key, "") or "") — missing/null/falsy -> "". */
static char *fa2_attr_str_or_empty(const json_t *obj, const char *key) {
    return fa2_str_or_empty(obj ? json_obj_get(obj, key) : NULL);
}
static bool fa2_contains(const char *s, const char *sub) {
    return s && sub && strstr(s, sub) != NULL;
}
/* Membership of a string in a JSON array-of-strings (set model). */
static bool fa2_arr_has(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, s) == 0)
            return true;
    }
    return false;
}

/* ── logger seam: {fmt, args, kwargs}; returns NULL on success, raise on fail ── */
static json_t *fa2_log(fa2_ctx_t *ctx, const char *name, const char *fmt, json_t *pos) {
    json_t *a = json_object();
    json_set_hdr(a, "fmt", fmt);
    json_set(a, "args", pos ? pos : json_array());
    json_set(a, "kwargs", json_object());
    json_t *r = fa2_call(ctx, name, a);
    if (fa2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}
/* Build a one-string positional-args array (adopted by fa2_log). */
static json_t *fa2_pos1(const char *s) {
    json_t *a = json_array();
    json_append(a, json_string(s ? s : ""));
    return a;
}
/* One RAW-node positional arg (ids logged verbatim keep their JSON type). */
static json_t *fa2_pos_node(const json_t *v) {
    json_t *a = json_array();
    json_append(a, v ? json_copy(v) : json_null());
    return a;
}

/* ── loop double helper: loop is not None and not bool(loop.is_closed()) ── */
static bool fa2_loop_accepts(const json_t *loop) {
    if (!loop || loop->type == JSON_NULL) return false;
    /* getattr(loop, "is_closed", lambda: False)() — absent key -> False. */
    bool closed = json_get_bool(loop, "is_closed", false);
    return !closed;
}

/* PoP: fa2_format_message @ plugins/platforms/feishu/adapter.py:format_message */
static json_t *fa2_format_message(fa2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *content = json_get_str(args, "content", "");
    char *s = fa2_strip(content);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* PoP: fa2__on_message_read_event @ plugins/platforms/feishu/adapter.py:_on_message_read_event */
static json_t *fa2__on_message_read_event(fa2_ctx_t *ctx, const json_t *args) {
    const json_t *data = json_obj_get(args, "data");
    const json_t *event = data ? json_obj_get(data, "event") : NULL;
    const json_t *message = event ? json_obj_get(event, "message") : NULL;
    /* getattr(message, "message_id", None) or "" */
    char *mid = fa2_str_or_empty(message ? json_obj_get(message, "message_id") : NULL);
    json_t *lr = fa2_log(ctx, "logger_debug",
                         "[Feishu] Ignoring message_read event: %s", fa2_pos1(mid));
    free(mid);
    if (lr) return lr;
    return NULL;
}

/* PoP: fa2__on_p2p_chat_entered @ plugins/platforms/feishu/adapter.py:_on_p2p_chat_entered */
static json_t *fa2__on_p2p_chat_entered(fa2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *lr = fa2_log(ctx, "logger_debug", "[Feishu] User entered P2P chat with bot", NULL);
    if (lr) return lr;
    return NULL;
}

/* PoP: fa2__on_message_recalled @ plugins/platforms/feishu/adapter.py:_on_message_recalled */
static json_t *fa2__on_message_recalled(fa2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *lr = fa2_log(ctx, "logger_debug", "[Feishu] Message recalled by user", NULL);
    if (lr) return lr;
    return NULL;
}

/* PoP: fa2__on_bot_membership_change @ plugins/platforms/feishu/adapter.py:_on_bot_membership_change */
static json_t *fa2__on_bot_membership_change(fa2_ctx_t *ctx, const json_t *args) {
    const json_t *self = json_obj_get(args, "self");
    const json_t *data = json_obj_get(args, "data");
    const char *verb = json_get_str(args, "verb", "");
    const json_t *event = data ? json_obj_get(data, "event") : NULL;
    char *chat_id = fa2_attr_str_or_empty(event, "chat_id");
    json_t *pa = json_array();
    json_append(pa, json_string(verb));
    json_append(pa, json_string(chat_id));
    json_t *lr = fa2_log(ctx, "logger_info", "[Feishu] Bot %s chat: %s", pa);
    if (lr) { free(chat_id); return lr; }
    /* self._chat_info_cache.pop(chat_id, None) */
    json_t *cache = self ? json_obj_get(self, "_chat_info_cache") : NULL;
    if (cache && cache->type == JSON_OBJECT) json_obj_del(cache, chat_id);
    free(chat_id);
    return NULL;
}

/* PoP: fa2__on_bot_added_to_chat @ plugins/platforms/feishu/adapter.py:_on_bot_added_to_chat */
static json_t *fa2__on_bot_added_to_chat(fa2_ctx_t *ctx, const json_t *args) {
    json_t *a = fa2_args();
    fa2_set_copy(a, "data", json_obj_get(args, "data"));
    fa2_set_s(a, "verb", "added to");
    return fa2_call(ctx, "_on_bot_membership_change", a);
}

/* PoP: fa2__on_bot_removed_from_chat @ plugins/platforms/feishu/adapter.py:_on_bot_removed_from_chat */
static json_t *fa2__on_bot_removed_from_chat(fa2_ctx_t *ctx, const json_t *args) {
    json_t *a = fa2_args();
    fa2_set_copy(a, "data", json_obj_get(args, "data"));
    fa2_set_s(a, "verb", "removed from");
    return fa2_call(ctx, "_on_bot_membership_change", a);
}

/* PoP: fa2__loop_accepts_callbacks @ plugins/platforms/feishu/adapter.py:_loop_accepts_callbacks */
static json_t *fa2__loop_accepts_callbacks(fa2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *loop = json_obj_get(args, "loop");
    return json_bool(fa2_loop_accepts(loop));
}

/* PoP: fa2__is_interactive_operator_authorized @ plugins/platforms/feishu/adapter.py:_is_interactive_operator_authorized */
static json_t *fa2__is_interactive_operator_authorized(fa2_ctx_t *ctx, const json_t *args) {
    const json_t *self = json_obj_get(args, "self");
    char *normalized = fa2_strip(fa2_str_or_empty(json_obj_get(args, "open_id")));
    if (!normalized[0]) { free(normalized); return json_bool(false); }
    const json_t *admins = self ? json_obj_get(self, "_admins") : NULL;
    const json_t *allowed = self ? json_obj_get(self, "_allowed_group_users") : NULL;
    bool has_allowlist = (admins && admins->type == JSON_ARRAY && json_len(admins) > 0)
                      || (allowed && allowed->type == JSON_ARRAY && json_len(allowed) > 0);
    if (has_allowlist) {
        /* operator_ids = {normalized, strip(user_id), strip(union_id)} - {""} */
        char *uid = fa2_strip(fa2_str_or_empty(json_obj_get(args, "user_id")));
        char *unid = fa2_strip(fa2_str_or_empty(json_obj_get(args, "union_id")));
        bool hit = fa2_arr_has(admins, "*") || fa2_arr_has(allowed, "*")
                || fa2_arr_has(admins, normalized) || fa2_arr_has(allowed, normalized)
                || (uid[0] && (fa2_arr_has(admins, uid) || fa2_arr_has(allowed, uid)))
                || (unid[0] && (fa2_arr_has(admins, unid) || fa2_arr_has(allowed, unid)));
        free(uid); free(unid); free(normalized);
        return json_bool(hit);
    }
    /* principal = strip(user_id) or normalized */
    char *uid = fa2_strip(fa2_str_or_empty(json_obj_get(args, "user_id")));
    const char *principal = uid[0] ? uid : normalized;
    /* str(chat_id or "") or None */
    const json_t *chat_node = json_obj_get(args, "chat_id");
    char *chat_str = fa2_str_or_empty(chat_node);
    json_t *sa = fa2_args();
    fa2_set_s(sa, "user_id", principal);
    fa2_set_s(sa, "chat_type", "group");
    if (chat_str[0]) fa2_set_s(sa, "chat_id", chat_str); else fa2_set_null(sa, "chat_id");
    json_t *verdict = fa2_call(ctx, "_is_sender_authorized", sa);
    free(chat_str); free(uid);
    if (fa2_is_raise(verdict)) { free(normalized); return verdict; }
    /* verdict is None -> deny (log); else return verdict (bool). */
    if (!verdict || verdict->type == JSON_NULL) {
        json_free(verdict);
        json_t *lr = fa2_log(ctx, "logger_debug",
            "[Feishu] No gateway authorization check for card operator %s; denying",
            fa2_pos1(normalized));
        free(normalized);
        if (lr) return lr;
        return json_bool(false);
    }
    free(normalized);
    return verdict;
}

/* PoP: fa2__card_response @ plugins/platforms/feishu/adapter.py:_card_response */
static json_t *fa2__card_response(fa2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *sdk = json_obj_get(args, "sdk");
    bool resp_present = json_get_bool(sdk, "response", true);
    if (!resp_present) return NULL;   /* P2CardActionTriggerResponse is None */
    json_t *response = json_object();
    fa2_set_null(response, "card");
    const json_t *card_data = json_obj_get(args, "card_data");
    bool card_present = json_get_bool(sdk, "card", true);
    if (card_data && card_data->type != JSON_NULL && card_present) {
        json_t *card = json_object();
        json_set_hdr(card, "type", "raw");
        json_set(card, "data", json_copy(card_data));
        json_set(response, "card", card);
    }
    return response;
}

/* PoP: fa2__card_toast @ plugins/platforms/feishu/adapter.py:_card_toast */
static json_t *fa2__card_toast(fa2_ctx_t *ctx, const json_t *args) {
    const json_t *sdk = json_obj_get(args, "sdk");
    const char *text = json_get_str(args, "text", "");
    /* response := self._card_response() — SEAMED sibling. */
    json_t *ra = fa2_args();
    fa2_set_null(ra, "card_data");
    json_t *response = fa2_call(ctx, "_card_response", ra);
    if (fa2_is_raise(response)) return response;
    bool toast_present = json_get_bool(sdk, "toast", true);
    if (response && response->type != JSON_NULL && toast_present) {
        json_t *toast = json_object();
        json_set_hdr(toast, "type", "warning");
        json_set_hdr(toast, "content", text);
        json_set(response, "toast", toast);
    }
    return response;
}

/* PoP: fa2__validate_card_action @ plugins/platforms/feishu/adapter.py:_validate_card_action */
static json_t *fa2__validate_card_action(fa2_ctx_t *ctx, const json_t *args) {
    const json_t *event = json_obj_get(args, "event");
    const json_t *state = json_obj_get(args, "state");
    const char *label = json_get_str(args, "label", "");
    const json_t *ident = json_obj_get(args, "ident");
    const json_t *operator = event ? json_obj_get(event, "operator") : NULL;
    char *open_id = fa2_attr_str_or_empty(operator, "open_id");
    char *user_id = fa2_attr_str_or_empty(operator, "user_id");
    char *union_id = fa2_attr_str_or_empty(operator, "union_id");
    const json_t *context = event ? json_obj_get(event, "context") : NULL;
    char *callback_chat_id = fa2_attr_str_or_empty(context, "open_chat_id");
    char *expected_chat_id = fa2_str_or_empty(state ? json_obj_get(state, "chat_id") : NULL);
    /* self._is_interactive_operator_authorized(open_id, user_id=, union_id=,
     *     chat_id=expected_chat_id or callback_chat_id) */
    json_t *aa = fa2_args();
    fa2_set_s(aa, "open_id", open_id);
    fa2_set_s(aa, "user_id", user_id);
    fa2_set_s(aa, "union_id", union_id);
    fa2_set_s(aa, "chat_id", expected_chat_id[0] ? expected_chat_id : callback_chat_id);
    json_t *auth = fa2_call(ctx, "_is_interactive_operator_authorized", aa);
    if (fa2_is_raise(auth)) {
        free(open_id); free(user_id); free(union_id); free(callback_chat_id); free(expected_chat_id);
        return auth;
    }
    bool authorized = fa2_truthy(auth);
    json_free(auth);
    if (!authorized) {
        json_t *pa = json_array();
        json_append(pa, json_string(label));
        json_append(pa, json_string(open_id[0] ? open_id : "<unknown>"));
        json_t *lr = fa2_log(ctx, "logger_warning", "[Feishu] Unauthorized %s click by %s", pa);
        free(open_id); free(user_id); free(union_id); free(callback_chat_id); free(expected_chat_id);
        if (lr) return lr;
        return NULL;
    }
    if (callback_chat_id[0] && expected_chat_id[0] && strcmp(callback_chat_id, expected_chat_id) != 0) {
        char *cap = fa2_capitalize_ascii(label);
        json_t *pa = json_array();
        json_append(pa, json_string(cap));
        json_append(pa, ident ? json_copy(ident) : json_null());
        json_append(pa, json_string(expected_chat_id));
        json_append(pa, json_string(callback_chat_id));
        json_t *lr = fa2_log(ctx, "logger_warning",
            "[Feishu] %s callback chat mismatch for %s (expected=%s, got=%s)", pa);
        free(cap);
        free(open_id); free(user_id); free(union_id); free(callback_chat_id); free(expected_chat_id);
        if (lr) return lr;
        return NULL;
    }
    /* name = self._get_cached_sender_name(open_id) or open_id */
    json_t *na = fa2_args();
    fa2_set_s(na, "sender_id", open_id);
    json_t *name_r = fa2_call(ctx, "_get_cached_sender_name", na);
    if (fa2_is_raise(name_r)) {
        free(open_id); free(user_id); free(union_id); free(callback_chat_id); free(expected_chat_id);
        return name_r;
    }
    const char *name = (name_r && name_r->type == JSON_STRING && name_r->str_val && name_r->str_val[0])
                       ? name_r->str_val : open_id;
    json_t *r = json_array();
    json_append(r, json_string(open_id));
    json_append(r, json_string(callback_chat_id));
    json_append(r, json_string(name));
    json_append(r, json_string(user_id));
    json_append(r, json_string(union_id));
    json_free(name_r);
    free(open_id); free(user_id); free(union_id); free(callback_chat_id); free(expected_chat_id);
    return r;
}

/* PoP: fa2__handle_approval_card_action @ plugins/platforms/feishu/adapter.py:_handle_approval_card_action */
static json_t *fa2__handle_approval_card_action(fa2_ctx_t *ctx, const json_t *args) {
    const json_t *self = json_obj_get(args, "self");
    const json_t *event = json_obj_get(args, "event");
    const json_t *action_value = json_obj_get(args, "action_value");
    const json_t *loop = json_obj_get(args, "loop");
    const json_t *approval_id = action_value ? json_obj_get(action_value, "approval_id") : NULL;
    if (!approval_id || approval_id->type == JSON_NULL) {
        json_t *lr = fa2_log(ctx, "logger_debug", "[Feishu] Card action missing approval_id, ignoring", NULL);
        if (lr) return lr;
        json_t *ra = fa2_args(); fa2_set_null(ra, "card_data");
        return fa2_call(ctx, "_card_response", ra);
    }
    char *aid = fa2_py_str_scalar(approval_id);
    const json_t *states = self ? json_obj_get(self, "_approval_state") : NULL;
    const json_t *state = states ? json_obj_get(states, aid) : NULL;
    if (!fa2_truthy(state)) {
        json_t *lr = fa2_log(ctx, "logger_debug", "[Feishu] Approval %s already resolved or unknown", fa2_pos_node(approval_id));
        free(aid);
        if (lr) return lr;
        json_t *ra = fa2_args(); fa2_set_null(ra, "card_data");
        return fa2_call(ctx, "_card_response", ra);
    }
    /* choice = _APPROVAL_CHOICE_MAP.get(action_value.get("hermes_action"), "deny") */
    const char *ha = fa2_str(action_value ? json_obj_get(action_value, "hermes_action") : NULL);
    const char *choice = "deny";
    if (ha) {
        if (strcmp(ha, "approve_once") == 0) choice = "once";
        else if (strcmp(ha, "approve_session") == 0) choice = "session";
        else if (strcmp(ha, "approve_always") == 0) choice = "always";
        else if (strcmp(ha, "deny") == 0) choice = "deny";
    }
    /* checked = self._validate_card_action(event=, state=, label="approval", ident=approval_id) */
    json_t *va = fa2_args();
    fa2_set_copy(va, "event", event);
    fa2_set_copy(va, "state", state);
    fa2_set_s(va, "label", "approval");
    fa2_set_copy(va, "ident", approval_id);
    json_t *checked = fa2_call(ctx, "_validate_card_action", va);
    if (fa2_is_raise(checked)) { free(aid); return checked; }
    if (!checked || checked->type == JSON_NULL) {
        json_free(checked); free(aid);
        json_t *ra = fa2_args(); fa2_set_null(ra, "card_data");
        return fa2_call(ctx, "_card_response", ra);
    }
    const char *open_id = fa2_str(json_get(checked, 0));
    const char *chat_id = fa2_str(json_get(checked, 1));
    const char *user_name = fa2_str(json_get(checked, 2));
    const char *user_id = fa2_str(json_get(checked, 3));
    const char *union_id = fa2_str(json_get(checked, 4));
    /* requester := approval_refusal(state["session_key"], user_id or open_id,
     *     extra=self.config.extra, request_id=state.get("request_id"), choice=choice) */
    const json_t *config = self ? json_obj_get(self, "config") : NULL;
    const json_t *extra = config ? json_obj_get(config, "extra") : NULL;
    json_t *ara = fa2_args();
    fa2_set_s(ara, "session_key", fa2_str(json_obj_get(state, "session_key")));
    fa2_set_s(ara, "actor_id", (user_id && user_id[0]) ? user_id : open_id);
    fa2_set_copy(ara, "extra", extra);
    fa2_set_copy(ara, "request_id", json_obj_get(state, "request_id"));
    fa2_set_s(ara, "choice", choice);
    json_t *requester = fa2_call(ctx, "approval_refusal", ara);
    if (fa2_is_raise(requester)) { json_free(checked); free(aid); return requester; }
    if (fa2_truthy(requester)) {
        json_t *rta = fa2_args();
        fa2_set_s(rta, "requester", fa2_str(requester));
        json_t *rt = fa2_call(ctx, "refusal_text", rta);
        json_free(requester);
        if (fa2_is_raise(rt)) { json_free(checked); free(aid); return rt; }
        json_t *ta = fa2_args();
        fa2_set_s(ta, "text", fa2_str(rt));
        json_t *toast = fa2_call(ctx, "_card_toast", ta);
        json_free(rt); json_free(checked); free(aid);
        return toast;
    }
    json_free(requester);
    /* coro = self._resolve_approval(approval_id=, choice=, user_name=, open_id=,
     *     chat_id=, user_id=, union_id=) */
    json_t *ca = fa2_args();
    fa2_set_copy(ca, "approval_id", approval_id);
    fa2_set_s(ca, "choice", choice);
    fa2_set_s(ca, "user_name", user_name);
    fa2_set_s(ca, "open_id", open_id);
    fa2_set_s(ca, "chat_id", chat_id);
    fa2_set_s(ca, "user_id", user_id);
    fa2_set_s(ca, "union_id", union_id);
    json_t *coro = fa2_call(ctx, "_resolve_approval", ca);
    if (fa2_is_raise(coro)) { json_free(checked); free(aid); return coro; }
    json_t *sla = fa2_args();
    fa2_set_copy(sla, "loop", loop);
    json_set(sla, "coro", coro);   /* adopt */
    json_t *submitted = fa2_call(ctx, "_submit_on_loop", sla);
    if (fa2_is_raise(submitted)) { json_free(checked); free(aid); return submitted; }
    bool ok = fa2_truthy(submitted);
    json_free(submitted);
    if (!ok) {
        json_free(checked); free(aid);
        json_t *ra = fa2_args(); fa2_set_null(ra, "card_data");
        return fa2_call(ctx, "_card_response", ra);
    }
    /* return self._card_response(self._build_resolved_approval_card(
     *     choice=, user_name=, body=state.get("body"))) */
    json_t *ba = fa2_args();
    fa2_set_s(ba, "choice", choice);
    fa2_set_s(ba, "user_name", user_name);
    fa2_set_copy(ba, "body", json_obj_get(state, "body"));
    json_t *card = fa2_call(ctx, "_build_resolved_approval_card", ba);
    if (fa2_is_raise(card)) { json_free(checked); free(aid); return card; }
    json_t *ra = fa2_args();
    json_set(ra, "card_data", card);   /* adopt */
    json_t *resp = fa2_call(ctx, "_card_response", ra);
    json_free(checked); free(aid);
    return resp;
}

/* PoP: fa2__handle_update_prompt_card_action @ plugins/platforms/feishu/adapter.py:_handle_update_prompt_card_action */
static json_t *fa2__handle_update_prompt_card_action(fa2_ctx_t *ctx, const json_t *args) {
    const json_t *self = json_obj_get(args, "self");
    const json_t *event = json_obj_get(args, "event");
    const json_t *action_value = json_obj_get(args, "action_value");
    const json_t *loop = json_obj_get(args, "loop");
    const json_t *prompt_id = action_value ? json_obj_get(action_value, "update_prompt_id") : NULL;
    if (!prompt_id || prompt_id->type == JSON_NULL) {
        json_t *lr = fa2_log(ctx, "logger_debug", "[Feishu] Card action missing update_prompt_id, ignoring", NULL);
        if (lr) return lr;
        json_t *ra = fa2_args(); fa2_set_null(ra, "card_data");
        return fa2_call(ctx, "_card_response", ra);
    }
    char *pid = fa2_py_str_scalar(prompt_id);
    const json_t *states = self ? json_obj_get(self, "_update_prompt_state") : NULL;
    const json_t *state = states ? json_obj_get(states, pid) : NULL;
    if (!fa2_truthy(state)) {
        json_t *lr = fa2_log(ctx, "logger_debug", "[Feishu] Update prompt %s already resolved or unknown", fa2_pos_node(prompt_id));
        free(pid);
        if (lr) return lr;
        json_t *ra = fa2_args(); fa2_set_null(ra, "card_data");
        return fa2_call(ctx, "_card_response", ra);
    }
    /* answer = str(action_value.get("hermes_update_prompt_action", "") or "").strip().lower() */
    char *raw = fa2_str_or_empty(action_value ? json_obj_get(action_value, "hermes_update_prompt_action") : NULL);
    char *stripped = fa2_strip(raw);
    char *answer = fa2_lower_ascii(stripped);
    free(raw); free(stripped);
    if (strcmp(answer, "y") != 0 && strcmp(answer, "n") != 0) {
        json_t *lr = fa2_log(ctx, "logger_debug",
            "[Feishu] Card action has invalid update prompt answer=%r", fa2_pos1(answer));
        free(answer); free(pid);
        if (lr) return lr;
        json_t *ra = fa2_args(); fa2_set_null(ra, "card_data");
        return fa2_call(ctx, "_card_response", ra);
    }
    json_t *va = fa2_args();
    fa2_set_copy(va, "event", event);
    fa2_set_copy(va, "state", state);
    fa2_set_s(va, "label", "update prompt");
    fa2_set_copy(va, "ident", prompt_id);
    json_t *checked = fa2_call(ctx, "_validate_card_action", va);
    if (fa2_is_raise(checked)) { free(answer); free(pid); return checked; }
    if (!checked || checked->type == JSON_NULL) {
        json_free(checked); free(answer); free(pid);
        json_t *ra = fa2_args(); fa2_set_null(ra, "card_data");
        return fa2_call(ctx, "_card_response", ra);
    }
    const char *open_id = fa2_str(json_get(checked, 0));
    const char *chat_id = fa2_str(json_get(checked, 1));
    const char *user_name = fa2_str(json_get(checked, 2));
    const char *user_id = fa2_str(json_get(checked, 3));
    const char *union_id = fa2_str(json_get(checked, 4));
    /* requester := update_prompt_refusal(user_id or open_id, extra=self.config.extra) */
    const json_t *config = self ? json_obj_get(self, "config") : NULL;
    const json_t *extra = config ? json_obj_get(config, "extra") : NULL;
    json_t *ura = fa2_args();
    fa2_set_s(ura, "actor_id", (user_id && user_id[0]) ? user_id : open_id);
    fa2_set_copy(ura, "extra", extra);
    json_t *requester = fa2_call(ctx, "update_prompt_refusal", ura);
    if (fa2_is_raise(requester)) { json_free(checked); free(answer); free(pid); return requester; }
    if (fa2_truthy(requester)) {
        json_t *rta = fa2_args();
        fa2_set_s(rta, "requester", fa2_str(requester));
        json_t *rt = fa2_call(ctx, "refusal_text", rta);
        json_free(requester);
        if (fa2_is_raise(rt)) { json_free(checked); free(answer); free(pid); return rt; }
        json_t *ta = fa2_args();
        fa2_set_s(ta, "text", fa2_str(rt));
        json_t *toast = fa2_call(ctx, "_card_toast", ta);
        json_free(rt); json_free(checked); free(answer); free(pid);
        return toast;
    }
    json_free(requester);
    /* coro = self._resolve_update_prompt(prompt_id, answer, user_name, open_id=,
     *     chat_id=, user_id=, union_id=) */
    json_t *ca = fa2_args();
    fa2_set_copy(ca, "prompt_id", prompt_id);
    fa2_set_s(ca, "answer", answer);
    fa2_set_s(ca, "user_name", user_name);
    fa2_set_s(ca, "open_id", open_id);
    fa2_set_s(ca, "chat_id", chat_id);
    fa2_set_s(ca, "user_id", user_id);
    fa2_set_s(ca, "union_id", union_id);
    json_t *coro = fa2_call(ctx, "_resolve_update_prompt", ca);
    if (fa2_is_raise(coro)) { json_free(checked); free(answer); free(pid); return coro; }
    json_t *sla = fa2_args();
    fa2_set_copy(sla, "loop", loop);
    json_set(sla, "coro", coro);   /* adopt */
    json_t *submitted = fa2_call(ctx, "_submit_on_loop", sla);
    if (fa2_is_raise(submitted)) { json_free(checked); free(answer); free(pid); return submitted; }
    bool ok = fa2_truthy(submitted);
    json_free(submitted);
    if (!ok) {
        json_free(checked); free(answer); free(pid);
        json_t *ra = fa2_args(); fa2_set_null(ra, "card_data");
        return fa2_call(ctx, "_card_response", ra);
    }
    json_t *ba = fa2_args();
    fa2_set_s(ba, "answer", answer);
    fa2_set_s(ba, "user_name", user_name);
    json_t *card = fa2_call(ctx, "_build_resolved_update_prompt_card", ba);
    if (fa2_is_raise(card)) { json_free(checked); free(answer); free(pid); return card; }
    json_t *ra = fa2_args();
    json_set(ra, "card_data", card);   /* adopt */
    json_t *resp = fa2_call(ctx, "_card_response", ra);
    json_free(checked); free(answer); free(pid);
    return resp;
}

/* PoP: fa2__pop_validated_prompt_state @ plugins/platforms/feishu/adapter.py:_pop_validated_prompt_state */
static json_t *fa2__pop_validated_prompt_state(fa2_ctx_t *ctx, const json_t *args) {
    json_t *states = json_obj_get(args, "states");   /* mutable; borrowed from args */
    const json_t *ident = json_obj_get(args, "ident");
    const char *label = json_get_str(args, "label", "");
    const char *open_id = json_get_str(args, "open_id", "");
    const char *chat_id = json_get_str(args, "chat_id", "");
    const char *unauthorized_fmt = json_get_str(args, "unauthorized_fmt", "");
    const char *operator_repr = json_get_str(args, "operator_repr", "");
    const char *user_id = json_get_str(args, "user_id", "");
    const char *union_id = json_get_str(args, "union_id", "");
    char *ident_str = fa2_py_str_scalar(ident);
    const json_t *state = (states && states->type == JSON_OBJECT) ? json_obj_get(states, ident_str) : NULL;
    if (!fa2_truthy(state)) {
        json_t *pa = json_array();
        json_append(pa, json_string(label));
        json_append(pa, ident ? json_copy(ident) : json_null());
        json_t *lr = fa2_log(ctx, "logger_debug", "[Feishu] %s %s already resolved or unknown", pa);
        free(ident_str);
        if (lr) return lr;
        return NULL;
    }
    /* chat_id = str(state.get("chat_id","") or "") or chat_id */
    char *state_chat = fa2_str_or_empty(json_obj_get(state, "chat_id"));
    const char *auth_chat = state_chat[0] ? state_chat : chat_id;
    json_t *aa = fa2_args();
    fa2_set_s(aa, "open_id", open_id);
    fa2_set_s(aa, "user_id", user_id);
    fa2_set_s(aa, "union_id", union_id);
    fa2_set_s(aa, "chat_id", auth_chat);
    json_t *auth = fa2_call(ctx, "_is_interactive_operator_authorized", aa);
    free(state_chat);
    if (fa2_is_raise(auth)) { free(ident_str); return auth; }
    bool authorized = fa2_truthy(auth);
    json_free(auth);
    if (!authorized) {
        json_t *pa = json_array();
        json_append(pa, json_string(operator_repr));
        json_append(pa, ident ? json_copy(ident) : json_null());
        json_t *lr = fa2_log(ctx, "logger_warning", unauthorized_fmt, pa);
        free(ident_str);
        if (lr) return lr;
        return NULL;
    }
    char *expected_chat_id = fa2_str_or_empty(json_obj_get(state, "chat_id"));
    if (expected_chat_id[0] && chat_id[0] && strcmp(expected_chat_id, chat_id) != 0) {
        json_t *pa = json_array();
        json_append(pa, json_string(label));
        json_append(pa, ident ? json_copy(ident) : json_null());
        json_append(pa, json_string(expected_chat_id));
        json_append(pa, json_string(chat_id));
        json_t *lr = fa2_log(ctx, "logger_warning",
            "[Feishu] %s %s chat mismatch (expected=%s, got=%s)", pa);
        free(expected_chat_id); free(ident_str);
        if (lr) return lr;
        return NULL;
    }
    free(expected_chat_id);
    /* state = states.pop(ident, None) */
    json_t *popped = NULL;
    if (states && states->type == JSON_OBJECT) {
        json_t *cur = json_obj_get(states, ident_str);
        if (cur) { popped = json_copy(cur); json_obj_del(states, ident_str); }
    }
    if (!fa2_truthy(popped)) {
        json_t *pa = json_array();
        json_append(pa, json_string(label));
        json_append(pa, ident ? json_copy(ident) : json_null());
        json_t *lr = fa2_log(ctx, "logger_debug",
            "[Feishu] %s %s already resolved while validating callback", pa);
        free(ident_str);
        if (lr) { json_free(popped); return lr; }
    }
    free(ident_str);
    return popped;   /* NULL when absent -> None */
}

/* PoP: fa2__is_card_action_duplicate @ plugins/platforms/feishu/adapter.py:_is_card_action_duplicate */
static json_t *fa2__is_card_action_duplicate(fa2_ctx_t *ctx, const json_t *args) {
    const json_t *self = json_obj_get(args, "self");
    const char *token = json_get_str(args, "token", "");
    json_t *ta = fa2_args();
    json_t *now_r = fa2_call(ctx, "time_time", ta);
    if (fa2_is_raise(now_r)) return now_r;
    double now = (now_r && now_r->type == JSON_NUMBER) ? now_r->num_val : 0.0;
    json_free(now_r);
    json_t *tokens = self ? json_obj_get(self, "_card_action_tokens") : NULL;
    if (tokens && tokens->type == JSON_OBJECT) {
        /* expired = [t for t,ts in tokens.items() if now - ts > TTL]; del each */
        size_t n = json_object_size(tokens);
        char **expired = malloc((n + 1) * sizeof(char *));
        size_t ne = 0;
        for (size_t i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(tokens, i);
            const json_t *v = json_object_get_at(tokens, i);
            double ts = (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
            if (now - ts > 900.0) expired[ne++] = fa2_strdup(k);
        }
        for (size_t i = 0; i < ne; i++) { json_obj_del(tokens, expired[i]); free(expired[i]); }
        free(expired);
        if (json_obj_get(tokens, token) != NULL) return json_bool(true);
        json_set(tokens, token, json_number(now));
    }
    return json_bool(false);
}

/* PoP: fa2__on_reaction_event @ plugins/platforms/feishu/adapter.py:_on_reaction_event */
static json_t *fa2__on_reaction_event(fa2_ctx_t *ctx, const json_t *args) {
    const json_t *self = json_obj_get(args, "self");
    const char *event_type = json_get_str(args, "event_type", "");
    const json_t *data = json_obj_get(args, "data");
    const json_t *event = data ? json_obj_get(data, "event") : NULL;
    char *message_id = fa2_attr_str_or_empty(event, "message_id");
    char *operator_type = fa2_attr_str_or_empty(event, "operator_type");
    const json_t *rt_obj = event ? json_obj_get(event, "reaction_type") : NULL;
    char *emoji_type = fa2_attr_str_or_empty(rt_obj, "emoji_type");
    const char *action = fa2_contains(event_type, "created") ? "added" : "removed";
    json_t *pa = json_array();
    json_append(pa, json_string(action));
    json_append(pa, json_string(message_id));
    json_append(pa, json_string(operator_type));
    json_append(pa, json_string(emoji_type));
    json_t *lr = fa2_log(ctx, "logger_debug",
        "[Feishu] Reaction %s on message %s (operator_type=%s, emoji=%s)", pa);
    if (lr) { free(message_id); free(operator_type); free(emoji_type); return lr; }
    const json_t *loop = self ? json_obj_get(self, "_loop") : NULL;
    /* if operator_type in {"bot","app"} or not message_id or not accepts(loop): return */
    bool bot_app = strcmp(operator_type, "bot") == 0 || strcmp(operator_type, "app") == 0;
    bool drop = bot_app || !message_id[0];
    if (!drop) {
        json_t *la = fa2_args();
        fa2_set_copy(la, "loop", loop);
        json_t *acc = fa2_call(ctx, "_loop_accepts_callbacks", la);
        if (fa2_is_raise(acc)) { free(message_id); free(operator_type); free(emoji_type); return acc; }
        drop = !fa2_truthy(acc);
        json_free(acc);
    }
    if (drop) { free(message_id); free(operator_type); free(emoji_type); return NULL; }
    /* self._submit_on_loop(loop, self._handle_reaction_event(event_type, data)) */
    json_t *ha = fa2_args();
    fa2_set_s(ha, "event_type", event_type);
    fa2_set_copy(ha, "data", data);
    json_t *coro = fa2_call(ctx, "_handle_reaction_event", ha);
    if (fa2_is_raise(coro)) { free(message_id); free(operator_type); free(emoji_type); return coro; }
    json_t *sla = fa2_args();
    fa2_set_copy(sla, "loop", loop);
    json_set(sla, "coro", coro);   /* adopt */
    json_t *sub = fa2_call(ctx, "_submit_on_loop", sla);
    free(message_id); free(operator_type); free(emoji_type);
    if (fa2_is_raise(sub)) return sub;
    json_free(sub);
    return NULL;
}

/* PoP: fa2__on_card_action_trigger @ plugins/platforms/feishu/adapter.py:_on_card_action_trigger */
static json_t *fa2__on_card_action_trigger(fa2_ctx_t *ctx, const json_t *args) {
    const json_t *self = json_obj_get(args, "self");
    const json_t *data = json_obj_get(args, "data");
    const json_t *loop = self ? json_obj_get(self, "_loop") : NULL;
    json_t *la = fa2_args();
    fa2_set_copy(la, "loop", loop);
    json_t *acc = fa2_call(ctx, "_loop_accepts_callbacks", la);
    if (fa2_is_raise(acc)) return acc;
    bool accepts = fa2_truthy(acc);
    json_free(acc);
    if (!accepts) {
        json_t *lr = fa2_log(ctx, "logger_warning",
            "[Feishu] Dropping card action before adapter loop is ready", NULL);
        if (lr) return lr;
        json_t *ra = fa2_args(); fa2_set_null(ra, "card_data");
        return fa2_call(ctx, "_card_response", ra);
    }
    const json_t *event = data ? json_obj_get(data, "event") : NULL;
    const json_t *action = event ? json_obj_get(event, "action") : NULL;
    const json_t *val = action ? json_obj_get(action, "value") : NULL;
    /* action_value = getattr(action, "value", {}) or {} */
    const json_t *av = fa2_truthy(val) ? val : NULL;   /* NULL == empty dict {} */
    bool av_is_dict = av ? (av->type == JSON_OBJECT) : true;
    if (av_is_dict) {
        /* _CARD_ACTION_ROUTES, first truthy value key routes (ordered table).
         * Each handler is a LITERAL fa2_call site so the oracle's seam-name
         * regex captures it. args = {event, action_value, loop}. */
        json_t *hav = av ? json_copy(av) : json_object();   /* live {} when falsy */
        if (fa2_truthy(av ? json_obj_get(av, "hermes_action") : NULL)) {
            json_t *ha = fa2_args();
            fa2_set_copy(ha, "event", event);
            json_set(ha, "action_value", json_copy(hav));
            fa2_set_copy(ha, "loop", loop);
            json_free(hav);
            return fa2_call(ctx, "_handle_approval_card_action", ha);
        }
        if (fa2_truthy(av ? json_obj_get(av, "hermes_update_prompt_action") : NULL)) {
            json_t *ha = fa2_args();
            fa2_set_copy(ha, "event", event);
            json_set(ha, "action_value", json_copy(hav));
            fa2_set_copy(ha, "loop", loop);
            json_free(hav);
            return fa2_call(ctx, "_handle_update_prompt_card_action", ha);
        }
        if (fa2_truthy(av ? json_obj_get(av, "hermes_clarify_action") : NULL)) {
            json_t *ha = fa2_args();
            fa2_set_copy(ha, "event", event);
            json_set(ha, "action_value", json_copy(hav));
            fa2_set_copy(ha, "loop", loop);
            json_free(hav);
            return fa2_call(ctx, "_handle_clarify_card_action", ha);
        }
        if (fa2_truthy(av ? json_obj_get(av, "hermes_choice_picker_action") : NULL)) {
            json_t *ha = fa2_args();
            fa2_set_copy(ha, "event", event);
            json_set(ha, "action_value", json_copy(hav));
            fa2_set_copy(ha, "loop", loop);
            json_free(hav);
            return fa2_call(ctx, "_handle_choice_picker_card_action", ha);
        }
        json_free(hav);
    }
    /* self._submit_on_loop(loop, self._handle_card_action_event(data)) */
    json_t *ha = fa2_args();
    fa2_set_copy(ha, "data", data);
    json_t *coro = fa2_call(ctx, "_handle_card_action_event", ha);
    if (fa2_is_raise(coro)) return coro;
    json_t *sla = fa2_args();
    fa2_set_copy(sla, "loop", loop);
    json_set(sla, "coro", coro);   /* adopt */
    json_t *sub = fa2_call(ctx, "_submit_on_loop", sla);
    if (fa2_is_raise(sub)) return sub;
    json_free(sub);
    json_t *ra = fa2_args(); fa2_set_null(ra, "card_data");
    return fa2_call(ctx, "_card_response", ra);
}