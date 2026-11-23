#!/usr/bin/env python3
"""Differential oracle for gateway/restart_watcher_env.py (gate w14_restartwat_1fbad5).

The checkout root and detached-restart log path are injected by patching the
LIVE module's filesystem readers; the LIVE shell-line builder and PYTHONPATH
rewrite then run unchanged.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
SRC = "C:/Users/eman5/hermes-agent/gateway/restart_watcher_env.py"
spec = importlib.util.spec_from_file_location("w14_rwe_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)


def want_for(r):
    func = r.get("func")
    if func == "detached_restart_shell":
        lp = r.get("log_path")
        live._detached_restart_log_path = lambda: (Path(lp) if lp is not None else None)
        return live.detached_restart_shell(r.get("cmd", ""), int(r.get("pid", 0)),
                                           r.get("after", 0.0))
    if func == "keep_importable":
        live._running_checkout_root = lambda: r.get("root")
        env = {"PYTHONPATH": r.get("pythonpath", "")}
        return live.keep_running_checkout_importable(env).get("PYTHONPATH", "")
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    want = want_for(r)
    if want is None:
        continue
    got = r.get("out")
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w14_restartwat_1fbad5: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
