#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_build_limits.py (w15_webbuildl_3b4d8f).

_cgroup_memory_limit_mb and _available_cores are patched from each case's
`cgroup_mb` / `cores` fields so the system-dependent branches are deterministic
and identical to the injected C seams.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/hermes_cli/web_build_limits.py"
spec = importlib.util.spec_from_file_location("w15_webbuildl_3b4d8f_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
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
    if func not in ("light", "threads", "heap", "limits", "apply"):
        continue
    cases += 1
    live._cgroup_memory_limit_mb = lambda: r.get("cgroup_mb")
    live._available_cores = lambda: int(r.get("cores", 1))
    env = dict(r.get("env", {}) or {})
    try:
        if func == "light":
            py = live.light_build_requested(env)
        elif func == "threads":
            py = live._bounded_threads()
        elif func == "heap":
            py = live._bounded_heap_mb()
        elif func == "limits":
            py = live.web_build_limits(env)
        else:
            py = live.apply_web_build_limits(env)
    except Exception as exc:
        py = {"error": repr(exc)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print("ORACLE w15_webbuildl_3b4d8f: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
