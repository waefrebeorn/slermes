#!/usr/bin/env python3
"""Oracle for upstream Hermes persistence/core/health.py (StorageState, StorageProbe)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

ROOT = "C:/Users/eman5/hermes-agent"
SRC = ROOT + "/persistence/core/health.py"
DOTTED = "persistence.core.health"
sys.path.insert(0, ROOT)
spec = importlib.util.spec_from_file_location(DOTTED, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[DOTTED] = live
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
    if func == "state_value":
        try:
            py = live.StorageState[r["name"]].value
        except Exception:
            py = None
    elif func == "state_from_value":
        try:
            py = live.StorageState(r["value"]).name
        except Exception:
            py = None
    elif func == "state_str":
        try:
            py = str(live.StorageState[r["name"]])
        except Exception:
            py = None
    elif func == "state_count":
        py = len(live.StorageState)
    elif func == "make_probe":
        detail = r.get("detail")
        probe = live.StorageProbe(r.get("status"), None if detail is None else detail)
        py = {"status": probe.status, "detail": probe.detail}
    else:
        continue

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE corehealth: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
