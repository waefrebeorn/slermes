/*
 * matrix_adapter_flow3.c -- FLOW port of UNCLAIMED sync/event + reaction
 * methods of plugins/platforms/matrix/adapter.py (hermes-agent): the
 * encrypted-drop warning, room allowlist gate, processing eyes ticks, reaction
 * routing, and the reaction-prompt claim/validate/expire/redact lifecycle.
 * The flow1/flow2 siblings and create_room are CLAIMED; every collaborator
 * below (ported or claimed) enters through the ONE universal seam as
 * "self.<name>" or a bare module-function name -- none is re-ported.
 *
 * Decision sequences only: every live I/O / injected dependency enters through
 * ma3_call(ctx, name, args). The harness wires it to a recording stub fed the
 * case's scripted results (in.io); the oracle installs recording stubs emitting
 * the identical event shape {"name","args","ret"} from the SAME scripts.
 *
 * Seam name -> live call:
 *   self._is_self_sender          -> self._is_self_sender(sender)
 *   self._is_duplicate_event      -> self._is_duplicate_event(event_id)
 *   hash_for_log                  -> hash_for_log(room_id)
 *   self._handle_approval_reaction     -> self._handle_approval_reaction(...)
 *   self._handle_model_picker_reaction -> self._handle_model_picker_reaction(...)
 *   self._handle_choice_picker_reaction-> self._handle_choice_picker_reaction(...)
 *   self._handle_picker_reaction        -> self._handle_picker_reaction(...)
 *   self._send_reaction           -> self._send_reaction(room, event, emoji)
 *   self._schedule_reaction_redaction -> self._schedule_reaction_redaction(...)
 *   self._claim_reaction_prompt   -> self._claim_reaction_prompt(...)
 *   on_expired                    -> the on_expired callback(prompt)
 *   self._validate_matrix_prompt_reactor -> self._validate_matrix_prompt_reactor(...)
 *   self._send_invalid_reaction_feedback -> self._send_invalid_reaction_feedback(...)
 *   self._is_authorized_user      -> self._is_authorized_user(user, room)
 *   t                             -> t(key, **kw)
 *   approval_refusal              -> approval_refusal(session_key, sender, ...)
 *   refusal_text                  -> refusal_text(requester)
 *   self._redact_bot_approval_reactions -> self._redact_bot_approval_reactions(...)
 *   resolve_gateway_approval_card -> tools.approval.resolve_gateway_approval_card(...)
 *   self._redact_bot_model_picker_reactions -> self._redact_bot_model_picker_reactions(...)
 *   self.send                     -> self.send(room, text, reply_to=)
 *   self._expire_matrix_approval_prompt -> self._expire_matrix_approval_prompt(...)
 *   self._expire_matrix_model_picker_prompt -> self._expire_matrix_model_picker_prompt(...)
 *   prompt.on_selected            -> await prompt.on_selected(room, *args)
 *   picker_refusal                -> picker_refusal(on_selected, sender, extra)
 *   self._redact_reaction         -> self._redact_reaction(room, event, reason)
 *   self._is_dm_room              -> self._is_dm_room(room)
 *   self._matrix_prompt_expired   -> self._matrix_prompt_expired(prompt)
 *   self.redact_message           -> self.redact_message(room, event, reason)
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and ma3_-prefixed.
 *
 * Representation contract (shared with tests/oracle_matrix_adapter_flow3.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; scripted raises
 *   round-trip verbatim when they escape a drive.
 * - Attribute carriers (client, prompt, config) are {"__attr__": {attrs}};
 *   events are plain objects. choices/offered/bot_reaction_events stay dicts;
 *   on_selected is a string marker. Expires_at is an int or null (no floats).
 * - Registries / config live in args.self and are NOT echoed; only state keys
 *   _warned_encrypted_drop_rooms (set -> sorted array) and _pending_reactions
 *   (dict; tuple keys projected as str(tuple)) are echoed.
 * - Prompt objects passed to seams project to their INNER attr dict (the
 *   oracle's SimpleNamespace canon is the same dict, on_selected -> marker).
 * - logger calls are silent; the cause string of _warn_encrypted_drops is
 *   unobservable. Fixture ids are simple ASCII (str(tuple)/repr domain).
 * - BaseException kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 *   CancelledError) pass through every except-Exception site.
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

/* -- universal seam context -- */
typedef struct ma3_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} ma3_ctx_t;

/* -- exception classification (FULL builtin hierarchy) -- */
static bool ma3_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ma3_exc_parent(const char *k) {
    static const struct { const char *n, *p; } T[] = {
        {"BaseException", NULL},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"}, {"CancelledError", "BaseException"},
        {"Exception", "BaseException"},
        {"TypeError", "Exception"}, {"ValueError", "Exception"},
        {"AttributeError", "Exception"}, {"NameError", "Exception"},
        {"UnboundLocalError", "NameError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"},
        {"KeyError", "LookupError"},
        {"ArithmeticError", "Exception"}, {"FloatingPointError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"}, {"ZeroDivisionError", "ArithmeticError"},
        {"AssertionError", "Exception"}, {"BufferError", "Exception"},
        {"EOFError", "Exception"}, {"ImportError", "Exception"},
        {"ModuleNotFoundError", "ImportError"}, {"OSError", "Exception"},
        {"ConnectionError", "OSError"}, {"TimeoutError", "OSError"},
        {NULL, NULL}
    };
    for (size_t i = 0; T[i].n; i++) if (strcmp(T[i].n, k) == 0) return T[i].p;
    return "Exception";
}
static bool ma3_exc_is_exception(const char *k) {
    const char *p = k;
    for (int depth = 0; p && depth < 16; depth++) {
        if (strcmp(p, "Exception") == 0) return true;
        if (strcmp(p, "BaseException") == 0) return false;
        p = ma3_exc_parent(p);
    }
    return false;
}
static bool ma3_raise_is_base(const json_t *v) {
    return ma3_is_raise(v) && !ma3_exc_is_exception(json_get_str(v, "__raise__", ""));
}

