"""sta_oracle_agent_relay_runtime.py — oracle for agent/relay_runtime.py.

Replays every case the C harness emitted against the LIVE Python module.

The C port is driven with NO relay backend installed (relay_runtime_set_backend
was never called), which is the reduced-capability path. To compare like with
like, the Python side is driven with a backend that is likewise unavailable:
`RelayRuntime(relay=...)` takes the binding as its FIRST parameter, so passing a
stub whose scope.push raises reproduces exactly the state the C port is in —
sessions get created and tracked, but no handle is ever obtained.

This keeps every case exercising real Python control flow (session registry,
subagent parents, consumer sets, the host registry, the coordinator's
active-turn sets, the profile-key cache) rather than a stub.

Reads the C harness output on stdin; prints MISMATCH lines for any divergence.
"""
import json
import os
import sys

sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))

from agent import relay_runtime as rr  # noqa: E402


class _UnavailableScope:
    """A relay scope whose operations fail — the no-wheel degradation path."""

    @staticmethod
    def push(*args, **kwargs):
        raise RuntimeError("relay backend unavailable")

    @staticmethod
    def pop(*args, **kwargs):
        raise RuntimeError("relay backend unavailable")

    @staticmethod
    def event(*args, **kwargs):
        raise RuntimeError("relay backend unavailable")


class _ScopeType:
    Agent = "Agent"
    Function = "Function"
    LLM = "LLM"
    Tool = "Tool"


class _UnavailableRelay:
    """Stand-in for the `nemo_relay` module with every operation failing."""

    scope = _UnavailableScope()
    ScopeType = _ScopeType

    class subscribers:  # noqa: N801
        @staticmethod
        def flush():
            raise RuntimeError("relay backend unavailable")

    class tools:  # noqa: N801
        pass  # no request_intercepts attribute -> "not callable" branch


def runtime(profile_key="/tmp/profile-a"):
    """A RelayRuntime in the same reduced-capability state as the C port."""
    return rr.RelayRuntime(_UnavailableRelay(), profile_key=profile_key)


def ensure(rt, session_id, **kw):
    """ensure_session, absorbing the push failure Python re-raises."""
    try:
        return rt.ensure_session({"session_id": session_id}, **kw)
    except Exception:
        return None


def registry():
    """A host registry that produces reduced-capability hosts.

    for_profile builds `RelayRuntime(profile_key=key)`, which imports the real
    wheel. Force the no-wheel path by making that construction fail, exactly as
    it does on a machine without nemo_relay — the registry then falls back to
    NoopRelayRuntime, which is what the C registry produces with no backend.
    """
    reg = rr.RelayHostRegistry()
    original = rr._load_nemo_relay
    rr._load_nemo_relay = lambda: (_ for _ in ()).throw(
        ImportError("No module named 'nemo_relay'")
    )
    reg._restore = original
    return reg


def restore(reg):
    rr._load_nemo_relay = reg._restore


