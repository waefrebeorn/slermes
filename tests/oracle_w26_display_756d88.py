#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/display.py (gate w26_display_756d88).

The route is nothing but a WebSocket spliced into Xvnc; the pure part is the
input gate's eviction decision.  ``_should_evict`` is called with a stand-in
lease (a plain object with ``holder``/``viewer_id``) and a plain dict for the
per-connection memory -- the lease file, the socket and the loop never exist.
"""
import json
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_display_756d88"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli.web_routers import display as live  # noqa: E402

WANTED = {"should_evict", "constants"}

CONSTANTS = (
    ("activity_stamp_s", "_ACTIVITY_STAMP_S"),
    ("read_chunk", "_READ_CHUNK"),
    ("close_control_taken", "_CLOSE_CONTROL_TAKEN"),
    ("close_desktop_gone", "_CLOSE_DESKTOP_GONE"),
    ("close_bad_ticket", "_CLOSE_BAD_TICKET"),
    ("close_not_allowed", "_CLOSE_NOT_ALLOWED"),
    ("close_protocol", "_CLOSE_PROTOCOL"),
    ("lease_refresh_s", "_LEASE_REFRESH_S"),
)


def should_evict(req):
    held = dict(req.get("held"))
    raw = req.get("lease")
    lease = SimpleNamespace(holder=raw.get("holder"), viewer_id=raw.get("viewer_id"))
    evict = live._should_evict(held, lease, req.get("viewer_id"))
    return {"evict": evict, "ever": held["ever"]}


def constants():
    out = {key: getattr(live, attr) for key, attr in CONSTANTS}
    out["clean_close"] = sorted(live._CLEAN_CLOSE)
    return out


def expected(r):
    f = r.get("func")
    if f == "should_evict":
        return should_evict(r)
    if f == "constants":
        return constants()
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
        except AttributeError:
            py = {"error": "AttributeError"}
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
