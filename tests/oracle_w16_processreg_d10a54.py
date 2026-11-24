#!/usr/bin/env python3
"""Differential oracle for gate w16_processreg_d10a54 (tools/process_registry_stdin.py)."""
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

NAME = "w16prs_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/tools/process_registry_stdin.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

G = "w16_processreg_d10a54"
DATA = {"pty_available": False}


def compute(r):
    inp = r.get("in") or {}
    func = r.get("func")
    DATA["pty_available"] = bool(inp.get("pty_available"))
    live.importlib = types.SimpleNamespace(
        util=types.SimpleNamespace(
            find_spec=lambda n: (object() if DATA["pty_available"] else None)))
    proc = None
    if inp.get("process_exists"):
        proc = types.SimpleNamespace(stdin=("pipe" if inp.get("stdin_open") else None))
    sess = types.SimpleNamespace(process=proc,
                                 pty_fallback=inp.get("pty_fallback"),
                                 output_log=inp.get("output_log"))
    if func == "no_pty_reason":
        return live._no_pty_reason(sess)
    if func == "already_exited_error":
        return live.already_exited_error(sess)
    if func == "stdin_unavailable_error":
        return live.stdin_unavailable_error(sess)
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
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print(f"ORACLE {G}: live raised {exc!r} case={r.get('case')}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {G} {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
