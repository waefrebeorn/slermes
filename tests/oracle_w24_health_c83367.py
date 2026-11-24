#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/_state/health.py (gate w24_health_c83367).

The live module owns one process-wide latch behind a lock.  Nothing here opens a
database: the latch is driven purely through its public verbs, reset before each
case so a case *is* the whole history of its paths, and the log sentence is
captured off the module logger (``mark_storage_corrupt`` is the only producer of
it) instead of being invented here.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_health_c83367"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite._state import health as live  # noqa: E402

# The latch logs one ERROR per new key; the message text is compared through
# Capture() below, so the real logger is muted to keep stderr readable.
live.logger.disabled = True

WANTED = {"latch", "log_message", "constants"}


class Capture:
    """Stands in for the module logger: keeps the rendered message only."""

    def __init__(self):
        self.text = None

    def error(self, msg, *args, **kwargs):  # noqa: D401 - logger shim
        self.text = msg % args if args else msg

    def warning(self, msg, *args, **kwargs):
        self.text = msg % args if args else msg

    def info(self, msg, *args, **kwargs):
        pass

    def debug(self, msg, *args, **kwargs):
        pass


def latch(req):
    ops = req.get("ops")
    live.reset_storage_state()
    out = []
    for op in ops if isinstance(ops, list) else []:
        if not isinstance(op, dict):
            out.append("UNKNOWN_OP")
            continue
        kind = op.get("op")
        if kind == "mark":
            live.mark_storage_corrupt(op.get("path"), op.get("reason"))
            out.append(None)
        elif kind == "state":
            out.append(live.storage_state(op.get("path")))
        elif kind == "reason":
            out.append(live.storage_corrupt_reason(op.get("path")))
        elif kind == "reset":
            live.reset_storage_state(op.get("path"))
            out.append(None)
        elif kind == "reset_all":
            live.reset_storage_state()
            out.append(None)
        else:
            out.append("UNKNOWN_OP")
    live.reset_storage_state()
    return out


def log_message(req):
    live.reset_storage_state()
    cap = Capture()
    original = live.logger
    live.logger = cap
    try:
        live.mark_storage_corrupt(req.get("path"), req.get("reason"))
    finally:
        live.logger = original
        live.reset_storage_state()
    return cap.text


def expected(req):
    func = req.get("func")
    if func == "latch":
        return latch(req)
    if func == "log_message":
        return log_message(req)
    if func == "constants":
        return {
            "storage_ok": live.STORAGE_OK,
            "storage_corrupt": live.STORAGE_CORRUPT,
            "all": sorted(live.__all__),
        }
    raise AssertionError("unknown func %r" % (func,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(req)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (req.get("func"), req.get("case"), exc), file=sys.stderr)
            continue
        got = req.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (req.get("func"), req.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
