/*
 * run_busy_flow.c — busy-session queue admission FLOW cores of
 * gateway/run_busy.py (GatewayBusySessionMixin + module slot helpers).
 *
 * Ports the DECISION sequences: whole-slot key matching, drain-mode gating,
 * FIFO overflow/slot admission ordering, wake-before-drain rescue, leftover
 * /steer coalescing branches, depth-cap requeue, carry-over event construction,
 * subagent steer fan-out (lock + per-child error paths), the agent-verb
 * try/except wrapper, busy slash routing, the interrupt+ack debounce gates and
 * the command-turn admission chain. All live I/O enters through one universal
 * seam; state lives in an injected json_t mirror mutated in place.
 *
 * Modeling contract (shared with tests/oracle_run_busy_flow.py):
 *  - Events/agents/adapters/sessions are referenced by ID STRINGS everywhere
 *    (pending slots, overflow arrays, call args). Object identity == id
 *    equality; the oracle keeps a per-id double registry so `is` agrees.
 *    The pending-agent sentinel is the id "__sentinel__".
 *  - State mirror shape (in["state"], copied by the harness before mutation):
 *      events:   {eid: {fields...}}           (text, internal, source,
 *                _admitted_before_drain, _gateway_accepted, user_id, ...)
 *      agents:   {aid: {_active_children: {kind, items:[cid...]}|null,
 *                _active_children_lock: bool, steer_callable: bool,
 *                interrupt_callable: bool, verb_callable: bool,
 *                _delegate_id: str}}
 *      sessions: {key: {conversation: {queued_events: [eid...],
 *                one_turn_restore: any}, turn: {busy_ack_ts: num}}}
 *      adapters: {aid: {_pending_messages: {key: eid}|null}}
 *  - Event attribute defaults (internal / _admitted_before_drain /
 *    _gateway_accepted = False) are read-side only; the mirror stores just
 *    fixture fields plus live writes, exactly like the oracle doubles.
 *  - `children` collections carry a kind tag (list/tuple/set/dict/str/none);
 *    set fixtures hold <= 1 item (live set order is hash-randomized).
 *  - MessageEvent construction is a scripted seam returning the new event's
 *    fields (incl. id); the stub registers it in the mirror, the oracle in
 *    its registry. turn.running_event() / event.get_command() are projected
 *    as the "running_event" seam / the event "command" field.
 *  - Bound-method seam results are projected to id strings (adapters) or
 *    handler-name strings (plain command table); None-valued arg keys are
 *    dropped on BOTH sides. Log seams record raw fmt + args (no printf
 *    formatting); exception args record str(exc).
 *  - Scripted seam results: NULL == None; {"__raise__": kind, "message": m}
 *    == a raised exception. BaseException kinds (SystemExit,
 *    KeyboardInterrupt, GeneratorExit, CancelledError) pass through every
 *    `except Exception` site via env->exc; Exception kinds are swallowed
 *    exactly where live swallows them.
 *  - Self-contained: the harness #includes this .c; every symbol is static.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "gateway_lifecycle_flow.h"

/* ── seam environment ─────────────────────────────────────────────────────── */
typedef struct rbf_env {
    json_t *state;   /* borrowed mirror (harness-owned copy of in["state"]) */
    json_t *(*call)(void *ud, const char *name, const json_t *args);
    void *ud;
    json_t *exc;     /* owned unhandled raise spec; NULL when none */
} rbf_env;

/* Universal seam call: args BORROWED by the stub (it copies into the trace),
 * result NEW (caller frees). NULL result == Python None. */
static json_t *rbf_call(rbf_env *e, const char *name, json_t *args) {
    json_t *r = e->call(e->ud, name, args);
    json_free(args);
    return r;
}

static bool rbf_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && gflow_is_str(json_obj_get(r, "__raise__"));
}
static const char *rbf_kind(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
static const char *rbf_msg(const json_t *r) {
    return json_get_str(r, "message", "");
}
/* BaseException-only kinds pass through `except Exception`. */
static bool rbf_is_base(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "GeneratorExit") == 0 || strcmp(kind, "CancelledError") == 0;
}

/* ── state mirror access (borrowed pointers) ─────────────────────────────── */
static json_t *rbf_events(rbf_env *e) { return json_obj_get(e->state, "events"); }
static json_t *rbf_event(rbf_env *e, const char *id) {
    return (id && id[0]) ? json_obj_get(rbf_events(e), id) : NULL;
}
static json_t *rbf_adapter(rbf_env *e, const char *id) {
    return (id && id[0]) ? json_obj_get(json_obj_get(e->state, "adapters"), id) : NULL;
}
static json_t *rbf_agent(rbf_env *e, const char *id) {
    return (id && id[0]) ? json_obj_get(json_obj_get(e->state, "agents"), id) : NULL;
}
/* _peek_session_state seam: records, returns the BORROWED session node. */
static json_t *rbf_peek(rbf_env *e, const char *key) {
    json_t *a = json_object();
    json_set(a, "session_key", json_string(key ? key : ""));
    json_t *r = e->call(e->ud, "_peek_session_state", a);
    json_free(a);
    return r;   /* borrowed mirror node (or NULL); never freed by the port */
}
/* _session_state seam: creates the session when absent, returns BORROWED. */
static json_t *rbf_sess(rbf_env *e, const char *key) {
    json_t *a = json_object();
    json_set(a, "session_key", json_string(key ? key : ""));
    json_t *r = e->call(e->ud, "_session_state", a);
    json_free(a);
    return r;
}
static json_t *rbf_queued(json_t *sess) {
    json_t *conv = sess ? json_obj_get(sess, "conversation") : NULL;
    json_t *q = conv ? json_obj_get(conv, "queued_events") : NULL;
    return (q && q->type == JSON_ARRAY) ? q : NULL;
}
static json_t *rbf_turn(json_t *sess) {
    return sess ? json_obj_get(sess, "turn") : NULL;
}

