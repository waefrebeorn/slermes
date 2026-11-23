#!/usr/bin/env python3
"""Oracle for upstream Hermes pm/operations.py (w5_operations_2c07e3 gate).

Compares _environment_root's regex validation and root absolute-path spelling.
Fixtures always supply a root, so the get_hermes_home() branch is never hit.
"""
import importlib.util, json, sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

spec = importlib.util.spec_from_file_location("w5_operations_live", Q + "/pm/operations.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w5_operations_live"] = live
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
    if func != "environment_root":
        continue
    try:
        root = Path(r.get("root", ""))
        # Canonicalise separators: pathlib's reported style varies by Python
        # version; the C port reports '/' and we compare in that form.
        py = {"path": str(live._environment_root(r.get("name", ""), root)).replace("\\", "/")}
    except ValueError as exc:
        py = {"error": str(exc)}
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w5_operations_2c07e3: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
