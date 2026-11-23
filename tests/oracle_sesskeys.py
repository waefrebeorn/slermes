#!/usr/bin/env python3
"""Oracle for persistence/core/keys.py (profile_from_session_key_namespace,
session_key_profile)."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/persistence/core/keys.py"
spec = importlib.util.spec_from_file_location("sesskeys_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

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
    if func == "ns":
        py = live.profile_from_session_key_namespace(r.get("value"))
    elif func == "profile":
        py = live.session_key_profile(r.get("value"))
    else:
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH sesskeys case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE sesskeys: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
