/*
 * base_session_command.c - C11 port of upstream
 * gateway/platforms/base_session_command.py (gate w23_basesessio_9eb9cc).
 *
 * PoP: w23_basesessio_9eb9cc_guard_under @ gateway/platforms/base_session_command.py:guard_under_commands
 * PoP: w23_basesessio_9eb9cc_in_flight @ gateway/platforms/base_session_command.py:command_in_flight
 * PoP: w23_basesessio_9eb9cc_dispatch @ gateway/platforms/base_session_command.py:dispatch_active_session_command
 *
 * The adapter is a seam: the caller supplies the active-session table, the
 * session tasks, the authorization check, the event's interrupt verdict, the
 * newer owner that may take the session during the reply, and the held text.
 * This file owns only the ladder -- what is cancelled, what is handed back, in
 * which order -- and the guard ledger that makes the hand-back land on the
 * right guard.
 */
#include <stdlib.h>
#include <string.h>
#include "noc_w23_basesessio_9eb9cc.h"

/* Python truthiness of a JSON value; a missing node is falsy. */
static int bs_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL: return 0;
    case JSON_BOOL: return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    case JSON_ARRAY: return json_len(v) > 0;
    default: return 0;
    }
}

static const char *bs_str_key(const json_t *obj, const char *key) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (!v || v->type != JSON_STRING || !v->str_val) return NULL;
    return v->str_val;
}

/* Guard names are identities: does the array hold this exact name? */
static int bs_arr_has(const json_t *arr, const char *name) {
    size_t i, n;
    if (!arr || arr->type != JSON_ARRAY || !name) return 0;
    n = json_len(arr);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(arr, i);
        if (e && e->type == JSON_STRING && e->str_val && strcmp(e->str_val, name) == 0) return 1;
    }
    return 0;
}

/* _replaced_guard[child] -> parent, where a JSON null parent is None.
 * Last write wins, like a dict built in order. */
static int bs_replaced(const json_t *pairs, const char *child, const char **parent) {
    size_t n;
    if (!pairs || pairs->type != JSON_ARRAY || !child) return 0;
    n = json_len(pairs);
    while (n > 0) {
        const json_t *p = json_get(pairs, n - 1);
        const json_t *k, *v;
        n--;
        if (!p || p->type != JSON_ARRAY || json_len(p) < 2) continue;
        k = json_get(p, 0);
        v = json_get(p, 1);
        if (!k || k->type != JSON_STRING || !k->str_val || strcmp(k->str_val, child) != 0) continue;
        *parent = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
        return 1;
    }
    return 0;
}

/* The `while guard in _replaced_guard:` walk.  With *stop_in_flight* set the
 * walk also stops on a command that is still replying, so a refusal never
 * hands the session to a settled command's guard. */
static const char *bs_walk(const json_t *world, const char *guard, int stop_in_flight) {
    const json_t *pairs = json_obj_get(world, "replaced");
    const json_t *fl = json_obj_get(world, "in_flight");
    const char *cur = guard;
    int steps = 0;
    while (cur && steps < 64) {
        const char *parent = NULL;
        if (stop_in_flight && bs_arr_has(fl, cur)) break;
        if (!bs_replaced(pairs, cur, &parent)) break;
        cur = parent;
        steps++;
    }
    return cur;
}

/* Number of distinct guard names a weak set would still hold. */
static double bs_distinct(const json_t *arr) {
    size_t i, j, n, seen = 0;
    if (!arr || arr->type != JSON_ARRAY) return 0.0;
    n = json_len(arr);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(arr, i);
        int dup = 0;
        if (!e || e->type != JSON_STRING || !e->str_val) continue;
        for (j = 0; j < i; j++) {
            const json_t *p = json_get(arr, j);
            if (p && p->type == JSON_STRING && p->str_val && strcmp(p->str_val, e->str_val) == 0) {
                dup = 1;
                break;
            }
        }
        if (!dup) seen++;
    }
    return (double)seen;
}

