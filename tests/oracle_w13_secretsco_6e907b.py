#!/usr/bin/env python3
"""Differential oracle for hermes_cli/_secrets_common.py (gate w13_secretsco_6e907b).

Replays the LIVE module's pure helpers against the C port's output. All six
functions are import-light; only sys.path bootstrap is needed.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
SRC = "C:/Users/eman5/hermes-agent/hermes_cli/_secrets_common.py"
spec = importlib.util.spec_from_file_location("w13secrets_live", SRC)
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
    got = r.get("out")
    if func == "console_safe":
        want = live.console_safe(r.get("text", ""))
    elif func == "yn":
        want = live.yn(bool(r.get("b", False)))
    elif func == "section_cfg":
        want = live.section_cfg(r.get("cfg") or {}, r.get("section", ""))
    elif func == "cfg_str":
        want = live.cfg_str(r.get("cfg") or {}, r.get("key", ""))
    elif func == "arg":
        want = list(live.arg(r.get("name", ""), r.get("help", ""), **(r.get("kwargs") or {})))
    elif func == "flag":
        want = list(live.flag(r.get("name", ""), r.get("help", "")))
    else:
        continue
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_secretsco_6e907b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
