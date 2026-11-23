#!/usr/bin/env python3
"""Differential oracle for agent/skills_index_delta.py (gate w10_skillsinde_bc8d57).

Imports the LIVE module with a stubbed session_rules.turns parent package;
compares the harness "out" to the pure index-row/note codec results.
Set-derived results are emitted sorted so the comparison is order-independent.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return m

# Parent-package bootstrap for `from session_rules.turns import flatten_message_text`.
sr = _pkg("session_rules")
turns = types.ModuleType("session_rules.turns")
turns.flatten_message_text = lambda msg: ""
sr.turns = turns
sys.modules["session_rules.turns"] = turns

SRC = QUARRY + "/agent/skills_index_delta.py"
spec = importlib.util.spec_from_file_location("w10_skillsinde_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w10_skillsinde_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "row_name":
        return live._row_name(r.get("line", ""))
    if func == "index_entries":
        return live.index_entries(r.get("prompt"))
    if func == "render":
        return live._render(r.get("added") or {}, set(r.get("removed") or []))
    if func == "parse":
        added, removed = live._parse(r.get("note", ""))
        return {"added": sorted(added), "removed": sorted(removed)}
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
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w10_skillsinde_bc8d57: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w10_skillsinde_bc8d57: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
