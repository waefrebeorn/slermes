#!/usr/bin/env python3
"""Oracle for upstream Hermes pm/filesystem.py (w5_filesystem_427a52 gate).

Compares the pure path-spelling primitives native() and _long_form() against
the live stdlib-only module. Runs on Windows (os.name == "nt"), so both
verbatim-prefix branches are exercised.
"""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

spec = importlib.util.spec_from_file_location("w5_filesystem_live", Q + "/pm/filesystem.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w5_filesystem_live"] = live
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
        if func == "native":
            py = live.native(r.get("text", ""))
        elif func == "long_form":
            py = live._long_form(r.get("text", ""))
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

print(f"ORACLE w5_filesystem_427a52: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
