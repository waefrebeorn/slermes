#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/goal_command.py
(w9_goalcommand_7b6c68 gate). The live is_goal_control body does the real work."""
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


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


hcli = _mod("hermes_cli")
hcli.__path__ = []
goals = _mod("hermes_cli.goals")
hcli.goals = goals

SRC = Q + "/hermes_cli/goal_command.py"
spec = importlib.util.spec_from_file_location("goalcmd_w9_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["goalcmd_w9_live"] = live
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
    try:
        if func == "ctrl":
            py = bool(live.is_goal_control(r.get("arg", "")))
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

print(f"ORACLE w9_goalcommand_7b6c68: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
