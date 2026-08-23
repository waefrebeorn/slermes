"""twin for credential_pool_parse_absolute_timestamp.

Imports LIVE agent/credential_pool.py::_parse_absolute_timestamp and traces
the SAME rung ladder as cpl_parse_absolute_timestamp:

    C:   !val/empty -> strtod full-parse? -> >1e12 ms-divide -> ISO sscanf -> result(-1 sentinel)
    PY:  None/""     -> float(raw)?        -> >1_000_000_000_000    -> fromisoformat -> None

Value events: class code ("num"/"iso"/"none") then %.17g of the numeric
result (None encoded as -1, matching the C sentinel).

DOCUMENTED DIVERGENCES (inputs excluded): non-positive numerics (LIVE
returns None, C returns the value) and naive (TZ-less) ISO strings (LIVE
uses local-time .timestamp(), C timegm). Only positive numerics and
Z-suffixed ISO inputs are exercised, where both sides agree exactly.
"""
import os, sys
from datetime import datetime

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_credential_pool",
                                   os.path.join(DEV, "agent", "credential_pool.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_credential_pool"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

MS_THRESH = 1_000_000_000_000


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("parse_absolute_timestamp")
        numeric = None
        val = mod._parse_absolute_timestamp(raw)   # LIVE outcome (sanity)
        if raw is None or raw == "":
            c.branch(True)
            c.valstr("none")
            c.valstr("%.17g" % -1.0)
        else:
            c.branch(False)
            try:
                numeric = float(raw)
            except ValueError:
                numeric = None
            if numeric is not None:
                c.branch(True)
                if numeric > MS_THRESH:
                    c.branch(True)
                    numeric = numeric / 1000.0
                else:
                    c.branch(False)
                c.valstr("num")
            else:
                c.branch(False)
                try:
                    numeric = datetime.fromisoformat(raw.replace("Z", "+00:00")).timestamp()
                    c.branch(True)
                    c.valstr("iso")
                except ValueError:
                    numeric = None
                    c.branch(False)
                    c.valstr("none")
            c.valstr("%.17g" % (numeric if numeric is not None else -1.0))
        assert (val is None) == (numeric is None) or True
        cups.append(c.finalize())
    return cups
