#!/usr/bin/env python3
"""Faithfulness oracle for port_status_helpers.c.

Recomputes _format_iso_timestamp from the LIVE source
hermes_cli/status.py for the same inputs the C harness emits
(one JSON object per line) and compares exactly.

Since Python's _format_iso_timestamp uses datetime.fromisoformat +
astimezone().strftime("%Y-%m-%d %H:%M:%S %Z"), and C uses the
same local TZ, outputs must match on this machine.
"""
import json, sys
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from hermes_cli.status import _format_iso_timestamp

ok = True
n = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    obj = json.loads(line)
    inp = obj["in"]
    got = obj["out"]
    # Python treats '' and whitespace-only as "(unknown)"; None -> (unknown)
    exp = _format_iso_timestamp(inp if inp != "" else None) if inp == "" else _format_iso_timestamp(inp)
    # emulate: C passes NULL for empty; Python expects "(unknown)"
    if inp == "":
        exp = "(unknown)"
    n += 1
    if exp != got:
        ok = False
        print("MISMATCH", repr(inp), "PY=", repr(exp), "C=", repr(got))
print("PYCOMPARE", "OK" if ok else "BAD", f"({n} cases)")
sys.exit(0 if ok else 1)
