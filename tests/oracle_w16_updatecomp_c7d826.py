#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_completion.py (w16_updatecomp_c7d826).

File/print side effects are replaced with captured values: _write_json is patched
to hand back the dict it would persist, and builtins.print is captured for the
desktop-build report.
"""
import builtins
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/hermes_cli/update_completion.py"
spec = importlib.util.spec_from_file_location("w16_updatecomp_c7d826_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

FUNCS = ("exit_status", "owed", "write_result", "report_owed")

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
    if func not in FUNCS:
        continue
    cases += 1
    try:
        if func == "exit_status":
            py = live._exit_status(int(r.get("code", 0)))
        elif func == "owed":
            py = live._owed_user_action(r)
        elif func == "write_result":
            captured = {}
            old = live._write_json
            live._write_json = lambda path, data: captured.update(data)
            try:
                live._write_bootstrap_result(r, "/fake/result.json", int(r.get("code", 0)),
                                             r.get("result_receipt"))
            finally:
                live._write_json = old
            py = captured
        else:
            lines = []
            old = builtins.print
            builtins.print = lambda *a, **k: lines.append(" ".join(str(x) for x in a))
            try:
                live._report_desktop_build_owed(r, r.get("record"), r.get("reason", ""))
            finally:
                builtins.print = old
            py = lines
    except Exception as exc:
        py = {"error": repr(exc)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print("ORACLE w16_updatecomp_c7d826: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
