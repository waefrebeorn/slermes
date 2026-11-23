/*
 * turn_thread_reset.c — C port of upstream Hermes gateway/turn_thread_reset.py.
 *
 * PoP: w12_turnthread_3f2b0b_reset @ gateway/turn_thread_reset.py:reset_reused_thread_state
 * PoP: w12_turnthread_3f2b0b_reset @ gateway/turn_thread_reset.py:_PER_TURN_THREAD_LOCALS
 *
 * The live function mutates sys.modules / threading.local namespaces in the
 * calling thread. This port receives a JSON projection of exactly those three
 * state holders and applies the identical transformations; the ORACLE installs
 * fake modules / locals with the same projections and drives the LIVE function
 * to produce the expected projection.
 */
#include "noc_w12_turnthread_3f2b0b.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const PER_TURN[] = {
    "tools.terminal_tool._callback_tls",
    "tools.environments.base._activity_callback_local",
    "agent.vault_backends.unlock._callback_tls",
    "agent.vault_backends.unlock._current_session_tls",
    "agent.auxiliary_client._aux_probe_state",
    "agent.auxiliary_client._aux_interrupt_protection",
    "agent.auxiliary_client._aux_progress",
    "agent.auxiliary_client._aux_dispatch",
    "agent.auxiliary_client._aux_provider_response",
    "agent.auxiliary_client._aux_stream_deadline",
    "agent.local_prompt_progress._reading",
    "hermes_cli.plugins._thread_tool_whitelist",
    "hermes_cli.observability.shared_metrics_events._compression_attempt",
    "hermes_logging._session_context",
};
static const size_t PER_TURN_N = sizeof(PER_TURN) / sizeof(PER_TURN[0]);

static int is_per_turn(const char *k) {
    for (size_t i = 0; i < PER_TURN_N; i++)
        if (!strcmp(k, PER_TURN[i])) return 1;
    return 0;
}

json_t *w12_turnthread_3f2b0b_per_turn_locals(void) {
    json_t *a = json_array();
    for (size_t i = 0; i < PER_TURN_N; i++) json_append(a, json_string(PER_TURN[i]));
    return a;
}

static long long tid_from(const json_t *world) {
    const char *s = json_get_str(world, "tid", "0");
    if (!s) return 0;
    return strtoll(s, NULL, 10);
}

json_t *w12_turnthread_3f2b0b_reset(const json_t *world) {
    json_t *out = json_object();
    long long tid = tid_from(world);

    /* tools.interrupt.set_interrupt(False, tid): clears bit, reason, pending yield. */
    json_t *intr = json_obj_get(world, "interrupt");
    if (json_is_object(intr)) {
        json_t *st = json_object();
        json_set(st, "set", json_bool(false));
        json_set(st, "reason", json_null());
        json_set(st, "pending_yield", json_bool(false));
        json_set(st, "last_tid", json_number((double)tid));
        json_set(out, "interrupt", st);
    } else {
        json_set(out, "interrupt", json_null());
    }

    /* tools.terminal_tool_sudo._sudo_password_cache.pop("thread:<tid>", None) */
    json_t *sc = json_obj_get(world, "sudo_cache");
    if (json_is_object(sc)) {
        char key[80];
        snprintf(key, sizeof(key), "thread:%lld", tid);
        json_t *newsc = json_copy(sc);
        json_obj_del(newsc, key);
        json_set(out, "sudo_cache", newsc);
    } else {
        json_set(out, "sudo_cache", json_null());
    }

    /* Per-turn threading.local.__dict__.clear() for each configured local present. */
    json_t *locals_in = json_obj_get(world, "locals");
    json_t *locals_out = json_object();
    if (json_is_object(locals_in)) {
        for (size_t i = 0; i < json_object_size(locals_in); i++) {
            const char *k = json_object_get_key_at(locals_in, i);
            json_t *v = json_object_get_at(locals_in, i);
            if (!k) continue;
            if (is_per_turn(k) && v && v->type == JSON_OBJECT)
                json_set(locals_out, k, json_object());
            else
                json_set(locals_out, k, json_copy(v));
        }
    }
    json_set(out, "locals", locals_out);
    return out;
}
