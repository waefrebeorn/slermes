"""twin for billing_format_money.

Imports the LIVE Python agent/billing_view.py:format_money and traces the SAME
branch ladder as the C harness, so the cup matches iff the C port executes the
identical control structure as LIVE Python.

LIVE format_money(value: Decimal) -> str:
    if value == value.to_integral_value():   # whole dollars
        return f"${format(value.to_integral_value(), 'f')}"
    return f"${value:.2f}"                     # fractional -> 2dp
(None -> "—", but the C port takes a double so we test finite values.)
"""
import os, sys
from decimal import Decimal
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))  # .../slermes
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "agent"))
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_billing_view", os.path.join(DEV, "agent", "billing_view.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_billing_view"] = mod   # register before exec
spec.loader.exec_module(mod)

from watercup_py import Cup  # adjacent to gate dir

def run(inputs):
    cups = []
    for raw in inputs:
        amt = Decimal(raw)
        c = Cup("format_money")
        c.valstr("%.17g" % float(amt))
        if amt == amt.to_integral_value():
            c.branch(True)    # whole dollars
            out = f"${format(amt.to_integral_value(), 'f')}"
        else:
            c.branch(False)   # fractional -> 2dp
            out = f"${amt:.2f}"
        c.valstr(out)
        cups.append(c.finalize())
    return cups
