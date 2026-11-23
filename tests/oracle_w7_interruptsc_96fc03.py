#!/usr/bin/env python3
"""Differential oracle for w7_interruptsc_96fc03 -> agent/interrupt_scope.py.

The live module is imported with spec_from_file_location; agent.interrupt_compat
is a stubbed parent module whose request_hard_interrupt records the interrupt
calls. The C harness replays the same ops script against its latch/registry
state machine; outputs must match.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_pkg("agent")
REC = []
_mod("agent.interrupt_compat", request_hard_interrupt=lambda a, r, tool_reason=None: REC.append(
    {"agent": a, "reason": r, "tool_reason": tool_reason}))

SRC = Q + "/agent/interrupt_scope.py"
spec = importlib.util.spec_from_file_location("interruptsc_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["interruptsc_live"] = live
spec.loader.exec_module(live)


def canonical(x):
    if x is None or isinstance(x, (bool, str, int)):
        return x
    if isinstance(x, float):
        return int(x) if x.is_integer() else x
    if isinstance(x, dict):
        return {k: canonical(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [canonical(v) for v in x]
    return x


def replay(ops):
    REC.clear()
    scope = live.InterruptScope()
    cms = []  # keep contextmanagers alive so their finally blocks do not untrack agents
    for op in ops or []:
        name = op.get("op")
        if name == "track":
            cm = scope.track(op.get("agent", ""))
            cm.__enter__()
            cms.append(cm)
        elif name == "track_in":
            if op.get("bound") is True:
                cm = scope.track(op.get("agent", ""))
                cm.__enter__()
                cms.append(cm)
        elif name == "cancel":
            kw = {}
            if "tool_reason" in op:
                kw["tool_reason"] = op["tool_reason"]
            scope.cancel(op.get("reason", ""), **kw)
    return {"reason": scope.reason, "tool_reason": scope._tool_reason,
            "interrupts": [canonical(x) for x in REC]}


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
        if func == "default_reason":
            py = live._TOOL_REASON_HOST_CANCELLED
        elif func == "replay":
            py = replay(r.get("ops"))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = canonical(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w7_interruptsc_96fc03: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
