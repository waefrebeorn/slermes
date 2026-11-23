#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_side.py (w9_gatewayside_3248c8 gate).

The store/connection/inbox are fixture seams; the live module's real start()
and its wait() closure run against a fake connection so no socket or event
loop is involved.
"""
import importlib.util
import json
import queue
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


cli = _mod("hermes_cli")
cli.__path__ = []
STORE = {"has_store": True, "final": False}


class FakeConn:
    closed = False
    epoch = 1

    def __init__(self, result):
        self.result = result

    def call_tagged(self, method, **kwargs):
        return self.result, self.epoch

    def side_waiter(self, task_id):
        return FakeInbox(CURRENT_PAYLOAD)

    def drop_side_waiter(self, task_id):
        pass


class FakeInbox:
    def __init__(self, payload):
        self.payload = payload
        self.calls = 0

    def get(self, timeout=None):
        self.calls += 1
        if self.calls == 1:
            return self.payload
        return {"text": ""}


_mod("hermes_cli.gateway_tools_notify",
     gateway_store=lambda c: types.SimpleNamespace(connection=FakeConn({"task_id": "t"}))
     if STORE["has_store"] else None)
_mod("hermes_cli.gateway_connection", DISCONNECTED="disconnected")

SRC = Q + "/hermes_cli/gateway_side.py"
spec = importlib.util.spec_from_file_location("w9gs_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w9gs_live"] = live
spec.loader.exec_module(live)

CURRENT_PAYLOAD = None
cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "is_side_event":
            py = r.get("name", "") in live.SIDE_EVENTS
        elif func == "connection_for":
            STORE["has_store"] = bool(r.get("has_store", False))
            STORE["final"] = bool(r.get("final", False))
            c = types.SimpleNamespace(_gateway_session_open=bool(r.get("session_open", False)))
            # Patch the fake store's connection.final for this case.
            original = sys.modules["hermes_cli.gateway_tools_notify"].gateway_store
            def store_for(cli, r=r):
                if not STORE["has_store"]:
                    return None
                conn = FakeConn({"task_id": "t"})
                conn.final = bool(r.get("final", False))
                return types.SimpleNamespace(connection=conn)
            sys.modules["hermes_cli.gateway_tools_notify"].gateway_store = store_for
            py = live.connection_for(c) is not None
            sys.modules["hermes_cli.gateway_tools_notify"].gateway_store = original
        elif func == "start":
            conn = FakeConn(r.get("result"))
            try:
                task_id, _wait = live.start(conn, "sid", r.get("verb", "bg"), "text")
                py = {"task_id": task_id}
            except RuntimeError as exc:
                py = {"error": str(exc)}
        elif func == "answer":
            CURRENT_PAYLOAD = r.get("payload")
            conn = FakeConn({"task_id": "t"})
            _task_id, wait = live.start(conn, "sid", "bg", "text")
            try:
                py = {"answer": wait()}
            except RuntimeError as exc:
                py = {"error": str(exc)}
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w9_gatewayside_3248c8: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
