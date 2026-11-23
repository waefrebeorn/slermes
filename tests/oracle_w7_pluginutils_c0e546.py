#!/usr/bin/env python3
"""Differential oracle for plugins/plugin_utils.py (w7_pluginutils_c0e546)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/plugins/plugin_utils.py"
spec = importlib.util.spec_from_file_location("w7_pluginutils_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w7_pluginutils_live"] = live
spec.loader.exec_module(live)

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
        if func == "all":
            py = live.__all__
        elif func == "singleton_slot_ops":
            slot = live.SingletonSlot()
            values = r.get("factory_values", [])
            calls = [0]

            def factory():
                i = calls[0]
                calls[0] += 1
                return values[i] if i < len(values) else None

            py = []
            for op in r.get("ops", []):
                if op == "get":
                    py.append(slot.get(factory))
                elif op == "peek":
                    py.append(slot.peek())
                elif op == "reset":
                    slot.reset()
                    py.append(None)
                else:
                    py.append(None)
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

print(f"ORACLE w7_pluginutils_c0e546: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
