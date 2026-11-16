import os
#!/usr/bin/env python3
"""sta_oracle_status.py -- diffs the C gateway/status.py port against the real source.

Reads the harness JSONL on stdin. The Python reference is extracted with ast
(the module imports heavy gateway machinery that is not importable here).
"""
import ast
import json
import pathlib
import re
import sys
import types

SRC = "C:/Users/eman5/hermes-agent/gateway/status.py"
with open(SRC, encoding="utf-8") as f:
    text = f.read()
tree = ast.parse(text)

WANTED = {
    "hermes_home_assignments",
    "command_line_names_hermes_home",
    "_profile_name_for_home",
}

ns = {"re": re, "Optional": object, "Any": object,
      "__name__": "oracle_status_extracted"}

picked = set()
for node in tree.body:
    if isinstance(node, ast.FunctionDef) and node.name in WANTED:
        exec(compile(ast.get_source_segment(text, node), SRC, "exec"), ns)
        picked.add(node.name)
    elif isinstance(node, ast.Assign):
        names = {t.id for t in node.targets if isinstance(t, ast.Name)}
        if names & {"_HERMES_HOME_ASSIGNMENT_RE"}:
            exec(compile(ast.get_source_segment(text, node), SRC, "exec"), ns)
            picked |= (names & {"_HERMES_HOME_ASSIGNMENT_RE"})

missing = (WANTED | {"_HERMES_HOME_ASSIGNMENT_RE"}) - picked
if missing:
    print(f"ORACLE status: FATAL, source not found: {sorted(missing)}")
    raise SystemExit(1)

cases = mismatches = shown = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        c = json.loads(line)
    except json.JSONDecodeError:
        continue
    fn, arg = c["fn"], c.get("arg", "")
    cases += 1

    if fn == "assign":
        want = ns["hermes_home_assignments"](arg)
    elif fn == "names":
        want = bool(ns["command_line_names_hermes_home"](
            c.get("cmd", arg), c.get("home", "")))
    elif fn == "profile":
        # the Python helper takes a pathlib.Path, not a str
        want = ns["_profile_name_for_home"](pathlib.Path(arg))
    else:
        continue

    got = c["out"]
    if isinstance(got, str) and got != "MISSING":
        try:
            got = json.loads(got)
        except json.JSONDecodeError:
            pass
    if isinstance(want, bool):
        got = bool(got)
    if got != want:
        mismatches += 1
        if shown < 12:
            print(f"MISMATCH {fn} cmd={c.get('cmd', arg)!r} "
                  f"home={c.get('home')!r}: got {got!r} want {want!r}")
            shown += 1

print(f"ORACLE status: {cases} cases, {mismatches} mismatches")
raise SystemExit(1 if mismatches else 0)
