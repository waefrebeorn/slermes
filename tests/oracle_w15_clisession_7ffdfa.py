#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_session_dock.py (gate w15_clisession_7ffdfa)."""
import importlib.util
import json
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_clisession_7ffdfa"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/hermes_cli/cli_session_dock.py"
_spec = importlib.util.spec_from_file_location("cli_session_dock_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["cli_session_dock_live"] = live
_spec.loader.exec_module(live)


class _Item:
    pass


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
        if func not in ("countdown", "prompt_text"):
            continue
        cases += 1
        if func == "countdown":
            py = live.countdown(rec.get("seconds", 0))
        else:
            item = rec.get("item")
            if isinstance(item, dict):
                stand = _Item()
                stand.display_text = item.get("display_text")
                stand.text = item.get("text")
                item = stand
            py = live._prompt_text(item)
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
