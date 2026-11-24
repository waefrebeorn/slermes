#!/usr/bin/env python3
"""Oracle for gate w17_asyncdeleg_c94879 (tools/async_delegation_worker.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _load(dotted, rel):
    parts = dotted.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pkg = types.ModuleType(pn)
            pkg.__path__ = [QUARRY + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pkg
    spec = importlib.util.spec_from_file_location(dotted, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = m
    with contextlib.redirect_stderr(io.StringIO()):
        spec.loader.exec_module(m)
    return m

live = _load("tools.async_delegation_worker", "tools/async_delegation_worker.py")

class FakeStore:
    """Return the exact {'value': ...} shape the real durable store returns."""
    def __init__(self, scope):
        self.scope = scope
    def _apply(self, op, payload):
        return {"value": {"op": op, "payload": payload}}

led = live.WorkerDelegationLedger(FakeStore({"session_id": "S1", "pid": 1}))

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
        if func == "complete":
            py = led.complete(r.get("event"), r.get("result"), r.get("delivery", "pending"))
        elif func == "child":
            py = led.child(r.get("delegation_id", ""), r.get("entry"))
        elif func == "read":
            py = led.read(r.get("delegation_id", ""))
        elif func in ("claim", "ack", "defer", "release", "drop"):
            py = getattr(led, func)(r.get("delegation_id", ""), r.get("claim_id", ""))
        elif func == "unscheduled":
            py = led.unscheduled(r.get("delegation_id", ""))
        else:
            continue
    except Exception as exc:
        print(f"SKIP {func}: {exc}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w17_asyncdeleg_c94879: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
