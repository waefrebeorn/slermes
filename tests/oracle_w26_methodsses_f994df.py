#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_session_interrupt.py (gate w26_methodsses_f994df).

The live module's three handlers are registered through ``HandlerRegistry`` and
close over server.py globals (``_sess_nowait``, ``_ok``, ``_err``, the queues,
the TTS cut, the compute-host supervisor).  The oracle rebinds them onto a fake
namespace with ``method_ctx.rebind`` -- the project's own split-module seam -- so
the *upstream branch tree* runs verbatim while every transport seam is a value:

  * the session is a plain dict plus a real lock;
  * the agent is an object that has (or lacks) ``steer`` / ``redirect`` and
    returns the case's ``accepted`` flag, or raises;
  * interrupt's compute-host / hosted-task probes read the case's flags.

Nothing here talks to a socket, a thread, or the DB.
"""
import json
import logging
import os
import sys
import threading
import time
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_methodsses_f994df"

# The import chain resolves a hermes home; pin it so the gate never depends on
# the caller's shell.
os.environ["USERPROFILE"] = str(REPO.parent)
os.environ["HOME"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tui_gateway import methods_session_interrupt as live  # noqa: E402
from tui_gateway.method_ctx import rebind  # noqa: E402

HANDLERS = dict(live._registry._pending)
WANTED = {"correction", "interrupt", "constants"}
VERBS = {"steer": "session.steer", "redirect": "session.redirect"}


def make_agent(state):
    """An object with only the verbs the case grants; None is the mid-build window."""
    if state.get("agent") is None:
        return None

    class Agent:
        pass

    agent = Agent()
    accepted = state.get("accepted", True)
    error = state.get("agent_error")

    def _call(text):
        if error:
            raise RuntimeError(str(error))
        return accepted

    if state.get("supports_steer"):
        agent.steer = _call
    if state.get("supports_redirect"):
        agent.redirect = _call
        agent._supports_active_turn_redirect = True
    return agent


def make_session(state):
    return {
        "running": bool(state.get("running")),
        "agent": make_agent(state),
        "session_key": "oracle-session",
        "history_lock": threading.RLock(),
        "last_active": 0.0,
        "_compression_in_flight": bool(state.get("compression_in_flight")),
        "_hosted_room_task": state.get("_hosted_room_task"),
        "compute_host": bool(state.get("compute_host")),
        "queued_prompt": None,
    }


def fake_globals(session):
    g = dict(vars(live))
    g["time"] = time
    g["logger"] = logging.getLogger("oracle.methods_session_interrupt")
    g["_ok"] = lambda rid, result: {"__ok__": result}
    g["_err"] = lambda rid, code, msg, data=None: {"__err__": code, "message": msg}
    g["_sess_nowait"] = lambda params, rid: (session, None)
    g["_str_param"] = lambda params, key, default="": str(params.get(key) or "").strip() or default
    g["_note_user_input"] = lambda s: s.__setitem__("_turn_user_input", True) if s.get("running") else None
    g["_session_compression_in_flight"] = lambda s: bool(s.get("_compression_in_flight"))
    g["_enqueue_prompt"] = lambda s, text, transport: s.__setitem__("queued_prompt", text)
    g["current_transport"] = lambda: None
    g["_stdio_transport"] = object()
    g["_record_inflight_correction"] = lambda s, text: s.__setitem__("_inflight_correction", text)
    g["_drop_queued_duplicates_of_inflight_user"] = lambda s: None
    g["_apply_correction"] = rebind(live._apply_correction, g)
    g["_tts_stream_stop"] = lambda: None
    g["_session_uses_compute_host"] = lambda s: bool(s.get("compute_host"))
    g["_interrupt_session_turn"] = lambda sid, s, request_id=None: None
    g["_retire_turn_marker"] = lambda s, key: None
    g["_resume_wake_after_interrupt"] = lambda: None
    g["_recover_unreachable_compute_host_turn"] = lambda sid, s, inflight: None
    return g


def call_handler(name, params, session):
    fn = rebind(HANDLERS[name], fake_globals(session))
    res = fn("rid-1", params if isinstance(params, dict) else {})
    if "__ok__" in res:
        return {"ok": True, "result": res["__ok__"]}
    return {"ok": False, "code": res["__err__"], "message": res["message"]}


def expected(r):
    f = r.get("func")
    if f == "constants":
        return {"methods": sorted(live._registry.names())}
    params = r.get("params")
    state = r.get("session")
    if not isinstance(state, dict):
        state = {}
    if f == "correction":
        verb = r.get("verb")
        return call_handler(VERBS.get(verb, "session.steer"), params, make_session(state))
    if f == "interrupt":
        return call_handler("session.interrupt", params, make_session(state))
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
