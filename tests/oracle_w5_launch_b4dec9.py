#!/usr/bin/env python3
"""Differential oracle for upstream Hermes pm/launch.py (gate w5_launch_b4dec9).

launch.py's three side effects (truststore injection, runtime lease, CLI
delegation) are stubbed and recorded as events; the module-level sys.path
insert is compared against pathlib under Windows path semantics.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

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


events = []
RC = {"value": 0}
_mod("truststore", inject_into_ssl=lambda: events.append("inject"))
pm = _mod("pm")
pm.__path__ = []
_mod("pm.cli", main=lambda: (events.append("cli"), RC["value"])[1])
_mod("pm.runtime", lease_current_runtime=lambda: events.append("lease"))

SRC = Q + "/pm/launch.py"
spec = importlib.util.spec_from_file_location("w5_launch_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w5_launch_live"] = live
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
        if func == "path_insert":
            py = str(Path(r.get("file", "")).resolve().parents[1])
        elif func == "main":
            RC["value"] = int(r.get("cli_rc", 0))
            events.clear()
            rc = live.main()
            py = {"events": list(events), "rc": rc}
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

print(f"ORACLE w5_launch_b4dec9: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
