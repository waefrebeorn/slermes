"""twin for libcron_next_run.

LIVE scheduling twin: croniter (the engine hermes_cli/cron.py uses).
Traces the same decision rungs as the C harness over each probe:
  B1 @-special / B2 invalid expression / B3 next found / V1 next epoch (UTC).
Probes: "expr|from_epoch".
"""
import os
import sys
import calendar
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
from watercup_py import Cup

from croniter import croniter


def trace(expr, from_epoch):
    c = Cup("cron_next_run")
    # B1: @-special
    if expr.startswith("@") and not expr.startswith("@every "):
        c.branch(True)
    else:
        c.branch(False)

    dt_from = datetime.fromtimestamp(from_epoch, tz=timezone.utc)
    try:
        it = croniter(expr, dt_from)
        invalid = False
    except Exception:
        invalid = True

    if invalid:                                          # B2
        c.branch(True)
        c.valstr("-1")
        c.finalize()
        return -1, c
    c.branch(False)

    try:
        nxt = it.get_next(datetime)
    except Exception:
        nxt = None
    if nxt is None:                                      # B3: no occurrence
        c.branch(False)
        c.valstr("-1")
        c.finalize()
        return -1, c

    epoch = calendar.timegm(nxt.utctimetuple())
    c.branch(True)                                       # B3
    c.valstr(str(epoch))
    c.finalize()
    return epoch, c


def run(inputs):
    outs = []
    for probe in inputs:
        expr, _, ep = probe.rpartition("|")
        outs.append(trace(expr, int(ep))[1].finalize())
    return outs
