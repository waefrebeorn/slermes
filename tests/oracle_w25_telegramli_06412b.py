#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_live_location.py
(gate w25_telegramli_06412b).

PTB never loads here: the live ``is_redundant_live_refresh`` is called with a
fake adapter (its ``__dict__`` pre-seeded with the case's share table), a fake
message whose ``edit_date``/``date`` carry ``timestamp()``, and a fake location
with ``live_period``.  The share table is an ``OrderedDict`` exactly as the live
function would keep it, so the returned value plus the resulting table are both
compared.  ``_meters_between`` is called directly.
"""
import importlib.util
import json
import sys
from collections import OrderedDict
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_telegramli_06412b"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# Loaded from its file, not through plugins.platforms.telegram: that package
# __init__ pulls the whole PTB adapter stack (which needs a resolvable home dir)
# and this module has no relative imports of its own.  The source executed is the
# live upstream file.
_spec = importlib.util.spec_from_file_location(
    "telegram_live_location_live", QUARRY / "plugins/platforms/telegram/telegram_live_location.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {"meters_between", "is_redundant", "constants"}
TOLERANCE = 1e-9


class _Stamp:
    """A datetime stand-in: _stamp() only asks for timestamp()."""

    def __init__(self, ts):
        self._ts = ts

    def timestamp(self):
        return self._ts


class _Msg:
    def __init__(self, data):
        self.chat = None  # so the chat_id fallback is the one exercised
        self.chat_id = data.get("chat_id")
        self.message_id = data.get("message_id")
        edit = data.get("edit_date")
        self.edit_date = None if edit is None else _Stamp(edit)
        date = data.get("date")
        self.date = None if date is None else _Stamp(date)


class _Location:
    def __init__(self, data):
        self.live_period = data.get("live_period")


class _Adapter:
    pass


def meters_between(req):
    return live._meters_between(req.get("lat1", 0.0), req.get("lon1", 0.0),
                                req.get("lat2", 0.0), req.get("lon2", 0.0))


def is_redundant(req):
    adapter = _Adapter()
    table = OrderedDict()
    raw = req.get("shares")
    for entry in raw if isinstance(raw, list) else []:
        if not isinstance(entry, list) or len(entry) < 5:
            continue
        table[(str(entry[0]), str(entry[1]))] = (entry[2], entry[3], entry[4])
    adapter.__dict__["_telegram_live_location_shares"] = table
    msg = _Msg(req.get("msg") or {})
    location = _Location(req.get("location") or {})
    redundant = live.is_redundant_live_refresh(adapter, msg, location,
                                              req.get("lat", 0.0), req.get("lon", 0.0))
    after = adapter.__dict__["_telegram_live_location_shares"]
    return {
        "redundant": bool(redundant),
        "shares": [[str(k[0]), str(k[1]), v[0], v[1], v[2]] for k, v in after.items()],
    }


def constants():
    return {
        "live_refresh_min_meters": float(live.LIVE_REFRESH_MIN_METERS),
        "live_refresh_min_seconds": float(live.LIVE_REFRESH_MIN_SECONDS),
        "max_tracked_shares": int(live._MAX_TRACKED_SHARES),
    }


def expected(r):
    f = r.get("func")
    if f == "meters_between":
        return meters_between(r)
    if f == "is_redundant":
        return is_redundant(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def close(py, got, func):
    """Floats from libm are compared with a tolerance; everything else exactly."""
    if func == "meters_between" and isinstance(py, (int, float)) and isinstance(got, (int, float)):
        return abs(py - got) <= TOLERANCE
    return py == got


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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if not close(py, got, r.get("func")):
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
