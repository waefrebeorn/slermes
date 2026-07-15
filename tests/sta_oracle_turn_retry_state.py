"""Oracle for agent/turn_retry_state.py port.

Loads the LIVE upstream module and replays the fixture ops against a real
TurnRetryState instance, serializing (set -> null, get -> bool, iter ->
[[name,bool],...]) to match the C harness for byte-diffing.

argv[1] = fixture: {"ops":[...]}.
"""
import sys, os, json
from pathlib import Path

ROOT = Path("/home/wubu/hermes-agent-dev")
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

import importlib.util
spec = importlib.util.spec_from_file_location(
    "agent.turn_retry_state", str(ROOT / "agent" / "turn_retry_state.py"))
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
sys.modules["agent.turn_retry_state"] = mod
spec.loader.exec_module(mod)


def main():
    if len(sys.argv) < 2:
        return 1
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        data = json.load(f)

    s = mod.TurnRetryState()
    results = []
    for o in data.get("ops", []):
        op = o["op"]
        field = o.get("field")
        if op == "set":
            setattr(s, field, bool(o.get("val")))
            results.append(None)
        elif op == "get":
            results.append(bool(getattr(s, field)))
        elif op == "iter":
            results.append([[name, bool(val)] for name, val in s])
    sys.stdout.write(json.dumps(results, ensure_ascii=False, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
