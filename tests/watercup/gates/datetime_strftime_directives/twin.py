"""twin for datetime_strftime_directives.

Traces the format-string walk with the SAME rung order as the C harness
(D1 '%' / D2 covered directive) and pins the LIVE python strftime output
(aware, timezone.utc so %z/%Z match the C TZ=UTC port) as the V event.
"""
import os, sys, time
from datetime import datetime, timezone

os.environ["TZ"] = "UTC"
time.tzset()

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

COVERED = set("YmdHMSyjAaBbpIUWZz%FTe")


def run(inputs):
    cups = []
    for probe in inputs:
        ts_s, _, fmt = probe.partition("|")
        ts = int(ts_s)
        c = Cup("datetime_strftime_directives")
        q = 0
        while q < len(fmt):
            if fmt[q] == "%":                                   # D1
                c.branch(True)
                q += 1
                d = fmt[q] if q < len(fmt) else "\0"
                c.branch(True if d in COVERED else False)       # D2
            else:
                c.branch(False)
            q += 1
        live = datetime.fromtimestamp(ts, tz=timezone.utc).strftime(fmt)
        c.valstr(live)
        cups.append(c.finalize())
    return cups