/* -- truthiness / access -- */
static bool ma3_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ma3_truthy(const json_t *v) {
    if (ma3_is_null(v)) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *ma3_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
/* Read the STRING VALUE of a node itself (not a key inside an object).
 * json_get_str(v,"",d) wrongly looks up key "" in v and returns d for any
 * scalar node -- the source of an entire mismatch cluster. */
static const char *ma3_strval(const json_t *v, const char *def) {
    const char *s = ma3_str(v);
    return s ? s : def;
}
static bool ma3_str_eq(const json_t *v, const char *s) {
    const char *t = ma3_str(v);
    return t && s && strcmp(t, s) == 0;
}
static const char *ma3_type_name(const json_t *v) {
    if (ma3_is_null(v)) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
static json_t *ma3_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *ma3_make_exc(const json_t *spec) {
    return ma3_err(json_get_str(spec, "type", "RuntimeError"),
                   json_get_str(spec, "message", ""));
}

/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *ma3_call(ma3_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ma3_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ma3_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) ret = ma3_make_exc(raise);
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

/* -- string helpers -- */
static char *ma3_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static bool ma3_is_ascii_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static char *ma3_py_str(const json_t *v) {
    if (ma3_is_null(v)) return ma3_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return ma3_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return ma3_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else
                snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return ma3_strdup(buf);
        }
        default: return ma3_strdup("");
    }
}
/* getattr(obj,key,default) then str(); obj is a plain event object. */
static char *ma3_getattr_str(const json_t *obj, const char *key, const char *def) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (!v) return ma3_strdup(def);
    return ma3_py_str(v);
}

/* -- attribute-double access ({"__attr__": {attrs}}) -- */
static const json_t *ma3_attr(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    const json_t *in = json_obj_get(obj, "__attr__");
    if (in && in->type == JSON_OBJECT) return json_obj_get(in, key);
    return json_obj_get(obj, key);
}
/* -- prompt objects: {"__attr__": {attrs}} -- */
static const json_t *ma3_pinner(const json_t *p) {
    return (p && p->type == JSON_OBJECT) ? json_obj_get(p, "__attr__") : NULL;
}
static const json_t *ma3_pget(const json_t *p, const char *k) {
    const json_t *in = ma3_pinner(p);
    return in ? json_obj_get(in, k) : NULL;
}
static void ma3_pset(json_t *p, const char *k, json_t *v) {
    json_t *in = (json_t *)ma3_pinner(p);
    if (in && v) json_set(in, k, v); else json_free(v);
}
static json_t *ma3_pproj(const json_t *p) {
    const json_t *in = ma3_pinner(p);
    return in ? json_copy(in) : json_object();
}

/* -- self / state access -- */
static const json_t *ma3_self_attr(const json_t *args, const char *name) {
    const json_t *slf = json_obj_get(args, "self");
    return slf ? ma3_attr(slf, name) : NULL;
}
static json_t *ma3_state_get(ma3_ctx_t *ctx, const char *key) {
    return json_obj_get(ctx->state, key);
}
static const json_t *ma3_registry(const json_t *args) {
    static const char *keys[] = {
        "_approval_prompts_by_event", "_approval_prompt_by_session",
        "_model_picker_prompts_by_event", "_choice_picker_prompts_by_event", NULL};
    const json_t *slf = json_obj_get(args, "self");
    if (!slf || slf->type != JSON_OBJECT) return NULL;
    for (int i = 0; keys[i]; i++) {
        const json_t *r = ma3_attr(slf, keys[i]);
        if (r && r->type == JSON_OBJECT) return r;
    }
    return NULL;
}

/* -- sorted set (warned rooms) -- */
typedef struct { char **v; size_t n, cap; } ma3_strvec_t;
static void ma3_sv_push(ma3_strvec_t *sv, char *s) {
    if (sv->n == sv->cap) {
        sv->cap = sv->cap ? sv->cap * 2 : 8;
        sv->v = realloc(sv->v, sv->cap * sizeof(char *));
    }
    sv->v[sv->n++] = s;
}
static int ma3_sv_cmp(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}
static json_t *ma3_sorted_copy(const json_t *arr) {
    ma3_strvec_t sv = {NULL, 0, 0};
    if (arr && arr->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(arr); i++) {
            const char *s = ma3_str(json_get(arr, i));
            ma3_sv_push(&sv, ma3_strdup(s ? s : ""));
        }
    if (sv.n > 1) qsort(sv.v, sv.n, sizeof(char *), ma3_sv_cmp);
    json_t *out = json_array();
    for (size_t i = 0; i < sv.n; i++) {
        if (i == 0 || strcmp(sv.v[i], sv.v[i - 1]) != 0)
            json_append(out, json_string(sv.v[i]));
        free(sv.v[i]);
    }
    free(sv.v);
    return out;
}
static bool ma3_arr_contains(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < json_len(arr); i++)
        if (ma3_str_eq(json_get(arr, i), s)) return true;
    return false;
}
static void ma3_set_add(json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return;
    if (!ma3_arr_contains(arr, s)) json_append(arr, json_string(s));
}
static json_t *ma3_state_array(ma3_ctx_t *ctx, const char *key) {
    json_t *a = json_obj_get(ctx->state, key);
    if (!a || a->type != JSON_ARRAY) {
        a = json_array();
        json_set(ctx->state, key, a);
    }
    return a;
}

/* str((room_id, msg_id)) -- simple-ASCII repr domain */
static char *ma3_tuple_key(const char *room, const char *msg) {
    char buf[768];
    snprintf(buf, sizeof buf, "('%s', '%s')", room ? room : "", msg ? msg : "");
    return ma3_strdup(buf);
}

/* ========================= ported flows ========================= */

