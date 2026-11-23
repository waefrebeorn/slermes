#!/usr/bin/env python3
"""Differential oracle for hermes_cli/auth_device_flow.py (gate w15_authdevice_1acd9a)."""
import importlib.util
import json
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_authdevice_1acd9a"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/hermes_cli/auth_device_flow.py"
_spec = importlib.util.spec_from_file_location("auth_device_flow_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["auth_device_flow_live"] = live
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
        func = rec.get("func")
        if func not in ("console", "timeout_msg"):
            continue
        cases += 1
        if func == "console":
            py = live._names_console_browser(rec.get("value", ""))
        else:
            py = live._nous_device_auth_timeout_message(rec.get("portal"))
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
