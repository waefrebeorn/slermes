#!/usr/bin/env python3
"""Oracle for upstream Hermes persistence/sqlite/_dashboard_procs.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/persistence/sqlite/_dashboard_procs.py"
spec = importlib.util.spec_from_file_location("dashproc_live", SRC)
live = importlib.util.module_from_spec(spec)
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
    if func == "index":
        py = live._dashboard_subcommand_index(r["argv"])
    elif func == "ephemeral":
        py = live._is_ephemeral_port_zero_backend(r["argv"])
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE dashproc: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