/* ── container ops (libjson reallocs exact-count; detach moves ownership) ── */
static void rbf_arr_insert0(json_t *arr, json_t *item) {   /* adopts item */
    if (!arr || arr->type != JSON_ARRAY || !item) return;
    size_t n = arr->c.count;
    json_t **ni = (json_t **)realloc(arr->c.items, (n + 1) * sizeof(json_t *));
    if (!ni) return;
    arr->c.items = ni;
    memmove(ni + 1, ni, n * sizeof(json_t *));
    ni[0] = item;
    arr->c.count = n + 1;
}
static json_t *rbf_arr_pop(json_t *arr, size_t idx) {   /* detaches; caller owns */
    if (!arr || arr->type != JSON_ARRAY || idx >= arr->c.count) return NULL;
    json_t *it = arr->c.items[idx];
    for (size_t j = idx; j + 1 < arr->c.count; j++) arr->c.items[j] = arr->c.items[j + 1];
    arr->c.count--;
    return it;
}
static json_t *rbf_obj_pop(json_t *obj, const char *key) {   /* detaches; caller owns */
    if (!obj || obj->type != JSON_OBJECT || !key) return NULL;
    for (size_t i = 0; i < obj->c.count; i++) {
        if (obj->c.keys[i] && strcmp(obj->c.keys[i], key) == 0) {
            json_t *v = obj->c.items[i];
            free(obj->c.keys[i]);
            for (size_t j = i; j + 1 < obj->c.count; j++) {
                obj->c.keys[j] = obj->c.keys[j + 1];
                obj->c.items[j] = obj->c.items[j + 1];
            }
            obj->c.count--;
            return v;
        }
    }
    return NULL;
}

/* ── unicode string helpers ──────────────────────────────────────────────── */
static unsigned long rbf_cp(const char *s, size_t *adv) {
    const unsigned char *u = (const unsigned char *)s;
    unsigned long cp; size_t n;
    if (u[0] < 0x80) { cp = u[0]; n = 1; }
    else if ((u[0] & 0xE0) == 0xC0) { cp = u[0] & 0x1F; n = 2; }
    else if ((u[0] & 0xF0) == 0xE0) { cp = u[0] & 0x0F; n = 3; }
    else if ((u[0] & 0xF8) == 0xF0) { cp = u[0] & 0x07; n = 4; }
    else { cp = u[0]; n = 1; }
    for (size_t i = 1; i < n && u[i]; i++) cp = (cp << 6) | (u[i] & 0x3F);
    *adv = n;
    return cp;
}
/* Python str.strip() White_Space set. */
static bool rbf_ws(unsigned long cp) {
    return (cp >= 0x09 && cp <= 0x0D) || cp == 0x20 || (cp >= 0x1C && cp <= 0x1F)
        || cp == 0x85 || cp == 0xA0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A)
        || cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}
