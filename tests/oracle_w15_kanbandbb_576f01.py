#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/kanban_db_boards.py (gate w15_kanbandbb_576f01).

The late-bound origin namespace (hermes_cli.kanban_db) is stubbed: the ported
_default_board_display_name is pure and does not use it.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


hcli = _mod("hermes_cli")
hcli.__path__ = [QUARRY + "/hermes_cli"]
kb = _mod("hermes_cli.kanban_db", DEFAULT_BOARD="default")
hcli.kanban_db = kb

SRC = QUARRY + "/hermes_cli/kanban_db_boards.py"
spec = importlib.util.spec_from_file_location("w15_kanbandbb_576f01_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w15_kanbandbb_576f01_live"] = live
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
        if func == "display_name":
            py = live._default_board_display_name(r.get("slug", ""))
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

print(f"ORACLE w15_kanbandbb_576f01: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
