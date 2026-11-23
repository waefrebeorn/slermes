#!/usr/bin/env python3
"""Differential oracle for hermes_cli/anon_sign_in_cli.py (gate w15_anonsigni_6f4a1e)."""
import importlib.util
import json
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_anonsigni_6f4a1e"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/hermes_cli/anon_sign_in_cli.py"
_spec = importlib.util.spec_from_file_location("anon_sign_in_cli_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["anon_sign_in_cli_live"] = live
_spec.loader.exec_module(live)


class _State:
    """Minimal stand-in: drain_sign_in_copy only reads these three attributes."""
    def __init__(self, d):
        self.terminal = bool(d.get("terminal", False))
        self.copy = str(d.get("copy", ""))
        self.copy_terminal = str(d.get("copy_terminal", self.copy))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") != "drain":
            continue
        cases += 1
        states = [_State(s) for s in (rec.get("states") or [])]
        py = live.drain_sign_in_copy(states, chat=bool(rec.get("chat", True)))
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0

if __name__ == "__main__":
    sys.exit(main())
