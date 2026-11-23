#!/usr/bin/env python3
"""Differential oracle for tools/approval_pipe_launchers.py (gate apppipe)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tools/approval_pipe_launchers.py"
spec = importlib.util.spec_from_file_location("apppipe_live", SRC)
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
    if func == "launcher_re":
        name = r.get("name", "")
        short = r.get("short", "")
        long_opts = tuple(r.get("long_opts") or [])
        ws = r.get("ws")
        want = live._pipe_launcher_re(name, short, long_opts) if ws is None \
            else live._pipe_launcher_re(name, short, long_opts, ws)
    elif func == "path_prefix":
        want = live._PIPE_PATH_PREFIX_RE
    elif func == "wrappers":
        want = live._PIPE_WRAPPERS_RE
    else:
        continue
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE apppipe: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
