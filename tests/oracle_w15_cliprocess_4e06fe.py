#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_process_dock.py (gate w15_cliprocess_4e06fe)."""
import importlib.util
import json
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_cliprocess_4e06fe"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/hermes_cli/cli_process_dock.py"
_spec = importlib.util.spec_from_file_location("cli_process_dock_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["cli_process_dock_live"] = live
_spec.loader.exec_module(live)

DISPATCH = {
    "last_output_line": lambda r: live._last_output_line(r.get("buffer") or ""),
    "status": lambda r: live.process_status(r.get("entry") or {}),
    "glyph": lambda r: live.process_glyph(r.get("row") or {}),
}


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
        func = rec.get("func")
        if func not in DISPATCH:
            continue
        cases += 1
        try:
            py = DISPATCH[func](rec)
        except Exception as exc:
            py = f"__EXC__{type(exc).__name__}: {exc}"
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
