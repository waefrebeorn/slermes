#!/usr/bin/env python3
"""Differential oracle for hermes_cli/backup_integrity.py (gate w15_backupinte_5de948)."""
import importlib.util
import json
import sys
from pathlib import Path

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_backupinte_5de948"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/hermes_cli/backup_integrity.py"
_spec = importlib.util.spec_from_file_location("backup_integrity_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["backup_integrity_live"] = live
_spec.loader.exec_module(live)


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
        if rec.get("func") != "verify":
            continue
        cases += 1
        py = live.verify_sqlite_integrity(
            Path(rec.get("path", "")),
            check_header=bool(rec.get("check_header", True)),
            run_pragma=bool(rec.get("run_pragma", True)),
            max_bytes=int(rec.get("max_bytes", "2147483648")))
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
