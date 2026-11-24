/*
 * process_registry_awaiting.c - C11 port of upstream
 * tools/process_registry_awaiting.py (gate w26_processreg_e923be).
 *
 * PoP: w26_processreg_e923be_owner @ tools/process_registry_awaiting.py:_owner
 * PoP: w26_processreg_e923be_notifies @ tools/process_registry_awaiting.py:_notifies
 * PoP: w26_processreg_e923be_retained @ tools/process_registry_awaiting.py:retained_for_delivery
 * PoP: w26_processreg_e923be_adjust @ tools/process_registry_awaiting.py:_adjust
 * PoP: w26_processreg_e923be_awaits @ tools/process_registry_awaiting.py:awaits_notification
 *
 * The seams -- the completion queue's mutex, the in-flight lock, and the
 * liveness probe the registry's _any_running performs outside its own lock --
 * are fixed inputs here: the caller hands in the queued events, the entries it
 * already proved alive, the finished entries and the counter's contents.  What
 * is ported is the decision: which of the four notice states is live for an
 * owner, and how the hold counter moves when events are taken and released.
 *
 * A JSON number is read as a Python int for an owner id (JSON keeps no int /
 * float distinction); array and object ids fall back to their JSON text, which
 * str() would render as a Python repr -- no caller keys on that shape.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_processreg_e923be.h"

/* Python truthiness of a JSON value. */
static int pr_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python '==' over the shapes an owner id can hold.  A missing key and an
 * explicit null are the same element. */
static int pr_equal(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL) &&
        (b->type == JSON_NUMBER || b->type == JSON_BOOL)) {
        x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return 0;
}

/* str(evt.get("owner_task_id") or "") for a dict event, "" for anything else. */
static char *pr_owner_text(const json_t *evt) {
    const json_t *v;
    char buf[64];
    char *text = NULL;
    if (!evt || evt->type != JSON_OBJECT) return strdup("");
    v = json_obj_get(evt, "owner_task_id");
    if (!pr_truthy(v)) return strdup("");
    switch (v->type) {
    case JSON_STRING:
        text = strdup(v->str_val ? v->str_val : "");
        break;
    case JSON_BOOL:
        text = strdup(v->bool_val ? "True" : "False");
        break;
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        text = strdup(buf);
        break;
    default: {
        char *dumped = json_dumps(v, 0);
        text = strdup(dumped ? dumped : "");
        free(dumped);
        break;
    }
    }
    return text ? text : strdup("");
}

/* Does one session id equal any of the raw spawning owners? */
static int pr_owner_matches(const json_t *session, const json_t *owners) {
    size_t i;
    const json_t *oid;
    if (!owners || owners->type != JSON_ARRAY) return 0;
    if (!session || session->type != JSON_OBJECT) return 0;
    oid = json_obj_get(session, "owner_task_id");
    for (i = 0; i < json_len(owners); i++)
        if (pr_equal(json_get(owners, i), oid)) return 1;
    return 0;
}

json_t *w26_processreg_e923be_owner(const json_t *evt) {
    char *text = pr_owner_text(evt);
    json_t *out = json_string(text ? text : "");
    free(text);
    return out;
}

json_t *w26_processreg_e923be_notifies(const json_t *session) {
    int notify = session ? pr_truthy(json_obj_get(session, "notify_on_complete")) : 0;
    int watching = session ? (pr_truthy(json_obj_get(session, "watch_patterns")) &&
                              !pr_truthy(json_obj_get(session, "_watch_disabled"))) : 0;
    return json_bool((notify || watching) ? true : false);
}

json_t *w26_processreg_e923be_retained(const json_t *session) {
    int claimed = session ? pr_truthy(json_obj_get(session, "_finish_claimed")) : 0;
    int published = session ? pr_truthy(json_obj_get(session, "_completion_event")) : 0;
    return json_bool((claimed && !published) ? true : false);
}