json_t *w23_basesessio_9eb9cc_guard_under(const json_t *world) {
    const char *guard;
    const char *under;
    if (!world || world->type != JSON_OBJECT) return json_null();
    guard = bs_str_key(world, "guard");
    under = bs_walk(world, guard, 0);
    return under ? json_string(under) : json_null();
}

json_t *w23_basesessio_9eb9cc_in_flight(const json_t *world) {
    const char *guard;
    if (!world || world->type != JSON_OBJECT) return json_bool(0);
    guard = bs_str_key(world, "guard");
    return json_bool(bs_arr_has(json_obj_get(world, "in_flight"), guard) ? 1 : 0);
}

static void bs_note(json_t *calls, const char *what) {
    json_append(calls, json_string(what));
}

json_t *w23_basesessio_9eb9cc_dispatch(const json_t *world) {
    json_t *out = json_object();
    json_t *calls = json_array();
    const json_t *w = (world && world->type == JSON_OBJECT) ? world : NULL;
    const char *current = w ? bs_str_key(w, "current_guard") : NULL;
    const char *newer = w ? bs_str_key(w, "newer_owner") : NULL;
    const char *active = W23_BASESESSIO_9EB9CC_COMMAND_GUARD;
    const json_t *interrupt = w ? json_obj_get(w, "interrupt_accepted") : NULL;
    int has_held, auth_present, accepted, cancelled, restored = 0;
    int in_tasks;

    has_held = w ? bs_truthy(json_obj_get(w, "held")) : 0;
    auth_present = w ? bs_truthy(json_obj_get(w, "auth_check")) : 0;
    /* getattr(event, "_interrupt_accepted", False) is True -- the int 1 is not. */
    accepted = (interrupt && interrupt->type == JSON_BOOL && interrupt->bool_val) ? 1 : 0;
    in_tasks = w ? bs_truthy(json_obj_get(w, "in_session_tasks")) : 0;

    /* held.cancel_timer(): the command now owns the input the caller prepended. */
    if (has_held) bs_note(calls, "cancel_timer");

    /* await adapter._dispatch_inline_reply(event, log_cmd=cmd) */
    bs_note(calls, "dispatch_inline_reply");
    if (newer) active = newer; /* a newer owner may take the session during the reply */

    cancelled = 0;
    if (!auth_present || accepted) {
        bs_note(calls, "discard_text_debounce");
        bs_note(calls, "cancel_session_processing");
        cancelled = 1;
    }

    if (!cancelled) {
        /* finally: the command guard left the in-flight set, then the hand-back. */
        const char *cur = bs_walk(w, current, 1);
        const json_t *stale;
        if (has_held) restored = 1; /* adapter._text_debounce_store()[key] = held */
        if (strcmp(active, W23_BASESESSIO_9EB9CC_COMMAND_GUARD) != 0) {
            /* a newer owner took the session; it drains the held text itself. */
            bs_note(calls, "flush_text_debounce_now");
        } else if (cur && in_tasks) {
            bs_note(calls, "session_task_is_stale");
            stale = json_obj_get(w, "task_stale");
            if (!bs_truthy(stale)) {
                active = cur; /* the live owner gets its guard back */
                bs_note(calls, "flush_text_debounce_now");
            } else {
                bs_note(calls, "drain_pending");
            }
        } else {
            /* nobody live to hand it to: run the held text now. */
            bs_note(calls, "drain_pending");
        }
    }
    if (cancelled) bs_note(calls, "drain_pending");

    json_set(out, "calls", calls);
    json_set(out, "active", json_string(active));
    json_set(out, "held_restored", json_bool(restored ? 1 : 0));
    json_set(out, "in_flight_after", json_number(w ? bs_distinct(json_obj_get(w, "in_flight")) : 0.0));
    return out;
}
