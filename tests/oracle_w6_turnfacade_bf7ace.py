#!/usr/bin/env python3
"""Differential oracle for agent/turn_facade.py (noc_w6_turnfacade_bf7ace.h).

The mixin's turn orchestration is replayed against the LIVE TurnFacadeMixin
with deterministic collaborator stubs installed in sys.modules; the values the
C seams derive (task context, relay ids, outcomes) are captured from the live
call itself.
"""
import contextlib
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/agent/turn_facade.py"
sys.path.insert(0, QUARRY)

if "agent" not in sys.modules:
    _pkg = types.ModuleType("agent")
    _pkg.__path__ = [QUARRY + "/agent"]
    sys.modules["agent"] = _pkg
_lf = types.ModuleType("agent.lazy_forward")
_lf.forward = lambda module, name: (lambda self: None)
sys.modules["agent.lazy_forward"] = _lf

spec = importlib.util.spec_from_file_location("turn_facade_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["turn_facade_live"] = live
spec.loader.exec_module(live)


def _canon(x):
    if x is None:
        return ("null",)
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", round(float(x), 9))
    if isinstance(x, str):
        return ("str", x)
    if isinstance(x, (list, tuple)):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


@contextlib.contextmanager
def _noop_cm(*a, **k):
    yield


class _Uuid:
    def __init__(self, hexv):
        self.hex = hexv

    def __str__(self):
        return "00000000-0000-0000-0000-" + self.hex


class _Turn:
    pass


class _Coord:
    def __init__(self, state):
        self.state = state

    def acquire_conversation(self, **kw):
        self.state["acquire"] = dict(kw)
        return object()

    def begin_turn(self, lease, **kw):
        self.state["begin"] = dict(kw)
        return _Turn()

    def finish_logical_calls(self, turn, outcome=None):
        self.state["outcome"] = outcome

    def end_turn(self, turn, outcome=None):
        self.state["end_outcome"] = outcome

    def release_conversation(self, lease):
        pass


class _Queue:
    def note_turn_started(self):
        pass

    def note_turn_finished(self):
        pass


class _Self:
    def __init__(self, r):
        self.session_id = r.get("session_id")
        self.platform = r.get("platform")
        self._parent_session_id = r.get("parent_session_id")
        self._relay_pending_turn_id = None

    def _conversation_root_id(self):
        return "root"

    def _reset_activity_labels_after_turn(self):
        pass


class _ChatSelf:
    def __init__(self, result):
        self.result = result

    def run_conversation(self, message, stream_callback=None):
        return self.result


def _mkmod(name, **attrs):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        sys.modules[name] = m
    for k, v in attrs.items():
        setattr(m, k, v)
    parent, _, leaf = name.rpartition(".")
    if parent in sys.modules:
        setattr(sys.modules[parent], leaf, m)
    return m


_CancelledError = type("CancelledError", (Exception,), {})
_EXCS = {
    "KeyboardInterrupt": KeyboardInterrupt,
    "InterruptedError": InterruptedError,
    "CancelledError": _CancelledError,
    "TimeoutError": TimeoutError,
    "ValueError": ValueError,
}

for _pk, _sub in (("hermes_cli", "hermes_cli"),
                  ("hermes_cli.observability", "hermes_cli/observability")):
    if _pk not in sys.modules:
        _m = types.ModuleType(_pk)
        _m.__path__ = [QUARRY + "/" + _sub]
        sys.modules[_pk] = _m


def run_live(r):
    state = {}
    coord = _Coord(state)
    hexv = r.get("uuid_hex", "0123456789abcdef")
    fake_uuid = types.ModuleType("uuid")
    fake_uuid.uuid4 = lambda: _Uuid(hexv)
    live.uuid = fake_uuid

    def run_stub(self, *a, **k):
        exc = r.get("raise")
        if exc:
            raise _EXCS[exc]("boom")
        return r.get("result", {})

    _mkmod("agent.background_review",
           cancel_background_review_for_live_turn=lambda self: None)
    _mkmod("agent.relay_runtime", SESSION_COORDINATOR=coord,
           current_profile_key=lambda: "profile")
    _mkmod("agent.agent_init_session", agent_interface=lambda self: object())
    _mkmod("agent.aux_accounting", reset_accounting_context=lambda tok: None,
           set_accounting_context=lambda *a, **k: object())
    _mkmod("agent.auxiliary_client", scoped_runtime_main=_noop_cm)
    _mkmod("agent.conversation_loop", run_conversation=run_stub)
    _mkmod("agent.portal_tags",
           reset_affinity_scope=lambda tok: None,
           reset_conversation_context=lambda tok: None,
           set_affinity_scope=lambda v: object(),
           set_conversation_context=lambda v: object())
    _mkmod("agent.prompt_cache_scope",
           declared_conversation_scope_safe=lambda self: "scope")
    _mkmod("agent.relay_cwd", resolve_relay_scope_cwds=lambda *a: ("/s", "/t"))
    _mkmod("agent.review_idle_queue", QUEUE=_Queue())
    _mkmod("agent.subagent_lifecycle", bind_subagent_parent=_noop_cm)
    _mkmod("agent.interrupt_scope", track_in_interrupt_scope=_noop_cm)
    _mkmod("agent.turn_facade_lease",
           admit_durable_turn_lease=lambda self, **kw: types.SimpleNamespace(
               early_result=None, lease=None, conversation_history=[]),
           carry_unadmitted_user_message=lambda *a, **k: None)

    def start_task_run(**kw):
        state["task_context"] = {k: kw[k] for k in ("session_id", "task_id", "platform")}

    def finish_task_run(**kw):
        pass

    _mkmod("hermes_cli.observability.relay_shared_metrics",
           start_task_run=start_task_run, finish_task_run=finish_task_run)

    try:
        state["return"] = live.TurnFacadeMixin.run_conversation(
            _Self(r), r.get("user_message", "hi"), task_id=r.get("task_id"))
    except BaseException:
        pass
    return state


def compute(r):
    f = r["func"]
    if f == "chat":
        try:
            return live.TurnFacadeMixin.chat(_ChatSelf(r.get("result")), "m")
        except Exception:
            return None
    st = run_live(r)
    if f == "outcome":
        return st.get("outcome")
    if f == "exception_outcome":
        return st.get("outcome")
    if f == "task_context":
        return st.get("task_context")
    if f == "relay_turn_id":
        return st.get("begin", {}).get("turn_id")
    if f == "relay_parent":
        return st.get("acquire", {}).get("parent_session_id")
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE turnfacade: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    if _canon(py) != _canon(r.get("out")):
        mism += 1
        print(f"MISMATCH turnfacade {r.get('func')} case={r.get('case')} "
              f"py={py!r} c={r.get('out')!r}", file=sys.stderr)
print(f"ORACLE turnfacade: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
