#!/usr/bin/env python3
"""Differential oracle for tui_gateway/session_auto_continue.py (gate w31_sessionaut_4e7286).

The module is imported from the quarry and its *seams* are bound here: the wall
clock, the pid, the config file, the durable turn marker, the marker-writer
probe, the worker starter.  Everything else is the live Python code, so what is
compared is exactly what those values decide:

  * the recovery note and the desktop.auto_continue coercion;
  * the busy-submit mode precedence;
  * the queue fold (_enqueue_prompt + its self-duplicate scrub + merge);
  * the inflight / queued live-session projections;
  * the whole _maybe_schedule_auto_continue decision (return value plus the two
    side effects the caller observes: marker cleared, schedule flag set).
"""
import copy
import json
import pathlib
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_sessionaut_4e7286"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
# tests/ must be importable for the seam stubs, and it must NOT shadow the quarry.
sys.path.append(str(Path(__file__).resolve().parent))

from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

from tui_gateway import session_auto_continue as live  # noqa: E402
from tui_gateway import session_history as sh  # noqa: E402
from utils.env import is_truthy_value  # noqa: E402


def _server_function(name):
    """Pull one function out of tui_gateway/server.py without importing it:
    importing the server pulls the whole runtime (and needs a HOME)."""
    import ast
    tree = ast.parse((QUARRY / "tui_gateway" / "server.py").read_text(encoding="utf-8"))
    node = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == name)
    ns = {"Any": object}
    exec(compile(ast.Module(body=[node], type_ignores=[]), "<server.py:%s>" % name, "exec"), ns)
    return ns[name]


_coerce_int_config_value = _server_function("_coerce_int_config_value")

WANTED = {
    "note", "config", "busy_mode", "inflight_original", "sanitize_entry",
    "set_queue", "enqueue", "inflight_snapshot", "queued_snapshot", "maybe_schedule",
}


class _Log:
    def info(self, *a, **k): pass
    def debug(self, *a, **k): pass
    def warning(self, *a, **k): pass


class _Seams:
    cfg = {}
    marker = None
    cleared = []
    writer_state = "dead"
    now = 0.0
    pid = 0


SEAMS = _Seams()

# ---- bind every seam the module bodies read bare -------------------------
live.logger = _Log()
live._AUTO_CONTINUE_NOTE_PREFIX = sh._AUTO_CONTINUE_NOTE_PREFIX
live._inflight_text = sh._inflight_text
live.is_truthy_value = is_truthy_value
live._coerce_int_config_value = _coerce_int_config_value
live.Path = pathlib.Path
live._hermes_home = "/tmp/slermes-oracle-home"
live._load_cfg = lambda: SEAMS.cfg
live.read_turn_marker = lambda home, key: SEAMS.marker
live.clear_turn_marker = lambda home, key: SEAMS.cleared.append(key)
live.marker_writer_state = lambda marker: SEAMS.writer_state
live.time = types.SimpleNamespace(time=lambda: SEAMS.now)
live.os = types.SimpleNamespace(getpid=lambda: SEAMS.pid)
live._start_session_work = lambda fn, name=None: object()
live._start_agent_build = lambda sid, session: None
live._wait_agent = lambda session, rid, timeout=0.0: None


def _cfg(root):
    SEAMS.cfg = root if isinstance(root, dict) else {}


def note(req):
    return live._auto_continue_note(req.get("prompt"))


def config(req):
    _cfg(req.get("cfg"))
    enabled, freshness, attempts = live._auto_continue_config()
    return [enabled, freshness, attempts]


def busy_mode(req):
    queued = req.get("queued")
    mode = "queue" if queued else req.get("configured")
    if mode in ("steer", "interrupt") and req.get("compression"):
        mode = "queue"
    return mode


def inflight_original(req):
    return live._ac_inflight_original(req.get("session") if isinstance(req.get("session"), dict) else {})


def sanitize_entry(req):
    return live._sanitize_queued_entry_vs_inflight_user(req.get("entry"), req.get("original") or "")


def set_queue(req):
    session = copy.deepcopy(req.get("session") or {})
    entries = req.get("entries")
    live._ac_set_queue(session, list(entries) if isinstance(entries, list) else [])
    return session


def enqueue(req):
    session = copy.deepcopy(req.get("session") or {})
    images = req.get("image_paths")
    envelope = live._enqueue_prompt(session, req.get("text"), req.get("transport"),
                                    image_paths=list(images) if isinstance(images, list) else None,
                                    turn_author=req.get("turn_author"))
    return {"envelope": envelope, "session": session}


def inflight_snapshot(req):
    session = req.get("session")
    return live._inflight_snapshot(session if isinstance(session, dict) else {})


def queued_snapshot(req):
    session = req.get("session")
    return live._queued_prompt_snapshot(session if isinstance(session, dict) else {})


def maybe_schedule(req):
    SEAMS.marker = req.get("marker")
    SEAMS.cleared = []
    SEAMS.writer_state = req.get("writer_state") or "dead"
    SEAMS.now = float(req.get("now") or 0.0)
    SEAMS.pid = int(req.get("pid") or 0)
    _cfg(req.get("cfg"))
    session = copy.deepcopy(req.get("session") or {})
    ret = live._maybe_schedule_auto_continue("sid", session, session.get("session_key") or "")
    # "scheduled" is the DECISION (the C core reports it): upstream signals it by
    # returning a descriptor and flipping the flag; reading the flag back would
    # conflate a pre-existing True with a fresh schedule.
    return {"out": ret, "cleared": bool(SEAMS.cleared), "scheduled": ret is not None}


DISPATCH = {
    "note": note,
    "config": config,
    "busy_mode": busy_mode,
    "inflight_original": inflight_original,
    "sanitize_entry": sanitize_entry,
    "set_queue": set_queue,
    "enqueue": enqueue,
    "inflight_snapshot": inflight_snapshot,
    "queued_snapshot": queued_snapshot,
    "maybe_schedule": maybe_schedule,
}


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
            py = DISPATCH[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py =%r\n  c  =%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
