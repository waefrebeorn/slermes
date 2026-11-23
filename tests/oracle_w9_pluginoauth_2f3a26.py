#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/plugin_oauth.py
(w9_pluginoauth_2f3a26 gate): the live compiled regexes and _parent decide."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/hermes_cli/plugin_oauth.py"
spec = importlib.util.spec_from_file_location("pluginoauth_w9_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["pluginoauth_w9_live"] = live
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
        if func == "valid_id":
            py = bool(live._ID_RE.match(r.get("value", "")))
        elif func == "valid_env":
            py = bool(live._ENV_RE.match(r.get("value", "")))
        elif func == "valid_host":
            py = bool(live._HOST_RE.match(r.get("value", "")))
        elif func == "parent":
            py = live._parent(r.get("host", ""))
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

print(f"ORACLE w9_pluginoauth_2f3a26: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
