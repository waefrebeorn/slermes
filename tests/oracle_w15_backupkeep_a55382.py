#!/usr/bin/env python3
"""Differential oracle for hermes_cli/backup_keep_host.py (gate w15_backupkeep_a55382)."""
import importlib.util
import json
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_backupkeep_a55382"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/hermes_cli/backup_keep_host.py"
_spec = importlib.util.spec_from_file_location("backup_keep_host_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["backup_keep_host_live"] = live
_spec.loader.exec_module(live)

DISPATCH = {
    "host_env_key": lambda r: live.host_env_key(r.get("name", "")),
    "env_name": lambda r: live._env_name(r.get("line", "")),
    "host_lines": lambda r: live._host_lines(r.get("text", "")),
    "merge_env": lambda r: live.merge_env(r.get("archive", ""), r.get("host_text", "")),
    "merge_config": lambda r: live.merge_config(r.get("archive"), r.get("host")),
    "merge_auth": lambda r: live.merge_auth(r.get("archive"), r.get("host")),
    "home_parts": lambda r: list(live._home_parts(tuple(r.get("parts") or []))),
    "is_auth_store": lambda r: live._is_auth_store(tuple(r.get("parts") or [])),
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
