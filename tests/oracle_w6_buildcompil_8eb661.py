#!/usr/bin/env python3
"""Differential oracle for pm/build_compilers.py (gate w6_buildcompil_8eb661)."""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/pm/build_compilers.py"
NAME = "w6_bcomp_live"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def patch(platform, recorded, env, available):
    saved = (live.sys.platform, live._recorded_toolchain, live.shutil.which)
    live.sys.platform = platform
    live._recorded_toolchain = lambda py: recorded
    live.shutil.which = lambda cmd, path=None: ("/usr/bin/" + cmd) if cmd in available else None
    return saved


def restore(saved):
    live.sys.platform, live._recorded_toolchain, live.shutil.which = saved


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func, got = r.get("func"), r.get("out")
    saved = patch(r.get("platform", "linux"), r.get("recorded", {}),
                  r.get("env", {}), r.get("available", []))
    try:
        if func == "missing_compiler_remedy":
            want = live.missing_compiler_remedy(r.get("output", ""))
        elif func == "linux_defaults":
            want = live._linux_defaults(r.get("recorded", {}), r.get("env", {}))
        elif func == "pinned_build_defaults":
            want = live.pinned_build_defaults(Path("python"), r.get("env", {}))
        else:
            continue
    finally:
        restore(saved)
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_buildcompil_8eb661: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
