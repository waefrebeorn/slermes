#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_session_foreign.py (gate w24_methodsses_ccfd92).

The live handlers are called with their three collaborators faked in exactly the
places the module resolves them: ``hermes_cli.foreign_sessions_browser`` (the
reader/preview/importer) comes from ``sys.modules`` at call time, and the
server-injected globals (``_ok``, ``_err``, ``_profile_db``, ``_persistence_for``,
``_response_profile_name``, ``_db_unavailable_error``) are installed on the live
module.  What is compared is the handler's own decision: the params defaults it
passes on, which function it picks, the JSON-RPC envelope and the error
mapping (-32602 + str(exc), -32000 + the fixed text, the no-store refusal).

Nothing touches a profile store, a folder or a socket.
"""
import contextlib
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_methodsses_ccfd92"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tui_gateway import methods_session_foreign as live  # noqa: E402

WANTED = {"dispatch_list", "dispatch_history", "methods", "constants"}


def ok_env(rid, result):
    return {"jsonrpc": "2.0", "id": rid, "result": result}


def err_env(rid, code, msg, data=None):
    return {"jsonrpc": "2.0", "id": rid,
            "error": {"code": code, "message": msg}}


class FakeError(Exception):
    pass


def behavior(r):
    b = r.get("behavior")
    return b if isinstance(b, dict) else {}


def raised(b):
    kind = b.get("raise") or ""
    if kind == "ValueError":
        return ValueError(b.get("message") or "")
    if kind == "OSError":
        return OSError(b.get("message") or "")
    return None


def dispatch_list(r):
    b = behavior(r)
    calls = []

    def list_foreign_sessions(source, offset, limit):
        calls.append([source, offset, limit])
        exc = raised(b)
        if exc is not None:
            raise exc
        return b.get("result")

    fake = types.ModuleType("hermes_cli.foreign_sessions_browser")
    fake.list_foreign_sessions = list_foreign_sessions
    saved_mod = sys.modules.get("hermes_cli.foreign_sessions_browser")
    saved = {k: getattr(live, k, None) for k in ("_ok", "_err")}
    live._ok = ok_env
    live._err = err_env
    sys.modules["hermes_cli.foreign_sessions_browser"] = fake
    try:
        resp = live._foreign_list(r.get("rid"), r.get("params"))
    finally:
        for k, v in saved.items():
            if v is None:
                try:
                    delattr(live, k)
                except AttributeError:
                    pass
            else:
                setattr(live, k, v)
        if saved_mod is None:
            sys.modules.pop("hermes_cli.foreign_sessions_browser", None)
        else:
            sys.modules["hermes_cli.foreign_sessions_browser"] = saved_mod
    return {"call": calls[0] if calls else None, "response": resp}


def dispatch_history(r):
    b = behavior(r)
    importing = bool(r.get("importing"))
    calls = []

    def record(name):
        def fn(session_id, db, *args, **kwargs):
            calls.append([name, session_id, args[0] if args else None])
            exc = raised(b)
            if exc is not None:
                raise exc
            return b.get("result")
        return fn

    @contextlib.contextmanager
    def profile_db(params=None, *, writer=False):
        yield None if b.get("db") is False else object()

    @contextlib.contextmanager
    def persistence_for(db):
        yield object()

    def db_unavailable_error(rid, *, code):
        return err_env(rid, code, "Session storage is unavailable")

    fake = types.ModuleType("hermes_cli.foreign_sessions_browser")
    fake.preview_foreign_session = record("preview")
    fake.import_browser_session = record("import")
    saved_mod = sys.modules.get("hermes_cli.foreign_sessions_browser")
    names = ("_ok", "_err", "_profile_db", "_persistence_for",
             "_response_profile_name", "_db_unavailable_error")
    saved = {k: getattr(live, k, None) for k in names}
    live._ok = ok_env
    live._err = err_env
    live._profile_db = profile_db
    live._persistence_for = persistence_for
    live._response_profile_name = lambda profile=None: b.get("profile_name") or ""
    live._db_unavailable_error = db_unavailable_error
    sys.modules["hermes_cli.foreign_sessions_browser"] = fake
    try:
        resp = live._foreign_history_request(r.get("rid"), r.get("params"), importing)
    finally:
        for k, v in saved.items():
            if v is None:
                try:
                    delattr(live, k)
                except AttributeError:
                    pass
            else:
                setattr(live, k, v)
        if saved_mod is None:
            sys.modules.pop("hermes_cli.foreign_sessions_browser", None)
        else:
            sys.modules["hermes_cli.foreign_sessions_browser"] = saved_mod
    return {"call": calls[0] if calls else None, "response": resp}


def methods(r):
    return sorted(live._registry.names())


def constants(r):
    return {
        "list_method": "session.foreign.list",
        "preview_method": "session.foreign.preview",
        "import_method": "session.foreign.import",
        "invalid_params_code": -32602,
        "internal_code": -32000,
        "list_os_error": "Could not read session folders on this backend",
        "history_os_error": "Could not read this session on the backend",
        "db_unavailable": "Session storage is unavailable",
    }


def expected(r):
    f = r.get("func")
    if f == "dispatch_list":
        return dispatch_list(r)
    if f == "dispatch_history":
        return dispatch_history(r)
    if f == "methods":
        return methods(r)
    if f == "constants":
        return constants(r)
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
