#!/usr/bin/env python3
"""Differential oracle for gate w11_statusrepo_0555a2 (hermes_cli/status_report.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "hermes_cli" not in sys.modules:
    _hp = types.ModuleType("hermes_cli")
    _hp.__path__ = [os.path.join(Q, "hermes_cli")]
    sys.modules["hermes_cli"] = _hp
NAME = "w11_statusrepo_0555a2_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "hermes_cli", "status_report.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "status_lines":
        fields = r.get("fields", {})
        keys = tuple(r.get("keys", []))
        return live.status_lines(fields, *keys)
    return None


n = m = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if not isinstance(r, dict) or "out" not in r:
        continue
    n += 1
    py, got = compute(r), r.get("out")
    if py != got:
        m += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w11_statusrepo_0555a2: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
