#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_restart_watcher.py (w20_gatewayres_a0c591).

The live module is imported straight out of the quarry and its only impure
dependency -- ``process_start_time_stdlib(old_pid)``, the /proc or Win32 start
time of the process being replaced -- is rebound to the fixture's seam value.
Compared values: the dedented ``_SOURCE`` template and ``watcher_source``'s
rendered program, as exact strings.
"""
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

import hermes_cli.gateway_restart_watcher as live  # noqa: E402

GATE = "w20_gatewayres_a0c591"


def expected(r):
    f = r.get("func")
    if f == "source_template":
        return live._SOURCE
    if f == "watcher_source":
        start = r.get("start_time")
        start = int(start) if isinstance(start, str) and start else None
        live.process_start_time_stdlib = lambda pid: start
        return live.watcher_source(int(r.get("old_pid") or 0),
                                   respawn_cwd=r.get("respawn_cwd") or "",
                                   respawn_env_overlay=r.get("respawn_env_overlay") or {},
                                   timeout_s=float(r.get("timeout_s") or 0.0),
                                   project_root=r.get("project_root") or "")
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        import json
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") is None:
            continue
        cases += 1
        try:
            exp = expected(r)
        except Exception as exc:  # noqa: BLE001 - a live error is a failed case, not a crash
            mism += 1
            print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if exp != got:
            n = 0
            while n < min(len(exp), len(got or "")) and exp[n] == got[n]:
                n += 1
            mism += 1
            print("MISMATCH %s case=%s at %d py=%r c=%r"
                  % (r.get("func"), r.get("case"), n,
                     exp[max(0, n - 40):n + 40], (got or "")[max(0, n - 40):n + 40]),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
