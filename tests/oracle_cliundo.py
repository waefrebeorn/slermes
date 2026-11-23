#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_undo_hint.py (cliundo gate).

_user_turn_indices is injected per case (the real predicate lives in
session_rules.turns and is not the unit under test here), mirroring the C port.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


hermes_cli = _mod("hermes_cli")
hermes_cli.__path__ = []
INDICES = [[]]
_mod("hermes_cli.cli_session_mixin", _user_turn_indices=lambda history: INDICES[0])

SRC = Q + "/hermes_cli/cli_undo_hint.py"
spec = importlib.util.spec_from_file_location("cliundo_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["cliundo_live"] = live
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
    if r.get("func") != "undone_span":
        continue
    INDICES[0] = r.get("indices", [])
    before = r.get("before", [])
    res = live._undone_span(before, int(r.get("turns_undone", 0)))
    py = [res[0], res[1]]
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH undone_span args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE cliundo: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