json_t *w26_processreg_e923be_adjust(const json_t *in_flight,
                                     const json_t *owners,
                                     const json_t *delta) {
    double step = (delta && delta->type == JSON_NUMBER) ? delta->num_val : 0.0;
    size_t i, k, n;
    json_t *out = json_object();

    if (!owners || owners->type != JSON_ARRAY) return out;
    /* The counter as it stands: a hold is additive, and a key at <= 0 goes. */
    if (in_flight && in_flight->type == JSON_OBJECT) {
        for (k = 0; k < json_object_size(in_flight); k++) {
            const char *key = json_object_get_key_at(in_flight, k);
            const json_t *v = json_object_get_at(in_flight, k);
            if (key && v && v->type == JSON_NUMBER) json_set(out, key, json_number(v->num_val));
        }
    }
    n = json_len(owners);
    for (i = 0; i < n; i++) {
        const json_t *owner = json_get(owners, i);
        const char *key;
        double current = 0.0, next;
        const json_t *have;
        if (!owner || owner->type != JSON_STRING || !owner->str_val || !owner->str_val[0])
            continue;
        key = owner->str_val;
        have = json_obj_get(out, key);
        if (have && have->type == JSON_NUMBER) current = have->num_val;
        next = current + step;
        if (next <= 0.0) json_obj_del(out, key);
        else json_set(out, key, json_number(next));
    }
    return out;
}

json_t *w26_processreg_e923be_awaits(const json_t *state) {
    const json_t *owners, *running, *finished, *queued, *in_flight;
    size_t i;
    int any_owner = 0;

    if (!state || state->type != JSON_OBJECT) return json_bool(false);
    owners = json_obj_get(state, "owners");
    running = json_obj_get(state, "running");
    finished = json_obj_get(state, "finished");
    queued = json_obj_get(state, "queued");
    in_flight = json_obj_get(state, "in_flight");

    if (owners && owners->type == JSON_ARRAY) {
        for (i = 0; i < json_len(owners); i++)
            if (pr_truthy(json_get(owners, i))) { any_owner = 1; break; }
    }
    if (!any_owner) return json_bool(false);

    /* (1) A notifying process of one of these owners is still running. */
    if (running && running->type == JSON_ARRAY) {
        for (i = 0; i < json_len(running); i++) {
            const json_t *s = json_get(running, i);
            if (pr_owner_matches(s, owners) &&
                pr_truthy(w26_processreg_e923be_notifies(s)))
                return json_bool(true);
        }
    }
    /* (2) Exit recorded, completion not yet published. */
    for (i = 0; i < 2; i++) {
        const json_t *store = (i == 0) ? running : finished;
        size_t k;
        if (!store || store->type != JSON_ARRAY) continue;
        for (k = 0; k < json_len(store); k++) {
            const json_t *s = json_get(store, k);
            if (!pr_owner_matches(s, owners)) continue;
            if (pr_truthy(json_obj_get(s, "notify_on_complete")) &&
                pr_truthy(json_obj_get(s, "exited")) &&
                !pr_truthy(json_obj_get(s, "_completion_event")))
                return json_bool(true);
        }
    }
    /* (3) An in-flight hold is outstanding for one of these owners. */
    if (in_flight && in_flight->type == JSON_OBJECT) {
        for (i = 0; i < json_len(owners); i++) {
            const json_t *o = json_get(owners, i);
            if (o && o->type == JSON_STRING && o->str_val &&
                pr_truthy(json_obj_get(in_flight, o->str_val)))
                return json_bool(true);
        }
    }
    /* (4) The event itself is still sitting on the queue. */
    if (queued && queued->type == JSON_ARRAY) {
        for (i = 0; i < json_len(queued); i++) {
            char *owner = pr_owner_text(json_get(queued, i));
            size_t k;
            if (owner && owner[0]) {
                for (k = 0; k < json_len(owners); k++) {
                    const json_t *o = json_get(owners, k);
                    if (o && o->type == JSON_STRING && o->str_val &&
                        !strcmp(o->str_val, owner)) {
                        free(owner);
                        return json_bool(true);
                    }
                }
            }
            free(owner);
        }
    }
    return json_bool(false);
}
