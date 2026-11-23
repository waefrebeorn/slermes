#!/usr/bin/env python3
"""Oracle for upstream Hermes agent/nous_wire.py (gate w5_nouswire_774ca4)."""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def _pkg(name, path):
    m = types.ModuleType(name)
    m.__path__ = [path]
    sys.modules[name] = m


_pkg("agent", QUARRY + "/agent")
_pkg("hermes_cli", QUARRY + "/hermes_cli")

_MODE = {"v": "auto"}
prov = types.ModuleType("hermes_cli.providers")
prov._nous_anthropic_wire = lambda: _MODE["v"]
sys.modules["hermes_cli.providers"] = prov
sys.modules["hermes_cli"].providers = prov

spec = importlib.util.spec_from_file_location("agent.nous_wire", QUARRY + "/agent/nous_wire.py")
live = importlib.util.module_from_spec(spec)
sys.modules["agent.nous_wire"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func == "classify":
        resp = SimpleNamespace(provider=r.get("provider"), id=r.get("rid"))
        py = live.classify_upstream(resp)
    elif func == "for_upstream":
        py = live.wire_for_upstream(r.get("upstream"))
    elif func == "maybe":
        f = r["facts"]
        _MODE["v"] = f.get("nous_wire_mode", "auto")
        agent = SimpleNamespace(
            provider=f.get("provider", ""),
            model=f.get("model", ""),
            api_mode=f.get("api_mode"),
            _nous_wire_decided=f.get("decided", False),
        )
        resp = SimpleNamespace(provider=f.get("response_provider"), id=f.get("response_id"))
        switched = live.maybe_switch_wire_after_first_response(
            agent, resp, f.get("api_call_count", 1))
        pending = getattr(agent, "_nous_wire_pending", None)
        py = {
            "switched": switched,
            "decided": getattr(agent, "_nous_wire_decided", False),
            "pending": list(pending) if pending else None,
        }
    else:
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w5_nouswire_774ca4: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
