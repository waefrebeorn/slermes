#!/usr/bin/env python3
"""Differential oracle for gate w17_gatewayaut_143263 (hermes_cli/gateway_automation.py).

Live imports are stubbed with deterministic seams: GoalGate.from_dict and
GatewaySessionStore are injected so the __gate__ branch and the connection
decision can be compared without a running gateway.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if not (Path(QUARRY) / "hermes_cli" / "gateway_automation.py").is_file():
    QUARRY = str(Path(__file__).resolve().parent.parent.parent / "hermes-agent")
sys.path.insert(0, QUARRY)
_pkg = types.ModuleType("hermes_cli")
_pkg.__path__ = [str(Path(QUARRY) / "hermes_cli")]
sys.modules["hermes_cli"] = _pkg

class _GatewayClientError(Exception):
    pass

_gc = types.ModuleType("hermes_cli.gateway_client")
_gc.GatewayClientError = _GatewayClientError
sys.modules["hermes_cli.gateway_client"] = _gc

class _GoalGate:
    @staticmethod
    def from_dict(d):
        return {"__from_gate__": d}

_goals = types.ModuleType("hermes_cli.goals")
_goals.GoalGate = _GoalGate
_goals.GoalManager = type("_GoalManager", (), {
    n: (lambda self, *a, **k: None) for n in (
        "is_active", "has_goal", "has_contract", "status_line", "render_subgoals",
        "render_gates", "render_contract", "next_continuation_prompt")})
sys.modules["hermes_cli.goals"] = _goals

_loops = types.ModuleType("hermes_cli.loops")
_loops.LoopManager = type("_LoopManager", (), {
    n: (lambda self, *a, **k: None) for n in ("is_active", "has_loop", "status_line", "is_due")})
sys.modules["hermes_cli.loops"] = _loops

_hb = types.ModuleType("hermes_cli.heartbeat")
_hb.HeartbeatManager = type("_HeartbeatManager", (), {
    n: (lambda self, *a, **k: None) for n in ("has_heartbeat", "is_active", "status_line")})
sys.modules["hermes_cli.heartbeat"] = _hb

class _GatewaySessionStore:
    def __init__(self, connection):
        self.connection = connection

_gss = types.ModuleType("hermes_cli.gateway_session_store")
_gss.GatewaySessionStore = _GatewaySessionStore
sys.modules["hermes_cli.gateway_session_store"] = _gss

_SPEC = importlib.util.spec_from_file_location(
    "w17_gatewayaut_live", str(Path(QUARRY) / "hermes_cli" / "gateway_automation.py"))
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, list): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


class _Cli:
    pass


def expected(rec):
    f = rec.get("func")
    if f == "decode":
        return mod._decode(rec.get("value"))
    if f == "gateway_connection":
        cli = _Cli()
        cli._session_db = _GatewaySessionStore(rec.get("connection")) if rec.get("is_store") else object()
        cli._gateway_session_open = bool(rec.get("open"))
        return mod.gateway_connection(cli)
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        got = rec.get("out")
        want = expected(rec)
        if _canon(got) != _canon(want):
            mism += 1
            if mism <= 10:
                print("MISMATCH %s\n  got : %s\n  want: %s" %
                      (rec.get("func"), json.dumps(got, ensure_ascii=False),
                       json.dumps(want, ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w17_gatewayaut_143263: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
