#!/usr/bin/env python3
"""Differential oracle for agent/agent_init_memory_provider.py (gate w26_agentinitm_60b1c8).

The agent, its session store and the profile lookup are the seams.  Each case
builds a stub agent from the fixture's state (a ``SimpleNamespace`` carrying
the attributes the live code reads, a fake store answering ``list_row`` /
``latest_row_id`` / ``get_title``), so the live function decides exactly what a
real agent init would:

  * ``_memory_provider_init_kwargs`` -- the scoping kwargs ``initialize`` gets;
  * ``_stored_transcript_kwargs``   -- ``resumed`` and ``message_count``;
  * ``_warn_memory_provider_unavailable`` -- the one-per-home advisory text.

The agent's user-facing sinks (``_emit_warning`` / ``_emit_status``) are
callables, not values: they are reported as ``{"__callable__": "<param>"}`` on
both sides.
"""
import json
import os
import sys
import tempfile
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_agentinitm_60b1c8"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_HOME = tempfile.mkdtemp(prefix="w26_agentinitm_")
os.environ["HOME"] = _HOME
os.environ["USERPROFILE"] = _HOME

from agent import agent_init_memory_provider as live  # noqa: E402
import hermes_cli.profiles as profiles_mod  # noqa: E402

live.logger.setLevel(50)  # the advisory is compared as text, not logged

WANTED = {"init_kwargs", "stored_transcript", "unavailable_message", "constants"}


class _Row:
    def __init__(self, message_count):
        self.message_count = message_count


class _Source:
    def __init__(self, value):
        self.value = value


def _store(state):
    """A session store whose answers are the fixture's facts."""
    class _Sessions:
        def list_row(self, sid):
            return _Row(state.get("count")) if state.get("row") else None

        def get_title(self, sid):
            return state.get("session_title")

        def get_title_source(self, sid):
            src = state.get("session_title_source")
            return _Source(src) if src else None

    class _Messages:
        def latest_row_id(self, sid, role=None, require_text=True):
            return 1 if state.get("has_assistant") else None

    class _Store:
        sessions = _Sessions()
        messages = _Messages()

    return _Store()


class _SessionDb:
    def __init__(self, state):
        self.state = state

    def get_session_list_row(self, sid):
        return {"message_count": self.state.get("count")} if self.state.get("row") else None

    def latest_message_row_id(self, sid, role=None, require_text=True):
        return 1 if self.state.get("has_assistant") else None

    def get_session_title(self, sid):
        return self.state.get("session_title")

    def get_session_title_source(self, sid):
        return self.state.get("session_title_source")


def _agent(state):
    use_store = bool(state.get("use_store"))
    store = _store(state) if use_store else None
    live.agent_interface = lambda agent: store
    live.get_hermes_home = lambda: state.get("hermes_home", "")
    if state.get("profile_raises"):
        def _no_profile():
            raise RuntimeError("no active profile")
        profiles_mod.get_active_profile_name = _no_profile
    else:
        profiles_mod.get_active_profile_name = lambda: state.get("profile")

    class _Agent:
        session_id = state.get("session_id")
        _parent_session_id = state.get("parent_session_id")
        memory_agent_context = state.get("memory_agent_context")
        session_cwd = state.get("session_cwd")
        _session_db = _SessionDb(state) if state.get("have_db") else None
        _emit_warning = staticmethod(lambda message: None)
        _emit_status = staticmethod(lambda message: None)

    for ident in live._GATEWAY_IDENTITY_PARAMS:
        setattr(_Agent, "_" + ident, None)
    for key, value in (state.get("identities") or {}).items():
        setattr(_Agent, "_" + key, value)
    return _Agent()


def _normalise(kwargs):
    return {key: ({"__callable__": key} if callable(value) else value)
            for key, value in kwargs.items()}


def init_kwargs(req):
    agent = _agent(req)
    return _normalise(live._memory_provider_init_kwargs(agent, req.get("platform")))


def stored_transcript(req):
    agent = _agent(req)
    return live._stored_transcript_kwargs(agent)


def unavailable_message(req):
    collected = []
    live._warned_unavailable_providers.clear()
    live._warn_memory_provider_unavailable(req.get("name"), req.get("reason") or "",
                                          say=collected.append)
    return collected[0] if collected else None


def constants():
    return {
        "identity_params": list(live._GATEWAY_IDENTITY_PARAMS),
        "default_platform": "cli",
        "primary_context": "primary",
        "agent_workspace": "hermes",
        "warning_kind_unavailable": "unavailable",
        "warning_kind_parked": "parked",
        "warning_kind_load": "load",
    }


def expected(r):
    f = r.get("func")
    if f == "init_kwargs":
        return init_kwargs(r)
    if f == "stored_transcript":
        return stored_transcript(r)
    if f == "unavailable_message":
        return unavailable_message(r)
    if f == "constants":
        return constants()
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
