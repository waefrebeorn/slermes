#!/usr/bin/env python3
"""Differential oracle for hermes_cli/kanban_notify_thread.py
(gate w25_kanbannoti_2979d2).

Two seams are pinned and nothing else:

  * ``hermes_cli.kanban_db_notify.list_notify_subs`` returns the fixture rows
    (the real one needs a kanban DB connection);
  * ``_session_thread_for`` is replaced by the same body the module has, with
    ``get_session_env`` answered from the fixture's session identity.

``_chat_threads``, ``resolve_unsubscribe_thread`` and
``resolve_subscribe_thread`` are the live functions; the subscribe stderr
report is captured from the real ``print(..., file=sys.stderr)``.
"""
import contextlib
import importlib
import io
import json
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_kanbannoti_2979d2"

os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

live = importlib.import_module("hermes_cli.kanban_notify_thread")  # noqa: E402
hermes_cli_pkg = importlib.import_module("hermes_cli")  # noqa: E402

WANTED = {"chat_threads", "unsubscribe_thread", "subscribe_thread"}


def _args(req):
    return types.SimpleNamespace(
        thread_id=req.get("thread_id"),
        platform=req.get("platform"),
        chat_id=req.get("chat_id"),
        task_id=req.get("task_id"),
        chat_type=req.get("chat_type"),
    )


def _session_of(req):
    session = req.get("session")
    return session if isinstance(session, dict) else {}


@contextlib.contextmanager
def _pinned(req):
    """Pin the notify rows and the session identity for one live call."""
    subs = req.get("subs")
    subs = subs if isinstance(subs, list) else []
    fake = types.ModuleType("hermes_cli.kanban_db_notify")
    fake.list_notify_subs = lambda conn, task_id: subs

    session = _session_of(req)

    def session_thread_for(platform, chat_id):
        if (str(session.get("platform", "")).lower() == str(platform).lower()
                and str(session.get("chat_id", "")) == str(chat_id)):
            return session.get("thread_id") or None
        return None

    previous_mod = sys.modules.get("hermes_cli.kanban_db_notify")
    previous_attr = getattr(hermes_cli_pkg, "kanban_db_notify", None)
    previous_fn = live._session_thread_for
    sys.modules["hermes_cli.kanban_db_notify"] = fake
    setattr(hermes_cli_pkg, "kanban_db_notify", fake)
    live._session_thread_for = session_thread_for
    try:
        yield
    finally:
        live._session_thread_for = previous_fn
        if previous_mod is None:
            sys.modules.pop("hermes_cli.kanban_db_notify", None)
        else:
            sys.modules["hermes_cli.kanban_db_notify"] = previous_mod
        if previous_attr is None:
            try:
                delattr(hermes_cli_pkg, "kanban_db_notify")
            except AttributeError:
                pass
        else:
            setattr(hermes_cli_pkg, "kanban_db_notify", previous_attr)


def chat_threads(req):
    with _pinned(req):
        return list(live._chat_threads(req.get("subs") or [], _args(req)))


def unsubscribe_thread(req):
    with _pinned(req):
        return live.resolve_unsubscribe_thread(None, _args(req))


def subscribe_thread(req):
    buf = io.StringIO()
    with _pinned(req):
        with contextlib.redirect_stderr(buf):
            thread_id = live.resolve_subscribe_thread(None, _args(req))
    notice = buf.getvalue().rstrip("\n")
    return {"thread_id": thread_id, "notice": notice or None}


def expected(r):
    f = r.get("func")
    if f == "chat_threads":
        return chat_threads(r)
    if f == "unsubscribe_thread":
        return unsubscribe_thread(r)
    if f == "subscribe_thread":
        return subscribe_thread(r)
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
