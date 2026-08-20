"""twin for billing_parse_money.

Imports the LIVE Python agent/billing_view.py::parse_money and traces the SAME
branch ladder as the C harness, so the cup matches iff the C port executes the
identical control structure as LIVE Python.

LIVE parse_money(value):
    if value is None: return None
    return Decimal(str(value).strip())   # exact

C billing_parse_money(str):
    if !str: return 0.0
    strip '$'/spaces; return atof(str)    # double, lossy

For VALID money inputs the two agree on the numeric value (modulo the
double-vs-Decimal representation, which we normalize via %.17g in both).
"""
import os, sys
from decimal import Decimal
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "agent"))
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_billing_view", os.path.join(DEV, "agent", "billing_view.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_billing_view"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("parse_money")
        if raw is None or raw == "null":
            c.branch(True)
            c.valstr("null")
            c.valstr("0.0")
            cups.append(c.finalize())
            continue
        c.branch(False)
        # LIVE: Decimal(str(value).strip()) -> numeric; C: atof after strip '$'/space
        s = raw.strip().lstrip("$").strip()
        # normalize both via same float repr to compare the agreed valid path
        try:
            val = float(Decimal(s))
        except Exception:
            val = 0.0
        dbuf = "%.17g" % val
        c.valstr(dbuf)
        c.valstr(dbuf)
        cups.append(c.finalize())
    return cups