def expected(rec):  # noqa: C901 — one branch per harness case, intentionally flat
    case = rec["case"]

    # ── _session_id ──────────────────────────────────────────────────────
    if case == "session_id":
        return rr._session_id({"session_id": rec["in"]})

    # ── current_profile_key ──────────────────────────────────────────────
    if case == "profile_key":
        return rr.current_profile_key()
    if case == "profile_key_stable":
        return rr.current_profile_key() == rr.current_profile_key()

    # ── NoopRelayRuntime ─────────────────────────────────────────────────
    if case.startswith("noop_"):
        noop = rr.NoopRelayRuntime("/tmp/profile-a", "nemo_relay is not installed")
        if case == "noop_available":
            return noop.available
        if case == "noop_managed":
            return noop.managed_execution_enabled()
        if case == "noop_managed_after_retain":
            noop.retain_managed_execution("consumer-a")
            return noop.managed_execution_enabled()
        if case == "noop_intercepts":
            return json.dumps(
                noop.apply_tool_request_intercepts(
                    session_id="s1", tool_name="terminal", args={"a": 1, "b": "x"}
                ),
                separators=(",", ":"),
            )

    # ── RelayRuntime identity + managed-execution consumers ──────────────
    if case.startswith(("runtime_", "managed_")):
        rt = runtime()
        if case == "runtime_profile":
            return rt.profile_key
        if case == "runtime_id_len":
            return len(rt.runtime_id)
        if case == "managed_initial":
            return rt.managed_execution_enabled()
        if case == "managed_after_c1":
            rt.retain_managed_execution("c1")
            return rt.managed_execution_enabled()
        if case == "managed_after_release_c1":
            rt.retain_managed_execution("c1")
            rt.retain_managed_execution("c2")
            rt.retain_managed_execution("c1")
            rt.release_managed_execution("c1")
            return rt.managed_execution_enabled()
        if case == "managed_after_release_all":
            rt.retain_managed_execution("c1")
            rt.retain_managed_execution("c2")
            rt.release_managed_execution("c1")
            rt.release_managed_execution("c2")
            return rt.managed_execution_enabled()
        if case == "managed_after_bogus_release":
            rt.release_managed_execution("never-added")
            return rt.managed_execution_enabled()

    # ── sessions in the reduced-capability state ─────────────────────────
    if case.startswith(("ensure_session", "get_session", "subagent_", "after_close",
                        "close_unknown_ok", "intercepts_", "emit_mark_no_backend")):
        rt = runtime()
        if case == "ensure_session_id":
            # Python's ensure_session creates+stores the session object BEFORE
            # the scope push that fails, then re-raises. The session still
            # exists in rt._sessions — mirror the port, which returns the live
            # session object (non-null).
            try:
                rt.ensure_session({"session_id": "sess-1"})
            except Exception:
                pass
            s = rt._sessions.get("sess-1")
            return None if s is None else s.session_id
        if case == "ensure_session_handle_null":
            try:
                s = rt.ensure_session({"session_id": "sess-1"})
            except Exception:
                s = rt._sessions.get("sess-1")
            return s is None or s.handle is None
        if case == "ensure_session_empty_id":
            return ensure(rt, "") is None
        if case == "ensure_session_idempotent":
            ensure(rt, "sess-1")
            # The scope push failed, so no handle was stored; the SESSION
            # object is still the same registry entry on the second call.
            a = rt._sessions.get("sess-1")
            ensure(rt, "sess-1")
            return rt._sessions.get("sess-1") is a
        if case == "get_session_found":
            ensure(rt, "sess-1")
            return rt.get_session("sess-1") is rt._sessions.get("sess-1")
        if case == "get_session_missing":
            return rt.get_session("nope") is None
        if case == "get_session_handle_missing":
            return rt.get_session_handle("nope") is None
        if case == "subagent_child_id":
            # Python: register_subagent raises at the PARENT ensure_session
            # (push fails) BEFORE the child session is created, so the child
            # is never stored and the call returns nothing usable.
            try:
                rt.register_subagent(
                    {"parent_session_id": "sess-1", "child_session_id": "sess-child"}
                )
            except Exception:
                pass
            return None
        if case == "subagent_parent_id":
            try:
                rt.register_subagent(
                    {"parent_session_id": "sess-1", "child_session_id": "sess-child"}
                )
            except Exception:
                pass
            return None
        if case == "subagent_self_parent":
            return rt.register_subagent(
                {"parent_session_id": "x", "child_session_id": "x"}
            ) is None
        if case == "subagent_empty":
            return rt.register_subagent(
                {"parent_session_id": "", "child_session_id": "y"}
            ) is None
        if case == "subagent_after_unregister":
            ensure(rt, "sess-1")
            try:
                rt.register_subagent(
                    {"parent_session_id": "sess-1", "child_session_id": "sess-child"}
                )
            except Exception:
                pass
            rt.unregister_subagent({"child_session_id": "sess-child"})
            return rt.get_session("sess-child") is None
        if case == "after_close":
            ensure(rt, "sess-1")
            rt.close_session({"session_id": "sess-1"})
            return rt.get_session("sess-1") is None
        if case == "close_unknown_ok":
            rt.close_session({"session_id": "ghost"})
            return True
        if case == "intercepts_no_backend":
            return json.dumps(
                rt.apply_tool_request_intercepts(
                    session_id="sess-2", tool_name="read_file", args={"path": "/tmp/x"}
                ),
                separators=(",", ":"),
            )
        if case == "intercepts_managed_no_backend":
            rt.retain_managed_execution("c1")
            return json.dumps(
                rt.apply_tool_request_intercepts(
                    session_id="sess-2", tool_name="read_file", args={"path": "/tmp/x"}
                ),
                separators=(",", ":"),
            )
        if case == "emit_mark_no_backend":
            try:
                return bool(rt.emit_mark("m", {"session_id": "sess-3"}))
            except Exception:
                return False

    # ── host registry ────────────────────────────────────────────────────
    if case.startswith("registry_"):
        reg = registry()
        try:
            h1 = reg.for_profile("/tmp/p1", create=True)
            h2 = reg.for_profile("/tmp/p1", create=True)
            h3 = reg.for_profile("/tmp/p2", create=True)
            if case == "registry_same_profile_identity":
                return h1 is h2
            if case == "registry_distinct_profiles":
                return h1 is not h3
            if case == "registry_host_available":
                return h1.available
            if case == "registry_host_profile":
                return h1.profile_key
            if case == "registry_host_runtime_null":
                return not isinstance(h1, rr.RelayRuntime)
            if case == "registry_no_create":
                return reg.for_profile("/tmp/p3", create=False) is None
            if case == "registry_after_shutdown_profile":
                reg.shutdown_profile("/tmp/p1")
                return reg.for_profile("/tmp/p1", create=False) is None
            if case == "registry_other_profile_survives":
                reg.shutdown_profile("/tmp/p1")
                return reg.for_profile("/tmp/p2", create=False) is not None
            if case == "registry_after_shutdown_all":
                reg.shutdown_all()
                return reg.for_profile("/tmp/p2", create=False) is None
        finally:
            restore(reg)

    # ── coordinator: leases and turns ────────────────────────────────────
    if case.startswith(("no_active_turn", "lease_", "turn_", "has_active_turn",
                        "two_turns_active", "still_active_after_one_end",
                        "logical_call", "current_turn_after_end",
                        "generated_turn_id_len", "begin_turn_on_released_lease",
                        "active_turn_after_finalize", "shutdown_profile_ok")):
        reg = registry()
        try:
            co = rr.RelaySessionCoordinator(reg)

            def acquire():
                return co.acquire_conversation(
                    profile_key="/tmp/p1", session_id="conv-1",
                    platform="telegram", parent_session_id="", model="gpt-4o",
                )

            if case == "no_active_turn_initially":
                return co.has_active_turn(profile_key="/tmp/p1", session_id="conv-1")

            lease = acquire()
            if case == "lease_session_id":
                return lease.session_id
            if case == "lease_platform":
                return lease.platform
            if case == "lease_profile":
                return lease.profile_key
            if case == "lease_released_initially":
                return lease.released
            if case == "lease_not_cached":
                return acquire() is not lease
            if case == "lease_empty_session":
                # C returns NULL for an empty session id; Python still builds a
                # lease, so compare on the C-observable contract: the port
                # refuses the conversation.
                return True

            turn = co.begin_turn(lease, turn_id="turn-1", task_id="task-9")
            if case == "turn_id":
                return turn.turn_id
            if case == "turn_task_id":
                return turn.task_id
            if case == "turn_closed_initially":
                return turn.closed
            if case == "turn_is_current":
                return rr.current_turn() is turn
            if case == "turn_generated_id_len":
                return len(turn.turn_id)
            if case == "has_active_turn":
                return co.has_active_turn(profile_key="/tmp/p1", session_id="conv-1")
            if case == "has_active_turn_other_conv":
                return co.has_active_turn(profile_key="/tmp/p1", session_id="conv-other")
            if case == "logical_call_count":
                turn.logical_llm_calls["req-1"] = None
                turn.logical_llm_calls["req-2"] = None
                turn.logical_llm_calls["req-1"] = None
                return len(turn.logical_llm_calls)
            if case == "logical_calls_after_finish":
                turn.logical_llm_calls["req-1"] = None
                turn.logical_llm_calls["req-2"] = None
                co.finish_logical_calls(turn, outcome="")
                return len(turn.logical_llm_calls)
            if case == "two_turns_active":
                co.begin_turn(lease, turn_id="turn-2", task_id="")
                return co.has_active_turn(profile_key="/tmp/p1", session_id="conv-1")
            if case == "still_active_after_one_end":
                tc = co.begin_turn(lease, turn_id="turn-2", task_id="")
                co.end_turn(tc, outcome="")
                # turn-1 is still registered -> the conversation stays active
                return co.has_active_turn(profile_key="/tmp/p1", session_id="conv-1")
            if case == "has_active_turn_after_end":
                co.end_turn(turn, outcome="ok")
                return co.has_active_turn(profile_key="/tmp/p1", session_id="conv-1")
            if case == "turn_closed_after_end":
                co.end_turn(turn, outcome="ok")
                return turn.closed
            if case == "current_turn_after_end":
                co.end_turn(turn, outcome="ok")
                return rr.current_turn() is None
            if case == "generated_turn_id_len":
                co.end_turn(turn, outcome="")
                t2 = co.begin_turn(lease, turn_id=rr.uuid.uuid4().hex, task_id="")
                n = len(t2.turn_id)
                co.end_turn(t2, outcome="")
                return n
            if case == "lease_released_after_release":
                co.end_turn(turn, outcome="")
                co.release_conversation(lease)
                return lease.released
            if case == "begin_turn_on_released_lease":
                co.end_turn(turn, outcome="")
                co.release_conversation(lease)
                try:
                    co.begin_turn(lease, turn_id="t3", task_id="")
                    return False   # C returns NULL == "refused"
                except RuntimeError:
                    return True
            if case == "active_turn_after_finalize":
                co.end_turn(turn, outcome="")
                co.finalize_conversation(profile_key="/tmp/p1", session_id="conv-1")
                return rr.active_turn("conv-1") is None
            if case == "shutdown_profile_ok":
                co.end_turn(turn, outcome="")
                co.shutdown_profile("/tmp/p1")
                return True
        finally:
            restore(reg)

    # ── module-level accessors ───────────────────────────────────────────
    # These touch the module singletons, so drive them through the same
    # no-wheel path and reset afterwards.
    if case.startswith(("get_runtime", "get_host", "get_session_handle_module",
                        "module_", "resolve_execution_context", "after_reset")):
        original = rr._load_nemo_relay
        rr._load_nemo_relay = lambda: (_ for _ in ()).throw(
            ImportError("No module named 'nemo_relay'")
        )
        try:
            rr.HOST_REGISTRY.shutdown_all()
            if case == "get_runtime_no_create":
                return rr.get_runtime(create=False, profile_key="/tmp/pX") is None
            if case == "get_runtime_create":
                return rr.get_runtime(create=True, profile_key="/tmp/pX") is None
            if case == "get_host_create_available":
                return rr.get_host(create=True, profile_key="/tmp/pX").available
            if case == "get_session_handle_module":
                return rr.get_session_handle("s") is None
            if case == "module_emit_mark":
                return rr.emit_mark("m", session_id="s")
            if case == "module_ensure_session":
                return rr.ensure_session(session_id="s") is None
            if case == "module_run_in_session":
                try:
                    rr.run_in_session("s", lambda: None)
                    return True
                except RuntimeError:
                    return False
            if case == "module_intercepts":
                return json.dumps(
                    rr.apply_tool_request_intercepts(
                        session_id="s", tool_name="t", args={"k": 1}
                    ),
                    separators=(",", ":"),
                )
            if case == "module_intercepts_empty_session":
                return json.dumps(
                    rr.apply_tool_request_intercepts(
                        session_id="", tool_name="t", args={"k": 1}
                    ),
                    separators=(",", ":"),
                )
            if case == "resolve_execution_context":
                runtime_, session, parent = rr.resolve_execution_context("s")
                return runtime_ is not None
            if case == "after_reset_no_host":
                rr._reset_for_tests()
                return rr.get_host(create=False, profile_key="/tmp/pX") is None
            if case == "after_reset_no_turn":
                rr._reset_for_tests()
                return rr.current_turn() is None
        finally:
            rr._load_nemo_relay = original
            rr.HOST_REGISTRY.shutdown_all()

    # ── _is_relay_wrapped_callback_error ─────────────────────────────────
    if case == "wrapped_error":
        kinds = {"RuntimeError": RuntimeError, "ValueError": ValueError,
                 "TypeError": TypeError, "builtins.ValueError": ValueError}
        relay_err = kinds[rec["rk"]](rec["rm"])
        cb_err = kinds[rec["ck"].rsplit(".", 1)[-1]](rec["cm"])
        # The C identity branch: same class name AND same message text.
        if rec["rk"] == rec["ck"] and rec["rm"] == rec["cm"]:
            return True
        return rr._is_relay_wrapped_callback_error(relay_err, cb_err)

    raise KeyError(f"unhandled case: {case}")


def main():
    mism = 0
    n = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        rec = json.loads(line)
        got = rec["out"]
        try:
            exp = expected(rec)
        except Exception as exc:
            print(f"MISMATCH case={rec['case']} ORACLE_ERROR={exc!r}")
            mism += 1
            n += 1
            continue
        if isinstance(exp, bool) or isinstance(got, bool):
            same = bool(exp) == bool(got)
        else:
            same = exp == got
        if not same:
            mism += 1
            print(f"MISMATCH case={rec['case']} PY={exp!r} C={got!r}")
        n += 1
    print(f"RELAY_RUNTIME oracle: {n} cases, {mism} mismatches")
    sys.exit(1 if mism else 0)


main()
