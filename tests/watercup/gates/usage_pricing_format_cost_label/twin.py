"""twin for usage_pricing_format_cost_label.

Imports the LIVE Python agent/usage_pricing.py:format_cost_label and traces the
SAME branch ladder as the C harness, so the cup matches iff the C port executes
the identical control structure as LIVE Python.

format_cost_label(amount: Decimal) -> str:
    if amount == 0: return "$0.00"
    if amount < 0.01:
        label = f"~${amount:.4f}"
        return label if label != "~$0.0000" else "~$<0.0001"
    return f"~${amount:.2f}"
"""
import os, sys
from decimal import Decimal
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))  # .../slermes
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "agent"))
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_usage_pricing", os.path.join(DEV, "agent", "usage_pricing.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_usage_pricing"] = mod   # register before exec (dataclass needs it)
spec.loader.exec_module(mod)

from watercup_py import Cup  # adjacent to gate dir

def run(inputs):
    cups = []
    for raw in inputs:
        amt = Decimal(raw)
        c = Cup("format_cost_label")
        c.valstr("%.17g" % float(amt))
        if amt == Decimal(0):
            c.branch(True)
            out = "$0.00"
        else:
            c.branch(False)
            if amt < Decimal("0.01"):
                c.branch(True)
                label = f"~${amt:.4f}"
                # mirror C: guard fires when rendered label == "~$0.0000"
                if label == "~$0.0000":
                    c.branch(True)
                    out = "~$<0.0001"
                else:
                    c.branch(False)
                    out = label
            else:
                c.branch(False)
                out = f"~${amt:.2f}"
        c.valstr(out)
        cups.append(c.finalize())
    return cups
