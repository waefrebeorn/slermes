#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/home_data_layout.py (gate w15_homedatal_47b777)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
# Parent-package bootstrap so late absolute imports resolve.
if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg

SRC = QUARRY + "/hermes_cli/home_data_layout.py"
spec = importlib.util.spec_from_file_location("w15_homedatal_47b777_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w15_homedatal_47b777_live"] = live
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
        if func == "profile_root":
            py = live.profile_root_entry(tuple(r.get("parts", [])))
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

print(f"ORACLE w15_homedatal_47b777: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
