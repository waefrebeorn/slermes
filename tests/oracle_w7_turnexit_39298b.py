#!/usr/bin/env python3
"""Differential oracle for hermes_cli/turn_exit.py (w7_turnexit_39298b)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, pkg=False, **attrs):
    m = types.ModuleType(name)
    if pkg:
        m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_mod("hermes_cli", pkg=True)
_mod("hermes_cli.kanban_db",
     KANBAN_RATE_LIMIT_EXIT_CODE=75,
     KANBAN_TERMINAL_PROVIDER_EXIT_CODE=78)

SRC = Q + "/hermes_cli/turn_exit.py"
spec = importlib.util.spec_from_file_location("w7_turnexit_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w7_turnexit_live"] = live
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
        if func == "turn_exit_code":
            py = live.turn_exit_code(
                r.get("result"),
                kanban_worker=bool(r.get("kanban_worker", False)),
                credentials_rate_limited=bool(r.get("credentials_rate_limited", False)),
                credentials_terminal=bool(r.get("credentials_terminal", False)),
            )
        elif func == "reasons":
            attr = {"transient": "TRANSIENT_PROVIDER_REASONS",
                    "terminal": "TERMINAL_PROVIDER_REASONS"}.get(r.get("which"))
            if attr is None:
                continue
            py = sorted(getattr(live, attr))
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

print(f"ORACLE w7_turnexit_39298b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