static char *rbf_strip(const char *s) {
    if (!s) return strdup("");
    size_t start = 0, end = strlen(s), last = 0, i = 0;
    bool any = false;
    while (s[i]) {
        size_t adv; unsigned long cp = rbf_cp(s + i, &adv);
        if (!rbf_ws(cp)) { if (!any) { start = i; any = true; } last = i + adv; }
        i += adv;
    }
    if (!any) return strdup("");
    end = last;
    char *r = (char *)malloc(end - start + 1);
    if (!r) return strdup("");
    memcpy(r, s + start, end - start);
    r[end - start] = '\0';
    return r;
}
/* First n CODEPOINTS of s (Python s[:n]); malloc'd. */
static char *rbf_prefix(const char *s, size_t n) {
    if (!s) return strdup("");
    size_t i = 0, k = 0;
    while (s[i] && k < n) { size_t adv; rbf_cp(s + i, &adv); i += adv; k++; }
    char *r = (char *)malloc(i + 1);
    if (!r) return strdup("");
    memcpy(r, s, i);
    r[i] = '\0';
    return r;
}
static char *rbf_lower(const char *s) {
    char *r = strdup(s ? s : "");
    for (char *p = r; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return r;
}

/* ── log seam (raw fmt + args; stub emits {ev:"log",level,fmt,args}) ─────── */
static void rbf_log(rbf_env *e, int level, const char *fmt, json_t *args) {
    json_t *a = json_object();
    json_set(a, "level", json_number(level));
    json_set(a, "fmt", json_string(fmt));
    json_set(a, "args", args);   /* adopts */
    json_free(rbf_call(e, "log", a));
}
static json_t *rbf_logargs1(const char *s) {
    json_t *a = json_array();
    json_append(a, json_string(s ? s : ""));
    return a;
}

/* ── pure slot helpers ───────────────────────────────────────────────────── */
/* _strip_slot core: remainder after a WHOLE slot, "" for exact, NULL else.
 * text[len(slot)+1:] is codepoint slicing, but the matched prefix is exactly
 * slot's bytes plus the 1-byte ":" — so the byte offset is strlen(slot)+1. */
static char *rbf_strip_slot_str(const char *text, const char *slot) {
    if (!text || !slot) return NULL;
    if (strcmp(text, slot) == 0) return strdup("");
    size_t sl = strlen(slot);
    if (strncmp(text, slot, sl) == 0 && text[sl] == ':')
        return strdup(text + sl + 1);
    return NULL;
}

/* PoP: rbf_strip_slot @ gateway/run_busy.py:_strip_slot */
static json_t *rbf_strip_slot(rbf_env *e, const json_t *in) {
    (void)e;
    const char *text = json_get_str(in, "text", "");
    const char *slot = json_get_str(in, "slot", "");
    char *r = rbf_strip_slot_str(text, slot);
    json_t *ret = r ? json_string(r) : json_null();
    free(r);
    return ret;
}

/* PoP: rbf_same_chat_key_slots @ gateway/run_busy.py:_same_chat_key_slots */
static json_t *rbf_same_chat_key_slots(rbf_env *e, const json_t *in) {
    (void)e;
    const char *key = json_get_str(in, "key", "");
    const char *prefix = json_get_str(in, "prefix", "");
    const char *chat_id = json_get_str(in, "chat_id", "");
    const char *scope_id = json_get_str(in, "scope_id", NULL);
    size_t pl = strlen(prefix);
    if (strncmp(key, prefix, pl) != 0) return json_null();
    const char *rem = key + pl;
    const char *colon = strchr(rem, ':');
    char *chat_type = colon ? strndup(rem, (size_t)(colon - rem)) : strdup(rem);
    const char *rest = colon ? colon + 1 : "";
    /* candidates = (scope:chat, chat) if scope_id else (chat,) */
    char *cand0 = NULL;
    if (scope_id && scope_id[0]) {
        size_t n = strlen(scope_id) + 1 + strlen(chat_id) + 1;
        cand0 = (char *)malloc(n);
        if (cand0) snprintf(cand0, n, "%s:%s", scope_id, chat_id);
    }
    const char *cands[2] = { cand0 ? cand0 : chat_id, chat_id };
    int nc = cand0 ? 2 : 1;
    json_t *ret = json_null();
    for (int i = 0; i < nc; i++) {
        char *tail = rbf_strip_slot_str(rest, cands[i]);
        if (tail) {
            ret = json_array();
            json_append(ret, json_string(chat_type));
            json_append(ret, json_string(tail));
            free(tail);
            break;
        }
    }
    free(chat_type);
    free(cand0);
    return ret;
}

/* PoP: rbf_queue_during_drain_enabled @ gateway/run_busy.py:_queue_during_drain_enabled */
static json_t *rbf_queue_during_drain_enabled(rbf_env *e, const json_t *in) {
    (void)e;
    const json_t *selfo = json_obj_get(in, "self");
    const json_t *bim = json_obj_get(in, "busy_input_mode");
    const json_t *mode = gflow_truth(bim) ? bim : gflow_obj_get(selfo, "_busy_input_mode");
    const json_t *rr = gflow_obj_get(selfo, "_restart_requested");
    /* `restart and mode in {...}` returns restart itself when falsy. */
    if (!gflow_truth(rr)) return rr ? json_copy(rr) : json_null();
    const char *ms = gflow_str(mode);
    bool inset = ms && (strcmp(ms, "queue") == 0 || strcmp(ms, "steer") == 0);
    return json_bool(inset);
}

/* PoP: rbf_is_goal_continuation_event @ gateway/run_busy.py:_is_goal_continuation_event */
static json_t *rbf_is_goal_continuation_event(rbf_env *e, const json_t *in) {
    (void)e;
    const json_t *x = json_obj_get(in, "event_or_text");
    const json_t *text = x;
    if (x && x->type == JSON_OBJECT) {
        const json_t *t = json_obj_get(x, "text");
        text = t ? t : x;   /* getattr(event, "text", event) */
    }
    char *s;
    if (!gflow_truth(text)) s = strdup("");
    else if (text->type == JSON_STRING) s = strdup(text->str_val ? text->str_val : "");
    else s = gflow_pystr(text);
    static const char marker[] = "[Continuing toward your standing goal]\nGoal:";
    bool r = strncmp(s, marker, sizeof(marker) - 1) == 0;
    free(s);
    return json_bool(r);
}

/* PoP: rbf_overflow_queue @ gateway/run_busy.py:_overflow_queue */
static json_t *rbf_overflow_queue(rbf_env *e, const json_t *in) {
    const char *key = json_get_str(in, "session_key", "");
    json_t *sess = rbf_peek(e, key);
    if (!sess) return json_null();
    json_t *q = rbf_queued(sess);
    return q ? json_copy(q) : json_null();
}

/* PoP: rbf_queue_depth @ gateway/run_busy.py:_queue_depth */
static json_t *rbf_queue_depth(rbf_env *e, const json_t *in) {
    const char *key = json_get_str(in, "session_key", "");
    const char *aid = json_get_str(in, "adapter_id", NULL);
    json_t *sess = rbf_peek(e, key);
    json_t *q = sess ? rbf_queued(sess) : NULL;
    double depth = q ? (double)json_len(q) : 0.0;   /* len(overflow or ()) */
    if (aid) {
        json_t *ad = rbf_adapter(e, aid);
        json_t *slot = ad ? json_obj_get(ad, "_pending_messages") : NULL;
        /* getattr(adapter, "_pending_messages", {}): absent -> {} */
        if (slot && slot->type == JSON_OBJECT && json_obj_get(slot, key)) depth += 1;
    }
    return json_number(depth);
}
/* ── internal FIFO collaborators (real siblings; no PoP claim) ───────────── */
/* _enqueue_fifo: append to the overflow chain or take the empty slot. */
static void rbf_enqueue(rbf_env *e, const char *key, const char *eid, const char *adapter_id) {
    json_t *ad = adapter_id ? rbf_adapter(e, adapter_id) : NULL;
    json_t *slot = ad ? json_obj_get(ad, "_pending_messages") : NULL;
    if (!slot || slot->type == JSON_NULL) return;   /* pending_slot is None */
    if (slot->type != JSON_OBJECT) return;          /* dict-only modeling */
    if (json_obj_get(slot, key)) {
        json_t *sess = rbf_sess(e, key);
        json_t *q = rbf_queued(sess);
        if (q) json_append(q, json_string(eid));
    } else {
        json_set(slot, key, json_string(eid));
    }
    json_t *ev = rbf_event(e, eid);
    if (ev) json_set(ev, "_gateway_accepted", json_bool(true));
}

/* _promote_queued_event: overflow head becomes pending (None) or is staged. */
static json_t *rbf_promote(rbf_env *e, const char *key, const char *adapter_id,
                           const char *pending_eid) {
    json_t *sess = rbf_peek(e, key);
    json_t *q = sess ? rbf_queued(sess) : NULL;
    if (!gflow_truth(q))
        return pending_eid ? json_string(pending_eid) : json_null();
    if (!pending_eid) return rbf_arr_pop(q, 0);
    json_t *ad = adapter_id ? rbf_adapter(e, adapter_id) : NULL;
    if (ad && json_has(ad, "_pending_messages")) {
        json_t *slot = json_obj_get(ad, "_pending_messages");
        if (slot && slot->type == JSON_OBJECT)
            json_set(slot, key, rbf_arr_pop(q, 0));   /* adopts detached head */
    }
    return json_string(pending_eid);
}

/* _requeue_event_at_front: restore event at the head, un-staging the slot. */
static void rbf_requeue_front(rbf_env *e, const char *key, const char *eid,
                              const char *adapter_id) {
    json_t *ad = adapter_id ? rbf_adapter(e, adapter_id) : NULL;
    json_t *slot = ad ? json_obj_get(ad, "_pending_messages") : NULL;
    if (!slot || slot->type == JSON_NULL) return;
    if (slot->type != JSON_OBJECT) return;
    json_t *staged = json_obj_get(slot, key);   /* borrowed id node */
    if (staged && staged->type != JSON_NULL) {
        json_t *sess = rbf_sess(e, key);
        json_t *q = rbf_queued(sess);
        if (q) rbf_arr_insert0(q, json_copy(staged));
    }
    json_set(slot, key, json_string(eid));   /* frees the old staged node */
}

/* _carry_over_event: stand-alone event continuing the pinned channel context.
 * Returns the NEW event id node; NULL + env->exc on a scripted raise. */
static json_t *rbf_carry_over(rbf_env *e, const char *key, const json_t *source,
                              const char *text) {
    json_t *a = json_object();
    json_set(a, "session_key", json_string(key ? key : ""));
    if (source) json_set(a, "source", json_copy(source));
    json_set(a, "internal", json_bool(true));
    json_t *pin = rbf_call(e, "_pinned_channel_inputs", a);
    if (rbf_is_raise(pin)) { e->exc = pin; return NULL; }
    const json_t *prompt = json_get(pin, 0);
    const json_t *pinned_source = json_get(pin, 1);

    json_t *sess = rbf_sess(e, key);
    (void)sess;   /* turn.running_event() is projected as a seam */
    json_t *ra = json_object();
    json_set(ra, "session_key", json_string(key ? key : ""));
    json_t *rr = rbf_call(e, "running_event", ra);
    if (rbf_is_raise(rr)) { e->exc = rr; json_free(pin); return NULL; }
    const char *running_id = gflow_str(rr);
    json_t *running = running_id ? rbf_event(e, running_id) : NULL;
    json_t *uid = running ? json_obj_get(running, "user_id") : NULL;
    /* actor = running if getattr(running,"user_id",None) is not None
     *         else getattr(running,"source",None) */
    json_t *actor = (uid && uid->type != JSON_NULL) ? running
                  : (running ? json_obj_get(running, "source") : NULL);
    json_t *a_uid = actor ? json_obj_get(actor, "user_id") : NULL;
    json_t *a_uname = actor ? json_obj_get(actor, "user_name") : NULL;

    json_t *ma = json_object();
    json_set(ma, "text", json_string(text ? text : ""));
    if (pinned_source && pinned_source->type != JSON_NULL)
        json_set(ma, "source", json_copy(pinned_source));
    if (prompt && prompt->type != JSON_NULL)
        json_set(ma, "channel_prompt", json_copy(prompt));
    if (a_uid && a_uid->type != JSON_NULL) json_set(ma, "user_id", json_copy(a_uid));
    if (a_uname && a_uname->type != JSON_NULL) json_set(ma, "user_name", json_copy(a_uname));
    json_free(rr);
    json_free(pin);
    json_t *mr = rbf_call(e, "MessageEvent", ma);
    if (rbf_is_raise(mr)) { e->exc = mr; return NULL; }
    return mr;   /* new event id string */
}

/* ── claimed queue-flow entries ──────────────────────────────────────────── */
/* PoP: rbf_carry_over_event @ gateway/run_busy.py:_carry_over_event */
static json_t *rbf_carry_over_event(rbf_env *e, const json_t *in) {
    return rbf_carry_over(e, json_get_str(in, "session_key", ""),
                          json_obj_get(in, "source"), json_get_str(in, "text", ""));
}

/* PoP: rbf_enqueue_fifo @ gateway/run_busy.py:_enqueue_fifo */
static json_t *rbf_enqueue_fifo(rbf_env *e, const json_t *in) {
    rbf_enqueue(e, json_get_str(in, "session_key", ""),
                json_get_str(in, "event_id", ""), json_get_str(in, "adapter_id", NULL));
    return json_null();
}

/* PoP: rbf_requeue_event_at_front @ gateway/run_busy.py:_requeue_event_at_front */
static json_t *rbf_requeue_event_at_front(rbf_env *e, const json_t *in) {
    rbf_requeue_front(e, json_get_str(in, "session_key", ""),
                      json_get_str(in, "event_id", ""), json_get_str(in, "adapter_id", NULL));
    return json_null();
}

/* PoP: rbf_take_wake_admitted_before_drain @ gateway/run_busy.py:_take_wake_admitted_before_drain */
static json_t *rbf_take_wake_admitted_before_drain(rbf_env *e, const json_t *in) {
    const char *key = json_get_str(in, "session_key", "");
    const char *aid = json_get_str(in, "adapter_id", NULL);
    json_t *ad = aid ? rbf_adapter(e, aid) : NULL;
    json_t *slot = ad ? json_obj_get(ad, "_pending_messages") : NULL;
    if (gflow_truth(slot) && slot->type == JSON_OBJECT) {
        json_t *eidn = json_obj_get(slot, key);
        json_t *ev = eidn ? rbf_event(e, gflow_str(eidn)) : NULL;
        json_t *adm = ev ? json_obj_get(ev, "_admitted_before_drain") : NULL;
        /* getattr(..., False) is True — strict bool identity */
        if (adm && adm->type == JSON_BOOL && adm->bool_val) {
            json_t *popped = rbf_obj_pop(slot, key);   /* slot.pop(key) */
            const char *pid = gflow_str(popped);
            json_t *ret = rbf_promote(e, key, aid, pid);
            json_free(popped);
            return ret;
        }
    }
    json_t *sess = rbf_peek(e, key);   /* _overflow_queue(session_key) or [] */
    json_t *q = sess ? rbf_queued(sess) : NULL;
    if (q) {
        for (size_t i = 0; i < json_len(q); i++) {
            json_t *ev = rbf_event(e, gflow_str(json_get(q, i)));
            json_t *adm = ev ? json_obj_get(ev, "_admitted_before_drain") : NULL;
            if (adm && adm->type == JSON_BOOL && adm->bool_val)
                return rbf_arr_pop(q, i);
        }
    }
    return json_null();
}

/* PoP: rbf_requeue_at_depth_cap @ gateway/run_busy.py:_requeue_at_depth_cap */
static json_t *rbf_requeue_at_depth_cap(rbf_env *e, const json_t *in) {
    const char *key = json_get_str(in, "session_key", NULL);
    const json_t *source = json_obj_get(in, "source");
    const char *peid = json_get_str(in, "pending_event_id", NULL);
    const json_t *pending = json_obj_get(in, "pending");
    json_t *sa = json_object();
    if (source) json_set(sa, "source", json_copy(source));
    json_t *ar = rbf_call(e, "_delivery_adapter_for", sa);
    if (rbf_is_raise(ar)) { e->exc = ar; return json_null(); }
    const char *aid = gflow_str(ar);
    /* if not (adapter and session_key and (pending_event or pending)): return */
    bool guard = ar && ar->type != JSON_NULL && gflow_truth(json_obj_get(in, "session_key"))
              && ((peid && peid[0]) || gflow_truth(pending));
    if (!guard) { json_free(ar); return json_null(); }
    char *carry_id = NULL;
    if (!peid) {
        const char *ps = gflow_str(pending);
        json_t *cr = rbf_carry_over(e, key, source, (ps && ps[0]) ? ps : "");
        if (e->exc) { json_free(ar); return json_null(); }
        carry_id = cr ? strdup(gflow_str(cr)) : NULL;
        json_free(cr);
        peid = carry_id;
    }
    if (peid) rbf_requeue_front(e, key, peid, aid);
    free(carry_id);
    json_free(ar);
    return json_null();
}

/* PoP: rbf_defer_past_exhaustion_reset @ gateway/run_busy.py:_defer_past_exhaustion_reset */
static json_t *rbf_defer_past_exhaustion_reset(rbf_env *e, const json_t *in) {
    const char *key = json_get_str(in, "session_key", "");
    const char *aid = json_get_str(in, "adapter_id", NULL);
    const json_t *source = json_obj_get(in, "source");
    const char *peid = json_get_str(in, "pending_event_id", "");
    const json_t *result = json_obj_get(in, "result");
    const json_t *ps = result ? json_obj_get(result, "pending_steer") : NULL;
    char *leftover = rbf_strip(gflow_truth(ps) ? gflow_str(ps) : "");
    char *carry_id = NULL;
    if (leftover[0]) {
        json_t *sess = rbf_sess(e, key);
        json_t *q = rbf_queued(sess);
        if (q) rbf_arr_insert0(q, json_string(peid));
        json_t *cr = rbf_carry_over(e, key, source, leftover);
        if (e->exc) { free(leftover); return json_null(); }
        carry_id = cr ? strdup(gflow_str(cr)) : NULL;
        json_free(cr);
        if (carry_id) peid = carry_id;
    }
    json_t *ad = rbf_adapter(e, aid);
    json_t *slot = ad ? json_obj_get(ad, "_pending_messages") : NULL;
    if (slot && slot->type == JSON_OBJECT) json_set(slot, key, json_string(peid));
    rbf_log(e, 20, "Deferring queued follow-up for session %s until after its compression-exhausted reset",
            rbf_logargs1(key));
    free(leftover);
    free(carry_id);
    return json_null();
}

/* PoP: rbf_merge_leftover_steer @ gateway/run_busy.py:_merge_leftover_steer */
static json_t *rbf_merge_leftover_steer(rbf_env *e, const json_t *in) {
    const char *key = json_get_str(in, "session_key", NULL);
    const char *aid = json_get_str(in, "adapter_id", NULL);
    const json_t *source = json_obj_get(in, "source");
    const char *peid = json_get_str(in, "pending_event_id", NULL);
    const json_t *pending = json_obj_get(in, "pending");
    const json_t *result = json_obj_get(in, "result");
    char *leftover = NULL;
    if (gflow_truth(result)) {
        const json_t *ps = json_obj_get(result, "pending_steer");
        leftover = rbf_strip(gflow_truth(ps) ? gflow_str(ps) : "");
    }
    const char *pend_s = gflow_str(pending);
    if (leftover && leftover[0]) {
        if (!gflow_truth(pending) && !peid) {
            pend_s = leftover;
            char *p40 = rbf_prefix(pend_s, 40);
            rbf_log(e, 10, "Delivering leftover /steer as next turn: '%s...'", rbf_logargs1(p40));
            free(p40);
        } else if (peid && aid && gflow_truth(json_obj_get(in, "session_key"))) {
            json_t *sess = rbf_peek(e, key);   /* overflow = _overflow_queue */
            json_t *q = sess ? rbf_queued(sess) : NULL;
            json_t *ad = rbf_adapter(e, aid);
            json_t *slot = ad ? json_obj_get(ad, "_pending_messages") : NULL;
            if (ad && json_has(ad, "_pending_messages") && slot && slot->type == JSON_OBJECT) {
                json_t *promoted = json_obj_get(slot, key);
                if (promoted && promoted->type != JSON_NULL) {
                    if (q) rbf_arr_insert0(q, json_copy(promoted));
                    else {
                        json_t *s2 = rbf_sess(e, key);
                        json_t *q2 = rbf_queued(s2);
                        if (q2) rbf_arr_insert0(q2, json_copy(promoted));
                    }
                }
                json_set(slot, key, json_string(peid));
            } else {
                if (q) rbf_arr_insert0(q, json_string(peid));
                else {
                    json_t *s2 = rbf_sess(e, key);
                    json_t *q2 = rbf_queued(s2);
                    if (q2) rbf_arr_insert0(q2, json_string(peid));
                }
            }
            peid = NULL;
            pend_s = leftover;
            char *p40 = rbf_prefix(pend_s, 40);
            json_t *la = json_array();
            json_append(la, json_string(key ? key : ""));
            json_append(la, json_string(p40));
            rbf_log(e, 10, "Delivering leftover /steer before queued event for session %s: '%s...'", la);
            free(p40);
        } else if (gflow_truth(pending) && !peid) {
            if (gflow_truth(json_obj_get(in, "session_key"))) {
                json_t *cr = rbf_carry_over(e, key, source, leftover);
                if (e->exc) { free(leftover); return json_null(); }
                const char *ceid = gflow_str(cr);
                rbf_enqueue(e, key, ceid ? ceid : "", aid);
                json_free(cr);
            }
            char *p40 = rbf_prefix(leftover, 40);
            json_t *la = json_array();
            json_append(la, json_string((key && key[0]) ? key : "?"));
            json_append(la, json_string(p40));
            rbf_log(e, 10, "Enqueued leftover /steer behind pending message for session %s: '%s...'", la);
            free(p40);
        }
    }
    json_t *ret = json_array();
    json_append(ret, peid ? json_string(peid) : json_null());
    json_append(ret, pend_s ? json_string(pend_s) : json_null());
    free(leftover);
    return ret;
}
/* ── internal steer collaborators (real siblings; no PoP claim) ──────────── */
/* _steer_active_subagents: fan text out to every live child under the lock. */
static int rbf_steer_subagents(rbf_env *e, const char *agent_id, const char *text) {
    json_t *ag = agent_id ? rbf_agent(e, agent_id) : NULL;
    json_t *children = ag ? json_obj_get(ag, "_active_children") : NULL;
    if (!children || children->type != JSON_OBJECT) return 0;
    const char *kind = json_get_str(children, "kind", "");
    if (!(strcmp(kind, "list") == 0 || strcmp(kind, "tuple") == 0 || strcmp(kind, "set") == 0))
        return 0;   /* isinstance(children, (list, tuple, set)) */
    json_t *items = json_obj_get(children, "items");
    if (!gflow_truth(items)) return 0;   /* not children */

    json_t *lock = json_obj_get(ag, "_active_children_lock");
    json_t *snapshot = json_copy(items);
    if (gflow_truth(lock)) {
        json_t *la = json_object();
        json_set(la, "agent_id", json_string(agent_id));
        json_t *lr = rbf_call(e, "lock_enter", la);
        if (rbf_is_raise(lr)) {
            if (rbf_is_base(rbf_kind(lr))) { e->exc = lr; json_free(snapshot); return 0; }
            json_free(lr);   /* except Exception around the with-block */
            json_free(snapshot);
            return 0;
        }
        json_free(lr);
        json_t *xa = json_object();
        json_set(xa, "agent_id", json_string(agent_id));
        json_t *xr = rbf_call(e, "lock_exit", xa);
        if (rbf_is_raise(xr)) {
            if (rbf_is_base(rbf_kind(xr))) { e->exc = xr; json_free(snapshot); return 0; }
            json_free(xr);
            json_free(snapshot);
            return 0;
        }
        json_free(xr);
    }
    int accepted = 0;
    for (size_t i = 0; i < json_len(snapshot); i++) {
        const char *cid = gflow_str(json_get(snapshot, i));
        json_t *child = rbf_agent(e, cid);
        json_t *sc = child ? json_obj_get(child, "steer_callable") : NULL;
        if (!gflow_truth(sc)) continue;   /* not callable(steer) */
        json_t *ca = json_object();
        json_set(ca, "child_id", json_string(cid ? cid : ""));
        json_set(ca, "text", json_string(text ? text : ""));
        json_t *cr = rbf_call(e, "child_steer", ca);
        if (rbf_is_raise(cr)) {
            if (rbf_is_base(rbf_kind(cr))) {
                e->exc = cr;
                json_free(snapshot);
                return 0;
            }
            json_t *did = child ? json_obj_get(child, "_delegate_id") : NULL;
            json_t *wa = json_array();
            json_append(wa, json_string(gflow_str(did) ? gflow_str(did) : (cid ? cid : "")));
            json_append(wa, json_string(rbf_msg(cr)));
            rbf_log(e, 30, "Steer into subagent %r failed: %s", wa);
            json_free(cr);
            continue;
        }
        if (gflow_truth(cr)) accepted += 1;   /* accepted += bool(steer(text)) */
        json_free(cr);
    }
    json_free(snapshot);
    return accepted;
}

/* _steer_running_agent: parent steer plus subagent fan-out (no try here). */
static bool rbf_steer_running(rbf_env *e, const char *agent_id, const char *text) {
    json_t *sa = json_object();
    json_set(sa, "agent_id", json_string(agent_id ? agent_id : ""));
    json_set(sa, "text", json_string(text ? text : ""));
    json_t *sr = rbf_call(e, "agent_steer", sa);
    if (rbf_is_raise(sr)) { e->exc = sr; return false; }
    bool accepted = gflow_truth(sr);
    json_free(sr);
    int n = rbf_steer_subagents(e, agent_id, text);
    if (e->exc) return false;
    return (n != 0) || accepted;   /* bool(subagents) or accepted */
}

/* _interrupt_running_agent_for_busy_event: best-effort abort (except: pass). */
static void rbf_interrupt_busy(rbf_env *e, const char *event_id, const char *agent_id) {
    json_t *ta = json_object();
    json_set(ta, "event_id", json_string(event_id ? event_id : ""));
    json_t *tr = rbf_call(e, "busy_interrupt_text", ta);
    if (rbf_is_raise(tr)) {
        if (rbf_is_base(rbf_kind(tr))) e->exc = tr;
        else json_free(tr);
        return;
    }
    const char *itext = gflow_str(tr);
    json_t *ia = json_object();
    json_set(ia, "agent_id", json_string(agent_id ? agent_id : ""));
    json_set(ia, "text", json_string(itext ? itext : ""));
    json_t *ir = rbf_call(e, "agent_interrupt", ia);
    if (rbf_is_raise(ir)) {
        if (rbf_is_base(rbf_kind(ir))) e->exc = ir;
        else json_free(ir);
    } else {
        json_free(ir);
    }
    json_free(tr);
}

/* ── claimed steer / dispatch entries ────────────────────────────────────── */
/* PoP: rbf_steer_active_subagents @ gateway/run_busy.py:_steer_active_subagents */
static json_t *rbf_steer_active_subagents(rbf_env *e, const json_t *in) {
    int n = rbf_steer_subagents(e, json_get_str(in, "running_agent", NULL),
                                json_get_str(in, "text", ""));
    return json_number(n);
}

/* PoP: rbf_try_agent_verb @ gateway/run_busy.py:_try_agent_verb */
static json_t *rbf_try_agent_verb(rbf_env *e, const json_t *in) {
    const char *aid = json_get_str(in, "running_agent", NULL);
    const char *verb = json_get_str(in, "verb", "");
    const char *text = json_get_str(in, "text", "");
    const char *key = json_get_str(in, "session_key", "");
    const char *eid = json_get_str(in, "event_id", NULL);
    json_t *ct = NULL;
    const char *call_text = text;
    if (eid) {   /* if event: call_text = _steer_text_with_origin(text, event) */
        json_t *oa = json_object();
        json_set(oa, "text", json_string(text));
        json_set(oa, "event_id", json_string(eid));
        ct = rbf_call(e, "steer_text_with_origin", oa);
        if (rbf_is_raise(ct)) {
            if (rbf_is_base(rbf_kind(ct))) { e->exc = ct; return json_bool(false); }
            goto failed;
        }
        call_text = gflow_str(ct);
    }
    bool ret;
    if (strcmp(verb, "steer") == 0) {
        ret = rbf_steer_running(e, aid, call_text ? call_text : "");
        if (e->exc) {
            if (rbf_is_base(rbf_kind(e->exc))) { json_free(ct); return json_bool(false); }
            goto failed_exc;
        }
    } else {
        json_t *va = json_object();
        json_set(va, "agent_id", json_string(aid ? aid : ""));
        json_set(va, "verb", json_string(verb));
        json_set(va, "text", json_string(call_text ? call_text : ""));
        json_t *vr = rbf_call(e, "agent_verb", va);
        if (rbf_is_raise(vr)) {
            if (rbf_is_base(rbf_kind(vr))) { e->exc = vr; json_free(ct); return json_bool(false); }
            ct = vr;   /* reuse ct slot for the warning message */
            goto failed;
        }
        ret = gflow_truth(vr);
        json_free(vr);
    }
    json_free(ct);
    return json_bool(ret);

failed_exc:
    {
        json_t *spec = e->exc;
        e->exc = NULL;
        json_t *wa = json_array();
        json_append(wa, json_string(verb));
        json_append(wa, json_string(key));
        json_append(wa, json_string(rbf_msg(spec)));
        rbf_log(e, 30, "Gateway %s failed for session %s: %s", wa);
        json_free(spec);
        json_free(ct);
        return json_bool(false);
    }
failed:
    {
        json_t *wa = json_array();
        json_append(wa, json_string(verb));
        json_append(wa, json_string(key));
        json_append(wa, json_string(rbf_msg(ct)));
        rbf_log(e, 30, "Gateway %s failed for session %s: %s", wa);
        json_free(ct);
        return json_bool(false);
    }
}

/* Class constants projected from gateway/run.py (GatewayRunner) and the
 * run_busy mixin body; the oracle seeds the live tables. */
static const char *rbf_special_handler(const char *k) {
    static const char *keys[] = {"start","stop","new","queue","steer","egress","goal","loop","moa"};
    static const char *vals[] = {"_busy_start_command","_busy_stop_command","_busy_new_command",
        "_busy_queue_command","_busy_steer_command","_busy_egress_command","_busy_goal_command",
        "_busy_loop_command","_busy_moa_command"};
    for (int i = 0; i < 9; i++) if (strcmp(k, keys[i]) == 0) return vals[i];
    return NULL;
}
static const char *rbf_reject_text(const char *k) {
    if (strcmp(k, "model") == 0) return "gateway.busy.reject_model";
    if (strcmp(k, "codex-runtime") == 0) return "gateway.busy.reject_codex_runtime";
    if (strcmp(k, "moa") == 0) return "gateway.busy.reject_moa";
    return NULL;
}

/* PoP: rbf_busy_slash_route @ gateway/run_busy.py:_busy_slash_route */
static json_t *rbf_busy_slash_route(rbf_env *e, const json_t *in) {
    const json_t *cmd = json_obj_get(in, "cmd_def");
    const char *policy = json_get_str(cmd, "busy_policy", "reject");
    const char *hk = json_get_str(cmd, "busy_handler", NULL);
    const char *name = json_get_str(cmd, "name", "");
    json_t *ret = json_array();
    if (hk && hk[0]) {   /* if handler_key: */
        const char *special = rbf_special_handler(hk);
        if (special) {
            json_append(ret, json_string("special"));
            json_append(ret, json_string(special));
            return ret;
        }
        const char *reject = rbf_reject_text(hk);
        if (reject) {
            json_append(ret, json_string("reject"));
            json_append(ret, json_string(reject));
            return ret;
        }
    }
    if (strcmp(policy, "dispatch") == 0 || strcmp(policy, "interrupt_then_dispatch") == 0) {
        json_t *pr = rbf_call(e, "gateway_plain_command_handlers", json_object());
        if (rbf_is_raise(pr)) { e->exc = pr; json_free(ret); return NULL; }
        json_t *plain = pr ? json_obj_get(pr, name) : NULL;
        if (plain && plain->type != JSON_NULL) {
            json_append(ret, json_string("plain"));
            json_append(ret, json_string(gflow_str(plain) ? gflow_str(plain) : ""));
            json_free(pr);
            return ret;
        }
        json_t *wa = json_array();
        json_append(wa, json_string(policy));
        json_append(wa, json_string(name));
        rbf_log(e, 30, "busy_policy=%s for /%s has no mid-run handler \u2014 falling back to busy-reject", wa);
        json_free(pr);
    }
    json_append(ret, json_string("reject"));
    json_append(ret, json_null());
    return ret;
}

/* PoP: rbf_busy_interrupt_and_ack @ gateway/run_busy.py:_busy_interrupt_and_ack */
static json_t *rbf_busy_interrupt_and_ack(rbf_env *e, const json_t *in) {
    const char *eid = json_get_str(in, "event_id", "");
    const char *key = json_get_str(in, "session_key", "");
    const char *aid = json_get_str(in, "adapter_id", NULL);
    const char *bsk = json_get_str(in, "busy_state_key", NULL);
    const char *agent_id = json_get_str(in, "running_agent", NULL);
    const json_t *steer = json_obj_get(in, "steer");
    const char *em = json_get_str(steer, "effective_mode", "");
    bool redirected = json_get_bool(steer, "redirected", false);
    bool is_queue = strcmp(em, "queue") == 0;
    bool is_steer = strcmp(em, "steer") == 0;
    bool is_redirect = strcmp(em, "interrupt") == 0 && redirected;

    if (strcmp(em, "interrupt") == 0 && !redirected && agent_id
        && strcmp(agent_id, "__sentinel__") != 0) {
        rbf_interrupt_busy(e, eid, agent_id);
        if (e->exc) return json_null();
    }

    json_t *ea = json_object();
    json_set(ea, "key", json_string("HERMES_GATEWAY_BUSY_ACK_ENABLED"));
    json_set(ea, "default", json_string("true"));
    json_t *er = rbf_call(e, "env", ea);
    if (rbf_is_raise(er)) { e->exc = er; return json_null(); }
    char *envval = rbf_lower(gflow_str(er) ? gflow_str(er) : "");
    json_free(er);
    if (strcmp(envval, "true") != 0) {
        rbf_log(e, 10, "Busy ack suppressed for session %s", rbf_logargs1(key));
        free(envval);
        return json_null();
    }
    free(envval);

    json_t *tr = rbf_call(e, "time", json_object());
    if (rbf_is_raise(tr)) { e->exc = tr; return json_null(); }
    double now = (tr && tr->type == JSON_NUMBER) ? tr->num_val : 0.0;
    json_free(tr);
    double ts = 0.0;
    if (bsk) {
        json_t *bs = json_obj_get(json_obj_get(e->state, "sessions"), bsk);
        json_t *turn = rbf_turn(bs);
        json_t *tsn = turn ? json_obj_get(turn, "busy_ack_ts") : NULL;
        if (tsn && tsn->type == JSON_NUMBER) ts = tsn->num_val;
    }
    if (now - ts < 30) return json_null();   /* debounce */

    if (is_steer) {
        json_t *ga = json_object();
        json_set(ga, "event_id", json_string(eid));
        json_set(ga, "session_key", json_string(key));
        json_t *gr = rbf_call(e, "busy_steer_ack_enabled", ga);
        if (rbf_is_raise(gr)) { e->exc = gr; return json_null(); }
        bool enabled = gflow_truth(gr);
        json_free(gr);
        if (!enabled) return json_null();
    }

    json_t *sess = rbf_sess(e, key);
    json_t *turn = rbf_turn(sess);
    if (turn) json_set(turn, "busy_ack_ts", json_number(now));

    json_t *ca = json_object();
    json_set(ca, "event_id", json_string(eid));
    json_set(ca, "now", json_number(now));
    if (bsk) json_set(ca, "busy_state_key", json_string(bsk));
    if (agent_id) json_set(ca, "running_agent_id", json_string(agent_id));
    json_set(ca, "is_steer_mode", json_bool(is_steer));
    json_set(ca, "is_queue_mode", json_bool(is_queue));
    json_set(ca, "is_redirect_mode", json_bool(is_redirect));
    json_set(ca, "demoted_for_subagents", json_bool(json_get_bool(steer, "demoted_for_subagents", false)));
    json_set(ca, "demoted_for_compression", json_bool(json_get_bool(steer, "demoted_for_compression", false)));
    json_t *cr = rbf_call(e, "compose_busy_ack_message", ca);
    if (rbf_is_raise(cr)) { e->exc = cr; return json_null(); }
    const char *message = gflow_str(cr);
    json_t *sargs = json_object();
    json_set(sargs, "event_id", json_string(eid));
    if (aid) json_set(sargs, "adapter_id", json_string(aid));
    json_set(sargs, "message", json_string(message ? message : ""));
    json_t *sr = rbf_call(e, "send_busy_ack_reply", sargs);
    if (rbf_is_raise(sr)) { e->exc = sr; json_free(cr); return json_null(); }
    json_free(sr);
    json_free(cr);
    return json_null();
}

/* PoP: rbf_command_turn_admitted @ gateway/run_busy.py:_command_turn_admitted */
static json_t *rbf_command_turn_admitted(rbf_env *e, const json_t *in) {
    const char *eid = json_get_str(in, "event_id", "");
    const char *key = json_get_str(in, "session_key", "");
    json_t *ar = rbf_call(e, "active_authority", json_object());
    if (rbf_is_raise(ar)) { e->exc = ar; return json_bool(false); }
    if (gflow_is_null(ar)) { json_free(ar); return json_bool(false); }
    json_free(ar);
    json_t *er = rbf_call(e, "executing_admission_get", json_object());
    if (rbf_is_raise(er)) { e->exc = er; return json_bool(false); }
    bool exec_adm = gflow_truth(er);
    json_free(er);
    if (exec_adm) return json_bool(false);
    json_t *ev = rbf_event(e, eid);
    if (gflow_truth(ev ? json_obj_get(ev, "internal") : NULL)) return json_bool(false);
    if (gflow_truth(ev ? json_obj_get(ev, "command") : NULL)) return json_bool(false);
    json_t *sess = rbf_peek(e, key);
    if (!sess) return json_bool(true);
    json_t *conv = json_obj_get(sess, "conversation");
    json_t *otr = conv ? json_obj_get(conv, "one_turn_restore") : NULL;
    return json_bool(!gflow_truth(otr));
}