/* PoP: ma3_warn_encrypted_drops @ plugins/platforms/matrix/adapter.py:_warn_encrypted_drops */
static json_t *ma3_warn_encrypted_drops(ma3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *rooms = json_obj_get(args, "rooms_join");
    if (ma3_is_null(rooms)) return ma3_err("AttributeError",
        "'NoneType' object has no attribute 'items'");
    if (rooms->type != JSON_OBJECT) {
        char m[160];
        snprintf(m, sizeof m, "'%s' object has no attribute 'items'", ma3_type_name(rooms));
        return ma3_err("AttributeError", m);
    }
    const json_t *client = json_obj_get(args, "client");
    const json_t *crypto = ma3_attr(client, "crypto");
    if (!ma3_is_null(crypto)) return json_null();  /* decryptor attached */

    /* self._warned_encrypted_drop_rooms (set -> sorted array state) */
    const json_t *self = json_obj_get(args, "self");
    const json_t *warned = ma3_attr(self, "_warned_encrypted_drop_rooms");
    if (warned && warned->type != JSON_ARRAY) {
        char m[200];
        snprintf(m, sizeof m, "'%s' object has no attribute 'add'", ma3_type_name(warned));
        return ma3_err("AttributeError", m);
    }
    json_t *ws = ma3_state_array(ctx, "_warned_encrypted_drop_rooms");
    if (warned && warned->type == JSON_ARRAY && json_len(warned) > 0 && json_len(ws) == 0) {
        for (size_t i = 0; i < json_len(warned); i++) {
            const char *s = ma3_str(json_get(warned, i));
            if (s) ma3_set_add(ws, s);
        }
    }
    const json_t *mode = ma3_attr(self, "_e2ee_mode");
    if (!mode) return ma3_err("AttributeError",
        "'types.SimpleNamespace' object has no attribute '_e2ee_mode'");

    for (size_t i = 0; i < json_object_size(rooms); i++) {
        const char *room_id = json_object_get_key_at(rooms, i);
        if (ma3_arr_contains(ws, room_id)) continue;
        const json_t *rd = json_object_get_at(rooms, i);
        if (!rd || rd->type != JSON_OBJECT) {
            char m[180];
            snprintf(m, sizeof m, "'%s' object has no attribute 'get'", ma3_type_name(rd));
            return ma3_err("AttributeError", m);
        }
        const json_t *timeline = json_obj_get(rd, "timeline");
        if (ma3_is_null(timeline)) timeline = NULL;  /* .get("timeline", {}) */
        if (timeline && timeline->type != JSON_OBJECT) {
            char m[180];
            snprintf(m, sizeof m, "'%s' object has no attribute 'get'", ma3_type_name(timeline));
            return ma3_err("AttributeError", m);
        }
        const json_t *events = timeline ? json_obj_get(timeline, "events") : NULL;
        if (events && events->type != JSON_ARRAY) {
            if (events->type == JSON_NUMBER || events->type == JSON_BOOL) {
                char m[180];
                snprintf(m, sizeof m, "'%s' object is not iterable", ma3_type_name(events));
                return ma3_err("TypeError", m);
            }
            events = NULL;
        }
        bool encrypted = false;
        if (events) {
            for (size_t j = 0; j < json_len(events); j++) {
                const json_t *ev = json_get(events, j);
                if (ev && ev->type == JSON_OBJECT && ma3_str_eq(json_obj_get(ev, "type"),
                                                                 "m.room.encrypted")) {
                    encrypted = true;
                    break;
                }
            }
        }
        if (encrypted) ma3_set_add(ws, room_id);
        /* cause string + logger.warning are unobservable */
    }
    return json_null();
}

/* PoP: ma3_is_allowed_matrix_room_event @ plugins/platforms/matrix/adapter.py:_is_allowed_matrix_room_event */
static json_t *ma3_is_allowed_matrix_room_event(ma3_ctx_t *ctx, const json_t *args) {
    const json_t *allowed = ma3_self_attr(args, "_allowed_room_ids");
    const char *room = json_get_str(args, "room_id", "");
    if (ma3_is_null(allowed) || allowed->type != JSON_ARRAY || json_len(allowed) == 0
            || ma3_arr_contains(allowed, room))
        return json_bool(true);
    json_t *a = json_object();
    json_set_hdr(a, "room_id", room);
    json_t *r = ma3_call(ctx, "self._is_dm_room", a);
    if (ma3_is_raise(r)) {
        if (ma3_raise_is_base(r)) return r;  /* BaseException passes through */
        json_free(r);
        return json_bool(false);            /* except Exception -> debug + False */
    }
    bool v = ma3_truthy(r);
    json_free(r);
    return json_bool(v);
}

/* PoP: ma3_redact_reaction @ plugins/platforms/matrix/adapter.py:_redact_reaction */
static json_t *ma3_redact_reaction(ma3_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set_hdr(a, "room_id", json_get_str(args, "room_id", ""));
    json_set_hdr(a, "event_id", json_get_str(args, "reaction_event_id", ""));
    json_set_hdr(a, "reason", json_get_str(args, "reason", ""));
    return ma3_call(ctx, "self.redact_message", a);
}

/* PoP: ma3_on_processing_start @ plugins/platforms/matrix/adapter.py:on_processing_start */
static json_t *ma3_on_processing_start(ma3_ctx_t *ctx, const json_t *args) {
    const json_t *ev = json_obj_get(args, "event");
    char *msg_id = ma3_getattr_str(ev, "message_id", "");
    const json_t *src = ev ? json_obj_get(ev, "source") : NULL;
    char *room = ma3_getattr_str(src, "chat_id", "");
    bool enabled = ma3_truthy(ma3_self_attr(args, "_reactions_enabled"));
    if (!enabled || !msg_id[0] || !room[0]) {
        free(msg_id); free(room);
        return json_null();
    }
    json_t *a = json_object();
    json_set_hdr(a, "room_id", room);
    json_set_hdr(a, "event_id", msg_id);
    json_set(a, "emoji", json_string("\xF0\x9F\x91\x80"));  /* eyes */
    json_t *r = ma3_call(ctx, "self._send_reaction", a);
    if (ma3_is_raise(r)) { free(msg_id); free(room); return r; }
    if (ma3_truthy(r)) {
        char *key = ma3_tuple_key(room, msg_id);
        json_t *pending = ma3_state_get(ctx, "_pending_reactions");
        if (!pending || pending->type != JSON_OBJECT) {
            pending = json_object();
            json_set(ctx->state, "_pending_reactions", pending);
        }
        json_set_hdr(pending, key, ma3_str(r));
        free(key);
    }
    json_free(r);
    free(msg_id); free(room);
    return json_null();
}

