/* Oracle harness: agent/relay_runtime.py vs the C port.
 *
 * Drives the C port through the behaviours that are observable without a live
 * `nemo_relay` wheel — which is precisely the state the Python module is
 * designed to degrade through, so the comparison is meaningful, not vacuous:
 *   - current_profile_key / _session_id normalization
 *   - NoopRelayRuntime surface (available, intercepts, managed execution)
 *   - RelayRuntime managed-execution consumer refcounting
 *   - session bookkeeping (ensure/get/close, subagent parent tracking)
 *   - host registry per-profile identity and shutdown
 *   - coordinator lease/turn lifecycle and has_active_turn
 *   - _is_relay_wrapped_callback_error matching
 *
 * Emits one JSON object per case on stdout; sta_oracle_agent_relay_runtime.py
 * replays each against live Python and diffs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port_agent_relay_runtime.h"

static const char *jb(bool v) { return v ? "true" : "false"; }

static const char *jstr(const char *s)
{
    static char b[4][2048]; static int bi = 0;
    int idx = bi; char *q = b[idx]; bi = (bi + 1) & 3;
    if (!s) { snprintf(b[idx], sizeof b[idx], "null"); return b[idx]; }
    *q++ = '"';
    for (const char *p = s; *p && (size_t)(q - b[idx]) < sizeof(b[0]) - 8; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = (char)c; }
        else if (c < 0x20) {
            q += sprintf(q, "\\u%04x", c);
        } else *q++ = (char)c;
    }
    *q++ = '"'; *q = '\0';
    return b[idx];
}

int main(void)
{
    /* ── 1. _session_id normalization ─────────────────────────────────── */
    const char *sids[] = { "abc", "", NULL };
    for (size_t i = 0; i < sizeof(sids)/sizeof(sids[0]); i++) {
        printf("{\"case\":\"session_id\",\"in\":%s,\"out\":%s}\n",
               jstr(sids[i]), jstr(relay_session_id_of_event(sids[i])));
    }

    /* ── 2. current_profile_key ───────────────────────────────────────── */
    const char *pkey = relay_current_profile_key();
    printf("{\"case\":\"profile_key\",\"out\":%s}\n", jstr(pkey));
    /* Cached: a second call must return the identical string. */
    printf("{\"case\":\"profile_key_stable\",\"out\":%s}\n",
           jb(strcmp(pkey, relay_current_profile_key()) == 0));

    /* ── 3. NoopRelayRuntime surface ──────────────────────────────────── */
    printf("{\"case\":\"noop_available\",\"out\":%s}\n", jb(relay_noop_available()));
    printf("{\"case\":\"noop_managed\",\"out\":%s}\n", jb(relay_noop_managed_execution_enabled()));
    relay_noop_retain_managed_execution("consumer-a");
    printf("{\"case\":\"noop_managed_after_retain\",\"out\":%s}\n",
           jb(relay_noop_managed_execution_enabled()));
    relay_noop_release_managed_execution("consumer-a");
    {
        const char *args = "{\"a\":1,\"b\":\"x\"}";
        char *out = relay_noop_apply_tool_request_intercepts("s1", "terminal", args);
        printf("{\"case\":\"noop_intercepts\",\"out\":%s}\n", jstr(out));
        free(out);
    }

    /* ── 4. RelayRuntime managed-execution consumers ──────────────────── */
    relay_runtime_t *rt = relay_runtime_new("/tmp/profile-a");
    printf("{\"case\":\"runtime_profile\",\"out\":%s}\n", jstr(relay_runtime_profile_key(rt)));
    printf("{\"case\":\"runtime_id_len\",\"out\":%d}\n", (int)strlen(relay_runtime_id(rt)));
    printf("{\"case\":\"managed_initial\",\"out\":%s}\n",
           jb(relay_runtime_managed_execution_enabled(rt)));
    relay_runtime_retain_managed_execution(rt, "c1");
    printf("{\"case\":\"managed_after_c1\",\"out\":%s}\n",
           jb(relay_runtime_managed_execution_enabled(rt)));
    relay_runtime_retain_managed_execution(rt, "c2");
    relay_runtime_retain_managed_execution(rt, "c1");   /* set: idempotent */
    relay_runtime_release_managed_execution(rt, "c1");
    printf("{\"case\":\"managed_after_release_c1\",\"out\":%s}\n",
           jb(relay_runtime_managed_execution_enabled(rt)));
    relay_runtime_release_managed_execution(rt, "c2");
    printf("{\"case\":\"managed_after_release_all\",\"out\":%s}\n",
           jb(relay_runtime_managed_execution_enabled(rt)));
    /* discard of an absent consumer must not raise */
    relay_runtime_release_managed_execution(rt, "never-added");
    printf("{\"case\":\"managed_after_bogus_release\",\"out\":%s}\n",
           jb(relay_runtime_managed_execution_enabled(rt)));

    /* ── 5. sessions without a backend ────────────────────────────────── */
    relay_session_t *s = relay_runtime_ensure_session(rt, "sess-1", NULL, NULL);
    printf("{\"case\":\"ensure_session_id\",\"out\":%s}\n",
           jstr(s ? relay_session_id(s) : NULL));
    printf("{\"case\":\"ensure_session_handle_null\",\"out\":%s}\n",
           jb(s == NULL || relay_session_handle(s) == NULL));
    printf("{\"case\":\"ensure_session_empty_id\",\"out\":%s}\n",
           jb(relay_runtime_ensure_session(rt, "", NULL, NULL) == NULL));
    /* idempotent: the same id returns the same session object */
    printf("{\"case\":\"ensure_session_idempotent\",\"out\":%s}\n",
           jb(relay_runtime_ensure_session(rt, "sess-1", NULL, NULL) == s));
    printf("{\"case\":\"get_session_found\",\"out\":%s}\n",
           jb(relay_runtime_get_session(rt, "sess-1") == s));
    printf("{\"case\":\"get_session_missing\",\"out\":%s}\n",
           jb(relay_runtime_get_session(rt, "nope") == NULL));
    printf("{\"case\":\"get_session_handle_missing\",\"out\":%s}\n",
           jb(relay_runtime_get_session_handle(rt, "nope") == NULL));

    /* subagent parent tracking */
    relay_session_t *child = relay_runtime_register_subagent(rt, "sess-1", "sess-child", NULL);
    printf("{\"case\":\"subagent_child_id\",\"out\":%s}\n",
           jstr(child ? relay_session_id(child) : NULL));
    printf("{\"case\":\"subagent_parent_id\",\"out\":%s}\n",
           jstr(child ? relay_session_parent_id(child) : NULL));
    printf("{\"case\":\"subagent_self_parent\",\"out\":%s}\n",
           jb(relay_runtime_register_subagent(rt, "x", "x", NULL) == NULL));
    printf("{\"case\":\"subagent_empty\",\"out\":%s}\n",
           jb(relay_runtime_register_subagent(rt, "", "y", NULL) == NULL));
    relay_runtime_unregister_subagent(rt, "sess-child");
    printf("{\"case\":\"subagent_after_unregister\",\"out\":%s}\n",
           jb(relay_runtime_get_session(rt, "sess-child") == NULL));

    /* close_session removes the entry */
    relay_runtime_close_session(rt, "sess-1");
    printf("{\"case\":\"after_close\",\"out\":%s}\n",
           jb(relay_runtime_get_session(rt, "sess-1") == NULL));
    /* closing an unknown session is a no-op, not an error */
    relay_runtime_close_session(rt, "ghost");
    printf("{\"case\":\"close_unknown_ok\",\"out\":true}\n");

    /* ── 6. tool intercepts fall through to the original args ─────────── */
    {
        const char *args = "{\"path\":\"/tmp/x\"}";
        char *out = relay_runtime_apply_tool_request_intercepts(rt, "sess-2", "read_file", args);
        printf("{\"case\":\"intercepts_no_backend\",\"out\":%s}\n", jstr(out));
        free(out);
        /* managed execution on, still no backend -> unchanged */
        relay_runtime_retain_managed_execution(rt, "c1");
        out = relay_runtime_apply_tool_request_intercepts(rt, "sess-2", "read_file", args);
        printf("{\"case\":\"intercepts_managed_no_backend\",\"out\":%s}\n", jstr(out));
        free(out);
        relay_runtime_release_managed_execution(rt, "c1");
    }
    /* emit_mark cannot succeed without a backend */
    printf("{\"case\":\"emit_mark_no_backend\",\"out\":%s}\n",
           jb(relay_runtime_emit_mark(rt, "m", "sess-3", NULL, NULL)));

    relay_runtime_shutdown(rt);
    relay_runtime_free(rt);

    /* ── 7. host registry ─────────────────────────────────────────────── */
    relay_host_registry_t *reg = relay_host_registry_new();
    relay_host_t *h1 = relay_host_registry_for_profile(reg, "/tmp/p1", true);
    relay_host_t *h2 = relay_host_registry_for_profile(reg, "/tmp/p1", true);
    relay_host_t *h3 = relay_host_registry_for_profile(reg, "/tmp/p2", true);
    printf("{\"case\":\"registry_same_profile_identity\",\"out\":%s}\n", jb(h1 == h2));
    printf("{\"case\":\"registry_distinct_profiles\",\"out\":%s}\n", jb(h1 != h3));
    printf("{\"case\":\"registry_host_available\",\"out\":%s}\n", jb(relay_host_available(h1)));
    printf("{\"case\":\"registry_host_profile\",\"out\":%s}\n", jstr(relay_host_profile_key(h1)));
    printf("{\"case\":\"registry_host_runtime_null\",\"out\":%s}\n",
           jb(relay_host_runtime(h1) == NULL));
    printf("{\"case\":\"registry_no_create\",\"out\":%s}\n",
           jb(relay_host_registry_for_profile(reg, "/tmp/p3", false) == NULL));
    relay_host_registry_shutdown_profile(reg, "/tmp/p1");
    printf("{\"case\":\"registry_after_shutdown_profile\",\"out\":%s}\n",
           jb(relay_host_registry_for_profile(reg, "/tmp/p1", false) == NULL));
    printf("{\"case\":\"registry_other_profile_survives\",\"out\":%s}\n",
           jb(relay_host_registry_for_profile(reg, "/tmp/p2", false) != NULL));
    relay_host_registry_shutdown_all(reg);
    printf("{\"case\":\"registry_after_shutdown_all\",\"out\":%s}\n",
           jb(relay_host_registry_for_profile(reg, "/tmp/p2", false) == NULL));

    /* ── 8. coordinator lease / turn lifecycle ────────────────────────── */
    relay_coordinator_t *co = relay_coordinator_new(reg);
    printf("{\"case\":\"no_active_turn_initially\",\"out\":%s}\n",
           jb(relay_coordinator_has_active_turn(co, "/tmp/p1", "conv-1")));
    relay_lease_t *lease = relay_coordinator_acquire_conversation(
        co, "/tmp/p1", "conv-1", "telegram", NULL, "gpt-4o");
    printf("{\"case\":\"lease_session_id\",\"out\":%s}\n", jstr(relay_lease_session_id(lease)));
    printf("{\"case\":\"lease_platform\",\"out\":%s}\n", jstr(relay_lease_platform(lease)));
    printf("{\"case\":\"lease_profile\",\"out\":%s}\n", jstr(relay_lease_profile_key(lease)));
    printf("{\"case\":\"lease_released_initially\",\"out\":%s}\n",
           jb(relay_lease_released(lease)));
    /* Python builds a FRESH lease each acquire — never a cached one. */
    {
        relay_lease_t *again = relay_coordinator_acquire_conversation(
            co, "/tmp/p1", "conv-1", "telegram", NULL, "gpt-4o");
        printf("{\"case\":\"lease_not_cached\",\"out\":%s}\n", jb(again != lease));
        relay_lease_free(again);
    }
    printf("{\"case\":\"lease_empty_session\",\"out\":%s}\n",
           jb(relay_coordinator_acquire_conversation(co, "/tmp/p1", "", NULL, NULL, NULL) == NULL));

    relay_turn_t *turn = relay_coordinator_begin_turn(co, lease, "turn-1", "task-9");
    printf("{\"case\":\"turn_id\",\"out\":%s}\n", jstr(relay_turn_id(turn)));
    printf("{\"case\":\"turn_task_id\",\"out\":%s}\n", jstr(relay_turn_task_id(turn)));
    printf("{\"case\":\"turn_closed_initially\",\"out\":%s}\n", jb(relay_turn_closed(turn)));
    printf("{\"case\":\"turn_is_current\",\"out\":%s}\n", jb(relay_current_turn() == turn));
    printf("{\"case\":\"has_active_turn\",\"out\":%s}\n",
           jb(relay_coordinator_has_active_turn(co, "/tmp/p1", "conv-1")));
    printf("{\"case\":\"has_active_turn_other_conv\",\"out\":%s}\n",
           jb(relay_coordinator_has_active_turn(co, "/tmp/p1", "conv-other")));
    printf("{\"case\":\"turn_generated_id_len\",\"out\":%d}\n",
           (int)strlen(relay_turn_id(turn)));

    /* logical LLM child scopes */
    relay_turn_add_logical_call(turn, "req-1", NULL);
    relay_turn_add_logical_call(turn, "req-2", NULL);
    relay_turn_add_logical_call(turn, "req-1", NULL);   /* dict: replace */
    printf("{\"case\":\"logical_call_count\",\"out\":%d}\n",
           (int)relay_turn_logical_call_count(turn));
    relay_coordinator_finish_logical_calls(co, turn, NULL);
    printf("{\"case\":\"logical_calls_after_finish\",\"out\":%d}\n",
           (int)relay_turn_logical_call_count(turn));

    /* Two concurrent turns on one conversation: the active set holds both,
     * and the conversation stays active until the LAST one ends. */
    relay_turn_t *tc = relay_coordinator_begin_turn(co, lease, "turn-2", NULL);
    printf("{\"case\":\"two_turns_active\",\"out\":%s}\n",
           jb(relay_coordinator_has_active_turn(co, "/tmp/p1", "conv-1")));
    relay_coordinator_end_turn(co, tc, NULL);
    printf("{\"case\":\"still_active_after_one_end\",\"out\":%s}\n",
           jb(relay_coordinator_has_active_turn(co, "/tmp/p1", "conv-1")));
    relay_turn_free(tc);

    relay_coordinator_end_turn(co, turn, "ok");
    printf("{\"case\":\"has_active_turn_after_end\",\"out\":%s}\n",
           jb(relay_coordinator_has_active_turn(co, "/tmp/p1", "conv-1")));
    printf("{\"case\":\"turn_closed_after_end\",\"out\":%s}\n", jb(relay_turn_closed(turn)));
    printf("{\"case\":\"current_turn_after_end\",\"out\":%s}\n",
           jb(relay_current_turn() == NULL));
    relay_turn_free(turn);

    /* a turn with a generated id */
    relay_turn_t *t2 = relay_coordinator_begin_turn(co, lease, NULL, NULL);
    printf("{\"case\":\"generated_turn_id_len\",\"out\":%d}\n", (int)strlen(relay_turn_id(t2)));
    relay_coordinator_end_turn(co, t2, NULL);
    relay_turn_free(t2);

    relay_coordinator_release_conversation(lease);
    printf("{\"case\":\"lease_released_after_release\",\"out\":%s}\n",
           jb(relay_lease_released(lease)));
    /* A released lease must refuse to start a new turn. */
    printf("{\"case\":\"begin_turn_on_released_lease\",\"out\":%s}\n",
           jb(relay_coordinator_begin_turn(co, lease, "t3", NULL) == NULL));
    relay_coordinator_finalize_conversation(co, "/tmp/p1", "conv-1");
    printf("{\"case\":\"active_turn_after_finalize\",\"out\":%s}\n",
           jb(relay_active_turn("conv-1") == NULL));
    relay_lease_free(lease);

    relay_coordinator_shutdown_profile(co, "/tmp/p1");
    printf("{\"case\":\"shutdown_profile_ok\",\"out\":true}\n");
    relay_coordinator_free(co);
    relay_host_registry_free(reg);

    /* ── 9. module-level accessors with no backend ────────────────────── */
    printf("{\"case\":\"get_runtime_no_create\",\"out\":%s}\n",
           jb(relay_get_runtime(false, "/tmp/pX") == NULL));
    printf("{\"case\":\"get_runtime_create\",\"out\":%s}\n",
           jb(relay_get_runtime(true, "/tmp/pX") == NULL));
    printf("{\"case\":\"get_host_create_available\",\"out\":%s}\n",
           jb(relay_host_available(relay_get_host(true, "/tmp/pX"))));
    printf("{\"case\":\"get_session_handle_module\",\"out\":%s}\n",
           jb(relay_get_session_handle("s") == NULL));
    printf("{\"case\":\"module_emit_mark\",\"out\":%s}\n",
           jb(relay_emit_mark("m", "s", NULL, NULL)));
    printf("{\"case\":\"module_ensure_session\",\"out\":%s}\n",
           jb(relay_ensure_session("s") == NULL));
    printf("{\"case\":\"module_run_in_session\",\"out\":%s}\n",
           jb(relay_run_in_session("s", NULL, NULL, NULL)));
    {
        const char *args = "{\"k\":1}";
        char *out = relay_apply_tool_request_intercepts("s", "t", args);
        printf("{\"case\":\"module_intercepts\",\"out\":%s}\n", jstr(out));
        free(out);
        out = relay_apply_tool_request_intercepts("", "t", args);
        printf("{\"case\":\"module_intercepts_empty_session\",\"out\":%s}\n", jstr(out));
        free(out);
    }
    {
        relay_runtime_t *ort = NULL; relay_session_t *osess = NULL; relay_handle_t oh = NULL;
        bool got = relay_resolve_execution_context("s", &ort, &osess, &oh);
        printf("{\"case\":\"resolve_execution_context\",\"out\":%s}\n", jb(got));
    }

    /* ── 10. _is_relay_wrapped_callback_error ─────────────────────────── */
    struct { const char *rk, *rm, *ck, *cm; } errs[] = {
        { "RuntimeError", "internal error: ValueError: boom", "ValueError", "boom" },
        { "RuntimeError", "internal error: ValueError: boom", "TypeError",  "boom" },
        { "RuntimeError", "internal error: builtins.ValueError: boom", "builtins.ValueError", "boom" },
        { "ValueError",   "internal error: ValueError: boom", "ValueError", "boom" },
        { "RuntimeError", "policy denied", "ValueError", "boom" },
        { "ValueError",   "boom", "ValueError", "boom" },
        { "RuntimeError", "internal error: ValueError: boom trailing", "ValueError", "boom" },
    };
    for (size_t i = 0; i < sizeof(errs)/sizeof(errs[0]); i++) {
        printf("{\"case\":\"wrapped_error\",\"rk\":%s,\"rm\":%s,\"ck\":%s,\"cm\":%s,\"out\":%s}\n",
               jstr(errs[i].rk), jstr(errs[i].rm), jstr(errs[i].ck), jstr(errs[i].cm),
               jb(relay_is_relay_wrapped_callback_error(errs[i].rk, errs[i].rm,
                                                        errs[i].ck, errs[i].cm)));
    }

    /* ── 11. reset ────────────────────────────────────────────────────── */
    relay_reset_for_tests();
    printf("{\"case\":\"after_reset_no_host\",\"out\":%s}\n",
           jb(relay_get_host(false, "/tmp/pX") == NULL));
    printf("{\"case\":\"after_reset_no_turn\",\"out\":%s}\n", jb(relay_current_turn() == NULL));
    return 0;
}
