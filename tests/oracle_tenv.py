#!/usr/bin/env python3
"""Differential oracle for upstream Hermes tui_gateway/_env.py."""
import importlib.util
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tui_gateway/_env.py"
spec = importlib.util.spec_from_file_location("tenv_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)

FUNCS = {"env_int": live.env_int, "env_float": live.env_float}
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
    f = FUNCS.get(func)
    if f is None:
        continue
    name = r.get("name", "")
    if name in os.environ:
        del os.environ[name]
    v = r.get("value")
    if isinstance(v, str):
        os.environ[name] = v
    default = int(r["default"]) if func == "env_int" else float(r["default"])
    py = f(name, default)
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE tenv: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
