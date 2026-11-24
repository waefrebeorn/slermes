#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_cmd_commit.py (w16_updatecmd_383a4c).

Replays the LIVE debt_sha_for_move / _table / startup_files functions.  For
debt_sha_for_move the module's _run_start global is set from the fixture (the
head this run started from), exactly as record_run_start would.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/hermes_cli/update_cmd_commit.py"
spec = importlib.util.spec_from_file_location("w16_updatecmd_383a4c_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w16_updatecmd_383a4c_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "debt":
        head = r.get("run_start_head") or ""
        live._run_start = (["git"], (head, "main")) if head else None
        return live.debt_sha_for_move(r.get("pre", ""), r.get("target", ""))
    if func == "table":
        return live._table(r.get("obj", {}), *r.get("keys", []))
    if func == "startup":
        selection = live.startup_files(r.get("pyproject"), r.get("undeclared", []))
        if selection is None:
            return None
        files, required = selection
        return {"files": list(files), "required": required}
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w16_updatecmd_383a4c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
