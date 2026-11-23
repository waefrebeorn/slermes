#!/usr/bin/env python3
"""Differential oracle for gateway/systemd_stop_mark.py (w12_systemdsto_89d0b9).

The MAINPID env is injected through the fixture and installed into os.environ
around the LIVE call; write_planned_stop_marker is a recording stub so main()
runs its real resolve+try/except path without side effects.
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

gw = types.ModuleType("gateway")
gw.__path__ = []
sys.modules["gateway"] = gw
status = types.ModuleType("gateway.status")
_MARKED = []
status.write_planned_stop_marker = lambda pid: _MARKED.append(pid)
sys.modules["gateway.status"] = status

SRC = Q + "/gateway/systemd_stop_mark.py"
spec = importlib.util.spec_from_file_location("systemd_stop_mark_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["systemd_stop_mark_live"] = live
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
    argv = r.get("argv", [])
    mp = r.get("mainpid", "")
    if mp is None:
        mp = ""
    mp = str(mp)
    try:
        if func == "resolve":
            orig = os.environ.get("MAINPID")
            os.environ["MAINPID"] = mp
            try:
                py = live._resolve_target_pid(list(argv))
            finally:
                if orig is None:
                    os.environ.pop("MAINPID", None)
                else:
                    os.environ["MAINPID"] = orig
            if py is None:
                py = None
        elif func == "main":
            orig = os.environ.get("MAINPID")
            os.environ["MAINPID"] = mp
            try:
                _MARKED.clear()
                py = live.main(list(argv))
            finally:
                if orig is None:
                    os.environ.pop("MAINPID", None)
                else:
                    os.environ["MAINPID"] = orig
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

print(f"ORACLE w12_systemdsto_89d0b9: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