/* PoP: ma3_on_processing_complete @ plugins/platforms/matrix/adapter.py:on_processing_complete */
static json_t *ma3_on_processing_complete(ma3_ctx_t *ctx, const json_t *args) {
    const json_t *ev = json_obj_get(args, "event");
    char *msg_id = ma3_getattr_str(ev, "message_id", "");
    const json_t *src = ev ? json_obj_get(ev, "source") : NULL;
    char *room = ma3_getattr_str(src, "chat_id", "");
    const char *outcome = json_get_str(args, "outcome", "");
    bool enabled = ma3_truthy(ma3_self_attr(args, "_reactions_enabled"));
    if (!enabled || !msg_id[0] || !room[0] || strcmp(outcome, "cancelled") == 0) {
        free(msg_id); free(room);
        return json_null();
    }
    char *key = ma3_tuple_key(room, msg_id);
    json_t *pending = ma3_state_get(ctx, "_pending_reactions");
    if (!pending || pending->type != JSON_OBJECT) {
        pending = json_object();
        json_set(ctx->state, "_pending_reactions", pending);
    }
    const json_t *eyes = json_obj_get(pending, key);
    if (eyes && !ma3_is_null(eyes)) {
        char *eid = ma3_py_str(eyes);
        json_t *sc = json_object();
        json_set_hdr(sc, "room_id", room);
        json_set_hdr(sc, "reaction_event_id", eid);
        json_set_hdr(sc, "reason", "processing complete");
        json_obj_del(pending, key);
        free(eid);
        json_t *r = ma3_call(ctx, "self._schedule_reaction_redaction", sc);
        if (ma3_is_raise(r)) { free(key); free(msg_id); free(room); return r; }
        json_free(r);
    }
    json_t *a = json_object();
    json_set_hdr(a, "room_id", room);
    json_set_hdr(a, "event_id", msg_id);
    json_set(a, "emoji", json_string(strcmp(outcome, "success") == 0
                                     ? "\xE2\x9C\x85" : "\xE2\x9D\x8C"));
    free(key);
    json_t *r = ma3_call(ctx, "self._send_reaction", a);
    free(msg_id); free(room);
    if (ma3_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* handler seam args: self._handle_*_reaction(room, reacts_to, key, sender) */
static json_t *_hargs(const char *room, const char *reacts_to,
                      const char *key, const char *sender) {
    json_t *a = json_object();
    json_set_hdr(a, "room_id", room);
    json_set_hdr(a, "reacts_to", reacts_to);
    json_set_hdr(a, "key", key);
    json_set_hdr(a, "sender", sender);
    return a;
}

/* PoP: ma3_on_reaction @ plugins/platforms/matrix/adapter.py:_on_reaction */
static json_t *ma3_on_reaction(ma3_ctx_t *ctx, const json_t *args) {
    const json_t *ev = json_obj_get(args, "event");
    char *sender = ma3_getattr_str(ev, "sender", "");
    char *eid = ma3_getattr_str(ev, "event_id", "");
    char *room = ma3_getattr_str(ev, "room_id", "");
    json_t *a = json_object();
    json_set_hdr(a, "sender", sender);
    json_t *r = ma3_call(ctx, "self._is_self_sender", a);
    if (ma3_is_raise(r)) { free(sender); free(eid); free(room); return r; }
    bool self = ma3_truthy(r); json_free(r);
    if (self) { free(sender); free(eid); free(room); return json_null(); }
    a = json_object(); json_set_hdr(a, "event_id", eid);
    r = ma3_call(ctx, "self._is_duplicate_event", a);
    if (ma3_is_raise(r)) { free(sender); free(eid); free(room); return r; }
    bool dup = ma3_truthy(r); json_free(r);
    if (dup) { free(sender); free(eid); free(room); return json_null(); }

    const json_t *content = ev ? json_obj_get(ev, "content") : NULL;
    if (ma3_is_null(content) || (content && content->type == JSON_OBJECT && content->c.count == 0)) {
        free(sender); free(eid); free(room);
        return json_null();
    }
    const json_t *rel = content ? json_obj_get(content, "m.relates_to") : NULL;
    if (!rel || rel->type != JSON_OBJECT) rel = NULL;
    char *reacts_to = rel ? ma3_getattr_str(rel, "event_id", "") : ma3_strdup("");
    char *key = rel ? ma3_getattr_str(rel, "key", "") : ma3_strdup("");

    a = json_object(); json_set_hdr(a, "room_id", room);
    r = ma3_call(ctx, "hash_for_log", a);
    if (ma3_is_raise(r)) { free(sender); free(eid); free(room); free(reacts_to); free(key); return r; }
    json_free(r);

    /* for handler in (approval, model-picker, choice-picker): if await handler(...): return */
    r = ma3_call(ctx, "self._handle_approval_reaction", _hargs(room, reacts_to, key, sender));
    if (ma3_is_raise(r)) goto handler_raise;
    if (ma3_truthy(r)) { json_free(r); goto handler_done; }
    json_free(r);
    r = ma3_call(ctx, "self._handle_model_picker_reaction", _hargs(room, reacts_to, key, sender));
    if (ma3_is_raise(r)) goto handler_raise;
    if (ma3_truthy(r)) { json_free(r); goto handler_done; }
    json_free(r);
    r = ma3_call(ctx, "self._handle_choice_picker_reaction", _hargs(room, reacts_to, key, sender));
    if (ma3_is_raise(r)) goto handler_raise;
    if (ma3_truthy(r)) { json_free(r); goto handler_done; }
    json_free(r);
    goto handler_done;
handler_raise:
    free(sender); free(eid); free(room); free(reacts_to); free(key);
    return r;
handler_done:
    free(sender); free(eid); free(room); free(reacts_to); free(key);
    return json_null();
}

/* PoP: ma3_claim_reaction_prompt @ plugins/platforms/matrix/adapter.py:_claim_reaction_prompt */
static json_t *ma3_claim_reaction_prompt(ma3_ctx_t *ctx, const json_t *args) {
    const json_t *registry = ma3_registry(args);
    const json_t *reacts_to_v = json_obj_get(args, "reacts_to");
    const char *reacts_to = json_get_str(args, "reacts_to", "");
    const json_t *prompt = registry ? json_obj_get(registry, reacts_to) : NULL;
    if (ma3_is_null(prompt) || prompt->type != JSON_OBJECT) {
        json_t *out = json_array();
        json_append(out, json_bool(false));
        json_append(out, json_null());
        json_append(out, json_null());
        return out;
    }
    if (ma3_truthy(ma3_pget(prompt, "resolved"))) {
        json_t *out = json_array();
        json_append(out, json_bool(false));
        json_append(out, json_null());
        json_append(out, json_null());
        return out;
    }

    const char *room = json_get_str(args, "room_id", "");
    const json_t *chat = ma3_pget(prompt, "chat_id");
    const char *chat_s = ma3_str(chat);
    if (!chat_s || strcmp(chat_s, room) != 0) {
        json_t *out = json_array();
        json_append(out, json_bool(true));
        json_append(out, ma3_pproj(prompt));
        json_append(out, json_null());
        return out;
    }
    json_t *pa = json_object(); json_set(pa, "prompt", ma3_pproj(prompt));
    json_t *r = ma3_call(ctx, "self._matrix_prompt_expired", pa);
    if (ma3_is_raise(r)) return r;
    bool expired = ma3_truthy(r); json_free(r);
    if (expired) {
        json_t *ea = json_object();
        json_set_hdr(ea, "room_id", room);
        json_set_hdr(ea, "target_event_id", reacts_to);
        json_set(ea, "prompt", ma3_pproj(prompt));
        r = ma3_call(ctx, "on_expired", ea);
        if (ma3_is_raise(r)) return r;
        json_free(r);
        json_t *out = json_array();
        json_append(out, json_bool(true));
        json_append(out, ma3_pproj(prompt));
        json_append(out, json_null());
        return out;
    }
    json_t *va = json_object();
    json_set_hdr(va, "room_id", room);
    json_set_hdr(va, "target_event_id", reacts_to);
    json_set_hdr(va, "sender", json_get_str(args, "sender", ""));
    json_set(va, "prompt", ma3_pproj(prompt));
    json_set_hdr(va, "prompt_label", json_get_str(args, "label", ""));
    r = ma3_call(ctx, "self._validate_matrix_prompt_reactor", va);
    if (ma3_is_raise(r)) return r;
    bool valid = ma3_truthy(r); json_free(r);
    if (!valid) {
        json_t *out = json_array();
        json_append(out, json_bool(true));
        json_append(out, ma3_pproj(prompt));
        json_append(out, json_null());
        return out;
    }
    const json_t *choices = json_obj_get(args, "choices");
    if (ma3_is_null(choices)) choices = ma3_pget(prompt, "choices");
    const json_t *selection = choices ? json_obj_get(choices, json_get_str(args, "key", "")) : NULL;
    if (ma3_is_null(selection)) {
        json_t *fa = json_object();
        json_set_hdr(fa, "room_id", room);
        json_set_hdr(fa, "target_event_id", reacts_to);
        json_set_hdr(fa, "text", json_get_str(args, "invalid_text", ""));
        r = ma3_call(ctx, "self._send_invalid_reaction_feedback", fa);
        if (ma3_is_raise(r)) return r;
        json_free(r);
    }
    json_t *out = json_array();
    json_append(out, json_bool(true));
    json_append(out, ma3_pproj(prompt));
    json_append(out, selection ? json_copy(selection) : json_null());
    (void)reacts_to_v;
    return out;
}

/* PoP: ma3_handle_approval_reaction @ plugins/platforms/matrix/adapter.py:_handle_approval_reaction */
static json_t *ma3_handle_approval_reaction(ma3_ctx_t *ctx, const json_t *args) {
    const char *room = json_get_str(args, "room_id", "");
    const char *reacts_to = json_get_str(args, "reacts_to", "");
    const char *key = json_get_str(args, "key", "");
    const char *sender = json_get_str(args, "sender", "");
    /* t(invalid_reaction) is evaluated before the claim call */
    json_t *ta = json_object();
    json_set_hdr(ta, "key", "platform.matrix.approval.invalid_reaction");
    json_t *t = ma3_call(ctx, "t", ta);
    if (ma3_is_raise(t)) return t;
    char *invalid = ma3_py_str(t);
    json_free(t);

    const json_t *self = json_obj_get(args, "self");
    const json_t *registry = ma3_registry(args);
    const json_t *choices = ma3_self_attr(args, "_approval_reaction_map");
    json_t *ca = json_object();
    json_set(ca, "registry", registry ? json_copy(registry) : json_null());
    json_set_hdr(ca, "room_id", room);
    json_set_hdr(ca, "reacts_to", reacts_to);
    json_set_hdr(ca, "key", key);
    json_set_hdr(ca, "sender", sender);
    json_set_hdr(ca, "label", "approval");
    json_set_hdr(ca, "invalid_text", invalid);
    json_set(ca, "choices", choices ? json_copy(choices) : json_null());
    free(invalid);
    json_t *claim = ma3_call(ctx, "self._claim_reaction_prompt", ca);
    if (ma3_is_raise(claim)) return claim;
    json_t *choice = json_is_array(claim) ? json_get(claim, 2) : NULL;
    bool handled = ma3_truthy(json_is_array(claim) ? json_get(claim, 0) : NULL);
    json_t *prompt = json_is_array(claim) ? json_get(claim, 1) : NULL;
    if (ma3_is_null(choice)) {
        json_t *out = json_bool(handled);
        json_free(claim);
        return out;
    }
    const json_t *offered = ma3_pget(prompt, "offered");
    if (ma3_truthy(offered) && offered->type == JSON_OBJECT
            && !json_has(offered, ma3_strval(choice, ""))) {
        json_t *ta2 = json_object();
        json_set_hdr(ta2, "key", "platform.matrix.approval.invalid_reaction");
        json_t *t2 = ma3_call(ctx, "t", ta2);
        if (ma3_is_raise(t2)) { json_free(claim); return t2; }
        char *txt = ma3_py_str(t2); json_free(t2);
        json_t *fa = json_object();
        json_set_hdr(fa, "room_id", room);
        json_set_hdr(fa, "target_event_id", reacts_to);
        json_set_hdr(fa, "text", txt);
        free(txt);
        json_t *rr = ma3_call(ctx, "self._send_invalid_reaction_feedback", fa);
        json_free(claim);
        if (ma3_is_raise(rr)) return rr;
        json_free(rr);
        return json_bool(true);
    }
    const json_t *extra = ma3_attr(ma3_self_attr(args, "config"), "extra");
    json_t *ra = json_object();
    json_set_hdr(ra, "session_key", ma3_strval(ma3_pget(prompt, "session_key"), ""));
    json_set_hdr(ra, "sender", sender);
    json_set(ra, "extra", extra ? json_copy(extra) : json_null());
    json_set_hdr(ra, "request_id", ma3_strval(ma3_pget(prompt, "request_id"), ""));
    json_set(ra, "choice", json_copy(choice));
    json_t *requester = ma3_call(ctx, "approval_refusal", ra);
    if (ma3_is_raise(requester)) { json_free(claim); return requester; }
    if (ma3_truthy(requester)) {
        json_t *ra2 = json_object();
        json_set(ra2, "requester", json_copy(requester));
        json_t *rt = ma3_call(ctx, "refusal_text", ra2);
        if (ma3_is_raise(rt)) { json_free(requester); json_free(claim); return rt; }
        char *txt = ma3_py_str(rt); json_free(rt); json_free(requester);
        json_t *fa = json_object();
        json_set_hdr(fa, "room_id", room);
        json_set_hdr(fa, "target_event_id", reacts_to);
        json_set_hdr(fa, "text", txt);
        free(txt);
        json_t *rr = ma3_call(ctx, "self._send_invalid_reaction_feedback", fa);
        json_free(claim);
        if (ma3_is_raise(rr)) return rr;
        json_free(rr);
        return json_bool(true);
    }
    json_free(requester);
    /* try: resolve_gateway_approval_card(...) */
    json_t *ga = json_object();
    json_set_hdr(ga, "session_key", ma3_strval(ma3_pget(prompt, "session_key"), ""));
    json_set(ga, "choice", json_copy(choice));
    json_set_hdr(ga, "request_id", ma3_strval(ma3_pget(prompt, "request_id"), ""));
    json_set_hdr(ga, "actor_id", sender);
    json_set(ga, "extra", extra ? json_copy(extra) : json_null());
    json_t *count = ma3_call(ctx, "resolve_gateway_approval_card", ga);
    json_t *post = NULL;
    if (ma3_is_raise(count)) {
        if (ma3_raise_is_base(count)) { json_free(claim); return count; }
        json_free(count);  /* except Exception: logger.error -> True */
        return json_bool(true);
    }
    if (ma3_truthy(count)) {
        ma3_pset((json_t *)prompt, "resolved", json_bool(true));
        json_t *ba = json_object();
        json_set_hdr(ba, "room_id", room);
        json_set(ba, "prompt", ma3_pproj(prompt));
        post = ma3_call(ctx, "self._redact_bot_approval_reactions", ba);
    } else {
        json_t *ea = json_object();
        json_set_hdr(ea, "room_id", room);
        json_set_hdr(ea, "target_event_id", reacts_to);
        json_set(ea, "prompt", ma3_pproj(prompt));
        post = ma3_call(ctx, "self._expire_matrix_approval_prompt", ea);
    }
    json_free(count);
    json_free(claim);
    if (ma3_is_raise(post)) {
        if (!ma3_raise_is_base(post)) { json_free(post); return json_bool(true); }
        return post;  /* BaseException inside try propagates */
    }
    json_free(post);
    (void)self;
    return json_bool(true);
}

/* PoP: ma3_handle_model_picker_reaction @ plugins/platforms/matrix/adapter.py:_handle_model_picker_reaction */
static json_t *ma3_handle_model_picker_reaction(ma3_ctx_t *ctx, const json_t *args) {
    json_t *ta = json_object();
    json_set_hdr(ta, "key", "platform.matrix.picker.invalid_model_reaction");
    json_t *t = ma3_call(ctx, "t", ta);
    if (ma3_is_raise(t)) return t;
    char *invalid = ma3_py_str(t); json_free(t);
    const json_t *registry = ma3_registry(args);
    json_t *a = json_object();
    json_set(a, "registry", registry ? json_copy(registry) : json_null());
    json_set_hdr(a, "room_id", json_get_str(args, "room_id", ""));
    json_set_hdr(a, "reacts_to", json_get_str(args, "reacts_to", ""));
    json_set_hdr(a, "key", json_get_str(args, "key", ""));
    json_set_hdr(a, "sender", json_get_str(args, "sender", ""));
    json_set_hdr(a, "label", "model picker");
    json_set_hdr(a, "invalid_text", invalid);
    free(invalid);
    /* verbs tuple + redact_bot_reactions=True are carried in the seam args */
    json_t *verbs = json_array();
    json_append(verbs, json_string("switch model"));
    json_append(verbs, json_string("platform.matrix.picker.verb_switch_model"));
    json_set(a, "verbs", verbs);
    json_set(a, "choices", json_null());
    json_set(a, "redact_bot_reactions", json_bool(true));
    json_t *r = ma3_call(ctx, "self._handle_picker_reaction", a);
    return r;
}

/* PoP: ma3_handle_choice_picker_reaction @ plugins/platforms/matrix/adapter.py:_handle_choice_picker_reaction */
static json_t *ma3_handle_choice_picker_reaction(ma3_ctx_t *ctx, const json_t *args) {
    json_t *ta = json_object();
    json_set_hdr(ta, "key", "platform.matrix.picker.invalid_choice_reaction");
    json_t *t = ma3_call(ctx, "t", ta);
    if (ma3_is_raise(t)) return t;
    char *invalid = ma3_py_str(t); json_free(t);
    const json_t *registry = ma3_registry(args);
    json_t *a = json_object();
    json_set(a, "registry", registry ? json_copy(registry) : json_null());
    json_set_hdr(a, "room_id", json_get_str(args, "room_id", ""));
    json_set_hdr(a, "reacts_to", json_get_str(args, "reacts_to", ""));
    json_set_hdr(a, "key", json_get_str(args, "key", ""));
    json_set_hdr(a, "sender", json_get_str(args, "sender", ""));
    json_set_hdr(a, "label", "choice picker");
    json_set_hdr(a, "invalid_text", invalid);
    free(invalid);
    json_t *verbs = json_array();
    json_append(verbs, json_string("apply choice"));
    json_append(verbs, json_string("platform.matrix.picker.verb_apply_selection"));
    json_set(a, "verbs", verbs);
    json_set(a, "choices", json_null());
    json_set(a, "redact_bot_reactions", json_bool(false));
    return ma3_call(ctx, "self._handle_picker_reaction", a);
}

/* PoP: ma3_handle_picker_reaction @ plugins/platforms/matrix/adapter.py:_handle_picker_reaction */
static json_t *ma3_handle_picker_reaction(ma3_ctx_t *ctx, const json_t *args) {
    const char *room = json_get_str(args, "room_id", "");
    const char *reacts_to = json_get_str(args, "reacts_to", "");
    const json_t *registry = ma3_registry(args);
    json_t *ca = json_object();
    json_set(ca, "registry", registry ? json_copy(registry) : json_null());
    json_set_hdr(ca, "room_id", room);
    json_set_hdr(ca, "reacts_to", reacts_to);
    json_set_hdr(ca, "key", json_get_str(args, "key", ""));
    json_set_hdr(ca, "sender", json_get_str(args, "sender", ""));
    json_set_hdr(ca, "label", json_get_str(args, "label", ""));
    json_set_hdr(ca, "invalid_text", json_get_str(args, "invalid_text", ""));
    json_set(ca, "choices", json_null());
    json_t *claim = ma3_call(ctx, "self._claim_reaction_prompt", ca);
    if (ma3_is_raise(claim)) return claim;
    json_t *selection = json_is_array(claim) ? json_get(claim, 2) : NULL;
    bool handled = ma3_truthy(json_is_array(claim) ? json_get(claim, 0) : NULL);
    json_t *prompt = json_is_array(claim) ? json_get(claim, 1) : NULL;
    if (ma3_is_null(selection)) {
        json_t *out = json_bool(handled); json_free(claim); return out;
    }
    /* prompt.resolved = True; registry.pop(reacts_to, None) */
    ma3_pset((json_t *)prompt, "resolved", json_bool(true));

    /* args = selection if tuple else (selection,); scripted JSON arrays are
     * Python lists, never tuples, so the whole value is ONE positional arg. */
    json_t *sel_args = json_array();
    json_append(sel_args, json_copy(selection));
    json_t *oa = json_object();
    json_set_hdr(oa, "room_id", room);
    json_set(oa, "args", sel_args);
    json_t *confirmation = ma3_call(ctx, "prompt.on_selected", oa);
    json_t *post = NULL;
    if (ma3_is_raise(confirmation)) {
        if (ma3_raise_is_base(confirmation)) { json_free(claim); return confirmation; }
        char *msg = ma3_strdup(json_get_str(confirmation, "message", ""));
        json_free(confirmation);
        /* except Exception: inner t(verbs[1]), outer t(failed), send */
        const json_t *verbs = json_obj_get(args, "verbs");
        const char *v1 = (verbs && verbs->type == JSON_ARRAY && json_len(verbs) > 1)
                         ? ma3_strval(json_get(verbs, 1), "") : "";
        json_t *it = json_object(); json_set_hdr(it, "key", v1);
        json_t *inner = ma3_call(ctx, "t", it);
        if (ma3_is_raise(inner)) { free(msg); json_free(claim); return inner; }
        char *action = ma3_py_str(inner); json_free(inner);
        json_t *ot = json_object();
        json_set_hdr(ot, "key", "platform.matrix.picker.failed");
        json_set_hdr(ot, "action", action);
        json_set_hdr(ot, "error", msg);
        free(action); free(msg);
        json_t *outer = ma3_call(ctx, "t", ot);
        if (ma3_is_raise(outer)) { json_free(claim); return outer; }
        char *text = ma3_py_str(outer); json_free(outer);
        json_t *sa = json_object();
        json_set_hdr(sa, "room_id", room);
        json_set_hdr(sa, "text", text);
        json_set_hdr(sa, "reply_to", reacts_to);
        free(text);
        post = ma3_call(ctx, "self.send", sa);
        if (ma3_is_raise(post)) { json_free(claim); return post; }
        json_free(post);
        json_free(claim);
        return json_bool(true);
    }
    if (ma3_truthy(json_obj_get(args, "redact_bot_reactions"))) {
        json_t *ba = json_object();
        json_set_hdr(ba, "room_id", room);
        json_set(ba, "prompt", ma3_pproj(prompt));
        post = ma3_call(ctx, "self._redact_bot_model_picker_reactions", ba);
        if (ma3_is_raise(post)) {
            if (ma3_raise_is_base(post)) { json_free(confirmation); json_free(claim); return post; }
            /* swallowed by except Exception; still send confirmation below */
            json_free(post);
        }
    }
    if (ma3_truthy(confirmation)) {
        json_t *sa = json_object();
        json_set_hdr(sa, "room_id", room);
        json_set(sa, "text", json_copy(confirmation));
        json_set_hdr(sa, "reply_to", reacts_to);
        json_t *sent = ma3_call(ctx, "self.send", sa);
        if (ma3_is_raise(sent)) {
            if (ma3_raise_is_base(sent)) { json_free(confirmation); json_free(claim); return sent; }
            /* success-path send failed -> the try/except Exception handler runs */
            char *smsg = ma3_strdup(json_get_str(sent, "message", ""));
            json_free(sent);
            const json_t *vbs = json_obj_get(args, "verbs");
            const char *v1b = (vbs && vbs->type == JSON_ARRAY && json_len(vbs) > 1)
                              ? ma3_strval(json_get(vbs, 1), "") : "";
            json_t *it2 = json_object(); json_set_hdr(it2, "key", v1b);
            json_t *in2 = ma3_call(ctx, "t", it2);
            if (ma3_is_raise(in2)) { free(smsg); json_free(confirmation); json_free(claim); return in2; }
            char *act2 = ma3_py_str(in2); json_free(in2);
            json_t *ot2 = json_object();
            json_set_hdr(ot2, "key", "platform.matrix.picker.failed");
            json_set_hdr(ot2, "action", act2);
            json_set_hdr(ot2, "error", smsg);
            free(act2); free(smsg);
            json_t *ou2 = ma3_call(ctx, "t", ot2);
            if (ma3_is_raise(ou2)) { json_free(confirmation); json_free(claim); return ou2; }
            char *txt2 = ma3_py_str(ou2); json_free(ou2);
            json_t *sa2 = json_object();
            json_set_hdr(sa2, "room_id", room);
            json_set_hdr(sa2, "text", txt2);
            json_set_hdr(sa2, "reply_to", reacts_to);
            free(txt2);
            json_t *er = ma3_call(ctx, "self.send", sa2);
            json_free(confirmation); json_free(claim);
            if (ma3_is_raise(er)) return er;
            json_free(er);
            return json_bool(true);
        }
        json_free(sent);
    }
    json_free(confirmation);
    json_free(claim);
    return json_bool(true);
}

/* PoP: ma3_validate_matrix_prompt_reactor @ plugins/platforms/matrix/adapter.py:_validate_matrix_prompt_reactor */
static json_t *ma3_validate_matrix_prompt_reactor(ma3_ctx_t *ctx, const json_t *args) {
    const char *room = json_get_str(args, "room_id", "");
    const char *target = json_get_str(args, "target_event_id", "");
    const char *sender = json_get_str(args, "sender", "");
    const char *label = json_get_str(args, "prompt_label", "");
    const json_t *prompt = json_obj_get(args, "prompt");
    json_t *aa = json_object();
    json_set_hdr(aa, "user_id", sender);
    json_set_hdr(aa, "room_id", room);
    json_t *r = ma3_call(ctx, "self._is_authorized_user", aa);
    if (ma3_is_raise(r)) return r;
    bool auth = ma3_truthy(r); json_free(r);
    if (!auth) {
        json_t *ta = json_object();
        json_set_hdr(ta, "key", "platform.matrix.reaction.unauthorized");
        json_t *t = ma3_call(ctx, "t", ta);
        if (ma3_is_raise(t)) return t;
        char *txt = ma3_py_str(t); json_free(t);
        json_t *fa = json_object();
        json_set_hdr(fa, "room_id", room);
        json_set_hdr(fa, "target_event_id", target);
        json_set_hdr(fa, "text", txt);
        free(txt);
        json_t *fb = ma3_call(ctx, "self._send_invalid_reaction_feedback", fa);
        if (ma3_is_raise(fb)) return fb;
        json_free(fb);
        return json_bool(false);
    }
    const json_t *require = ma3_self_attr(args, "_approval_require_sender");
    bool require_sender = ma3_is_null(require) ? true : ma3_truthy(require);
    const json_t *requester = ma3_pget(prompt, "requester_user_id");
    if (require_sender && !ma3_is_null(requester) && strcmp(sender, ma3_strval(requester, "")) != 0) {
        json_t *ta = json_object();
        json_set_hdr(ta, "key", "platform.matrix.reaction.not_requester");
        json_t *t = ma3_call(ctx, "t", ta);
        if (ma3_is_raise(t)) return t;
        char *txt = ma3_py_str(t); json_free(t);
        json_t *fa = json_object();
        json_set_hdr(fa, "room_id", room);
        json_set_hdr(fa, "target_event_id", target);
        json_set_hdr(fa, "text", txt);
        free(txt);
        json_t *fb = ma3_call(ctx, "self._send_invalid_reaction_feedback", fa);
        if (ma3_is_raise(fb)) return fb;
        json_free(fb);
        return json_bool(false);
    }
    const json_t *extra = ma3_attr(ma3_self_attr(args, "config"), "extra");
    json_t *pa = json_object();
    const json_t *onsel = ma3_pget(prompt, "on_selected");
    json_set_hdr(pa, "on_selected", ma3_strval(onsel, ""));
    json_set_hdr(pa, "sender", sender);
    json_set(pa, "extra", extra ? json_copy(extra) : json_null());
    json_t *opener = ma3_call(ctx, "picker_refusal", pa);
    if (ma3_is_raise(opener)) return opener;
    if (ma3_truthy(opener)) {
        json_t *rt = json_object();
        json_set(rt, "requester", json_copy(opener));
        json_t *txtv = ma3_call(ctx, "refusal_text", rt);
        if (ma3_is_raise(txtv)) { json_free(opener); return txtv; }
        char *txt = ma3_py_str(txtv); json_free(txtv); json_free(opener);
        json_t *fa = json_object();
        json_set_hdr(fa, "room_id", room);
        json_set_hdr(fa, "target_event_id", target);
        json_set_hdr(fa, "text", txt);
        free(txt);
        json_t *fb = ma3_call(ctx, "self._send_invalid_reaction_feedback", fa);
        if (ma3_is_raise(fb)) return fb;
        json_free(fb);
        return json_bool(false);
    }
    json_free(opener);
    (void)label;
    return json_bool(true);
}

/* PoP: ma3_send_invalid_reaction_feedback @ plugins/platforms/matrix/adapter.py:_send_invalid_reaction_feedback */
static json_t *ma3_send_invalid_reaction_feedback(ma3_ctx_t *ctx, const json_t *args) {
    json_t *sa = json_object();
    json_set_hdr(sa, "room_id", json_get_str(args, "room_id", ""));
    json_set_hdr(sa, "text", json_get_str(args, "text", ""));
    json_set_hdr(sa, "reply_to", json_get_str(args, "target_event_id", ""));
    json_t *r = ma3_call(ctx, "self.send", sa);
    if (ma3_is_raise(r)) {
        if (ma3_raise_is_base(r)) return r;   /* except Exception swallows only Exception */
        json_free(r);
    }
    return json_null();
}

/* PoP: ma3_expire_matrix_approval_prompt @ plugins/platforms/matrix/adapter.py:_expire_matrix_approval_prompt */
static json_t *ma3_expire_matrix_approval_prompt(ma3_ctx_t *ctx, const json_t *args) {
    const json_t *prompt = json_obj_get(args, "prompt");
    ma3_pset((json_t *)prompt, "resolved", json_bool(true));
    json_t *ba = json_object();
    json_set_hdr(ba, "room_id", json_get_str(args, "room_id", ""));
    json_set(ba, "prompt", ma3_pproj(prompt));
    json_t *r = ma3_call(ctx, "self._redact_bot_approval_reactions", ba);
    if (ma3_is_raise(r)) return r;
    json_free(r);
    json_t *ta = json_object();
    json_set_hdr(ta, "key", "platform.matrix.approval.expired");
    json_t *t = ma3_call(ctx, "t", ta);
    if (ma3_is_raise(t)) return t;
    char *txt = ma3_py_str(t); json_free(t);
    json_t *fa = json_object();
    json_set_hdr(fa, "room_id", json_get_str(args, "room_id", ""));
    json_set_hdr(fa, "target_event_id", json_get_str(args, "target_event_id", ""));
    json_set_hdr(fa, "text", txt);
    free(txt);
    json_t *fb = ma3_call(ctx, "self._send_invalid_reaction_feedback", fa);
    if (ma3_is_raise(fb)) return fb;
    json_free(fb);
    return json_null();
}

/* PoP: ma3_expire_matrix_model_picker_prompt @ plugins/platforms/matrix/adapter.py:_expire_matrix_model_picker_prompt */
static json_t *ma3_expire_matrix_model_picker_prompt(ma3_ctx_t *ctx, const json_t *args) {
    const json_t *prompt = json_obj_get(args, "prompt");
    ma3_pset((json_t *)prompt, "resolved", json_bool(true));
    json_t *ba = json_object();
    json_set_hdr(ba, "room_id", json_get_str(args, "room_id", ""));
    json_set(ba, "prompt", ma3_pproj(prompt));
    json_t *r = ma3_call(ctx, "self._redact_bot_model_picker_reactions", ba);
    if (ma3_is_raise(r)) return r;
    json_free(r);
    json_t *ta = json_object();
    json_set_hdr(ta, "key", "platform.shared.model_picker_expired");
    json_t *t = ma3_call(ctx, "t", ta);
    if (ma3_is_raise(t)) return t;
    char *txt = ma3_py_str(t); json_free(t);
    json_t *fa = json_object();
    json_set_hdr(fa, "room_id", json_get_str(args, "room_id", ""));
    json_set_hdr(fa, "target_event_id", json_get_str(args, "target_event_id", ""));
    json_set_hdr(fa, "text", txt);
    free(txt);
    json_t *fb = ma3_call(ctx, "self._send_invalid_reaction_feedback", fa);
    if (ma3_is_raise(fb)) return fb;
    json_free(fb);
    return json_null();
}

/* PoP: ma3_redact_bot_approval_reactions @ plugins/platforms/matrix/adapter.py:_redact_bot_approval_reactions */
static json_t *ma3_redact_bot_approval_reactions(ma3_ctx_t *ctx, const json_t *args) {
    const json_t *prompt = json_obj_get(args, "prompt");
    const json_t *bot = ma3_pget(prompt, "bot_reaction_events");
    if (!bot || bot->type != JSON_OBJECT) return json_null();
    for (size_t i = 0; i < json_object_size(bot); i++) {
        const json_t *evt = json_object_get_at(bot, i);
        json_t *a = json_object();
        json_set_hdr(a, "room_id", json_get_str(args, "room_id", ""));
        json_set_hdr(a, "reaction_event_id", ma3_strval(evt, ""));
        json_set_hdr(a, "reason", "approval resolved");
        json_t *r = ma3_call(ctx, "self._schedule_reaction_redaction", a);
        if (ma3_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* PoP: ma3_redact_bot_model_picker_reactions @ plugins/platforms/matrix/adapter.py:_redact_bot_model_picker_reactions */
static json_t *ma3_redact_bot_model_picker_reactions(ma3_ctx_t *ctx, const json_t *args) {
    const json_t *prompt = json_obj_get(args, "prompt");
    const json_t *bot = ma3_pget(prompt, "bot_reaction_events");
    if (!bot || bot->type != JSON_OBJECT) return json_null();
    for (size_t i = 0; i < json_object_size(bot); i++) {
        const json_t *evt = json_object_get_at(bot, i);
        json_t *a = json_object();
        json_set_hdr(a, "room_id", json_get_str(args, "room_id", ""));
        json_set_hdr(a, "event_id", ma3_strval(evt, ""));
        json_set_hdr(a, "reason", "model picker resolved");
        json_t *r = ma3_call(ctx, "self.redact_message", a);
        if (ma3_is_raise(r)) {
            if (ma3_raise_is_base(r)) return r;  /* except Exception: continue */
            json_free(r);
            continue;
        }
        json_free(r);
    }
    return json_null();
}
