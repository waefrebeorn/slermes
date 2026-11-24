#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_runtime_start.py
(gate w26_gatewayrun_558360).

The live module is imported whole; only the values are fed in.  spawn_unmanaged_
gateway never runs: strip_supervisor_markers takes the env as a dict, os_refusal
takes the OSError's own fields, and stdio_log_path takes a Path.  The import
chain under tools.environments.local_env_policy needs a home directory, so
USERPROFILE/HOME default to the repo parent.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_gatewayrun_558360"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
os.environ.setdefault("USERPROFILE", str(QUARRY.parent))
os.environ.setdefault("HOME", str(QUARRY.parent))

from hermes_cli import gateway_runtime_start as live  # noqa: E402

WANTED = {"strip_markers", "os_refusal", "stdio_log_path", "constants"}


class Refusal(OSError):
    """An OSError carrying the fields os_refusal reads (winerror included)."""

    def __init__(self, winerror=None, errno=None, strerror=None):
        super().__init__(errno if errno is not None else 0, strerror)
        self.errno = errno
        self.strerror = strerror
        if winerror is not None:
            self.winerror = winerror


def expected(req):
    f = req.get("func")
    if f == "strip_markers":
        env = req.get("env")
        return live.strip_supervisor_markers(dict(env) if isinstance(env, dict) else {})
    if f == "os_refusal":
        exc = req.get("exc")
        exc = exc if isinstance(exc, dict) else {}
        return live.os_refusal(Refusal(exc.get("winerror"), exc.get("errno"), exc.get("strerror")))
    if f == "stdio_log_path":
        home = req.get("home")
        return str(live.stdio_log_path(Path(home) if isinstance(home, str) else Path("")))
    if f == "constants":
        return {
            "supervisor_marker_env": list(live.SUPERVISOR_MARKER_ENV),
            "daemon_stripped_env": list(live.DAEMON_STRIPPED_ENV),
            "client_elevated_remedy": live.CLIENT_ELEVATED_REMEDY,
            "banner_prefixes": list(live._BANNER_PREFIXES),
            "tail_lines": live._TAIL_LINES,
            "tail_bytes": live._TAIL_BYTES,
        }
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
