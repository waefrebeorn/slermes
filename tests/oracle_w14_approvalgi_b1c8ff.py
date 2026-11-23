#!/usr/bin/env python3
"""Differential oracle for tools/approval_git_options.py (gate w14_approvalgi_b1c8ff)."""
import importlib.util
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_approvalgi_b1c8ff"
sys.path.insert(0, QUARRY)

SRC = QUARRY + "/tools/approval_git_options.py"
_spec = importlib.util.spec_from_file_location("approval_git_options_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["approval_git_options_live"] = live
_spec.loader.exec_module(live)

import json

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
        if rec.get("func") != "join":
            continue
        cases += 1
        py = live._join_escaped_word_chars(rec.get("command", ""))
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
