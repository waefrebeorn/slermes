"""twin for libdatetime_tz_named_zone — LIVE zoneinfo, same ladder as harness.c.

The oracle is `zoneinfo.ZoneInfo`, i.e. the tz database Python itself would use,
not a table copied out of datetime.c. That is the point: if the port's DST rules
drift from the real rules, the cups move.

Known-zone rung: the harness asks the port whether its table covers the zone; here
the same question is answered by whether zoneinfo can load it.
"""
from datetime import datetime

from watercup_py import Cup
from watercup_twin_runtime import TwinBase


def _zone(name):
    try:
        from zoneinfo import ZoneInfo
        return ZoneInfo(name)
    except Exception:
        return None


class _Twin(TwinBase):
    PY_MODULE = "zoneinfo"
    PY_FN = "ZoneInfo"
    C_FN = "wc_tz_format"

    def trace(self, inp, inputs, idx=0):
        label = getattr(self, "_cup_label", None) or "tz_named_zone"
        c = Cup(label)
        text = str(inp)
        zone_name, _, ts_s = text.partition("|")
        try:
            ts = int(ts_s or 0)
        except ValueError:
            ts = 0

        tz = _zone(zone_name)
        c.branch(tz is not None)          # B1: known zone
        if tz is None:
            c.branch(False)               # B2: formatting failed
            c.valstr("-")
            return c.finalize()

        try:
            out = datetime.fromtimestamp(ts, tz).strftime("%Y-%m-%d %H:%M")
            ok = True
        except (OverflowError, OSError, ValueError):
            out, ok = "-", False
        c.branch(ok)                      # B2
        c.valstr(out)
        return c.finalize()

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
