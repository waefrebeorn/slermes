#!/usr/bin/env python3
"""Differential oracle for agent/agent_runtime_helpers_dead_connections.py
(gate w25_agentrunti_e1ad40).

Two seams are stood in for, never re-implemented:

  * the socket: a fake object classifies each case the way a POSIX non-blocking
    ``recv(MSG_PEEK | MSG_DONTWAIT)`` would (EOF, data, BlockingIOError,
    OSError, or a setblocking failure), and ``socket`` is swapped in
    ``sys.modules`` for the duration of the probe because this host's ``socket``
    module has no ``MSG_DONTWAIT`` (the code targets POSIX gateways);
  * the pools: ``_count_dead_pool_sockets`` is replaced by the plan's per-pool
    counts (or raises, for the error fan), and the agent is a fake whose two
    teardown hooks record what they were called with.

The live ``cleanup_dead_connections`` then runs unchanged, and what is compared
is its return value plus every log record it emits (level + rendered message)
and every teardown it invoked.
"""
import ast
import importlib.util
import json
import logging
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_agentrunti_e1ad40"
LIVE_PATH = QUARRY / "agent" / "agent_runtime_helpers_dead_connections.py"


def load_live():
    spec = importlib.util.spec_from_file_location("_live_dead_connections", LIVE_PATH)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["_live_dead_connections"] = mod
    spec.loader.exec_module(mod)
    return mod


live = load_live()

WANTED = {"socket_probe", "cleanup", "constants"}
PROBE_KINDS = {"eof", "data", "blocking", "oserror", "setblocking_error"}


class _FakeSock:
    def __init__(self, kind):
        self.kind = kind
        self.calls = []

    def setblocking(self, flag):
        self.calls.append(("setblocking", flag))
        if self.kind == "setblocking_error":
            raise OSError("socket already closed")

    def recv(self, _size, _flags):
        if self.kind == "eof":
            return b""
        if self.kind == "data":
            return b"x"
        if self.kind == "blocking":
            raise BlockingIOError()
        raise OSError("connection reset by peer")


def socket_probe(req):
    kind = req.get("kind")
    if kind not in PROBE_KINDS:
        return None
    real = sys.modules.get("socket")
    fake = types.ModuleType("socket")
    fake.MSG_PEEK = 2
    fake.MSG_DONTWAIT = 64
    sys.modules["socket"] = fake
    try:
        return live._socket_is_dead(_FakeSock(kind))
    finally:
        if real is not None:
            sys.modules["socket"] = real
        else:
            sys.modules.pop("socket", None)


class _Capture(logging.Handler):
    def __init__(self, records):
        super().__init__()
        self.records = records

    def emit(self, record):
        self.records.append([record.levelname, record.getMessage()])


class _Agent:
    def __init__(self, actions, fail):
        self.client = None
        self._request_client_cache = {}
        self.actions = actions
        self.fail = fail

    def _replace_primary_openai_client(self, reason=None):
        # The hook is invoked either way; only a completed teardown is recorded,
        # which is what the port's "actions" list means.
        if "primary" in self.fail:
            raise RuntimeError("rebuild refused")
        self.actions.append("primary:_replace_primary_openai_client:%s" % reason)

    def _close_cached_request_openai_client(self, reason=None):
        if "request" in self.fail:
            raise RuntimeError("cache hook refused")
        self.actions.append("request:_close_cached_request_openai_client:%s" % reason)


def cleanup(req):
    plan = req.get("plan")
    if not isinstance(plan, dict):
        return None
    dead = plan.get("dead")
    if dead is not None and not isinstance(dead, dict):
        return None
    dead = dead or {}
    probe_error = plan.get("probe_error") or []
    cleanup_error = plan.get("cleanup_error") or []

    agent = _Agent([], cleanup_error)
    if plan.get("primary"):
        agent.client = object()
    if plan.get("request_client"):
        agent._request_client_cache["client"] = agent.client if plan.get("same") else object()

    def count_dead(client):
        label = "primary" if client is agent.client else "request"
        if label in probe_error:
            raise ValueError("private httpcore internals moved")
        return dead.get(label, 0)

    real_count = live._count_dead_pool_sockets
    live._count_dead_pool_sockets = count_dead
    logger = logging.getLogger("run_agent")
    records = []
    handler = _Capture(records)
    old_level, old_propagate = logger.level, logger.propagate
    logger.addHandler(handler)
    logger.setLevel(logging.DEBUG)
    logger.propagate = False
    try:
        cleaned = live.cleanup_dead_connections(agent)
    finally:
        logger.removeHandler(handler)
        logger.setLevel(old_level)
        logger.propagate = old_propagate
        live._count_dead_pool_sockets = real_count
    return {"cleaned": cleaned, "actions": agent.actions, "logs": records}


def constants():
    tree = ast.parse(LIVE_PATH.read_text(encoding="utf-8"))
    reasons = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Call):
            for kw in node.keywords:
                if kw.arg == "reason" and isinstance(kw.value, ast.Constant):
                    reasons.add(kw.value.value)
    assert len(reasons) == 1, reasons
    return {"logger": live.logger.name, "cleanup_reason": reasons.pop()}


def expected(req):
    f = req.get("func")
    if f == "socket_probe":
        return socket_probe(req)
    if f == "cleanup":
        return cleanup(req)
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
    if cases == 0:
        print("ORACLE %s: no cases consumed" % GATE, file=sys.stderr)
        sys.exit(1)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
