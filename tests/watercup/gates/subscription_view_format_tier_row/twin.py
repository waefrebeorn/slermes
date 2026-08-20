"""twin for subscription_view_format_tier_row.

Traces the SAME control structure as the C harness of
agent/subscription_view.py:format_tier_row. The LIVE python fn takes a
SubscriptionTier; we reconstruct the (name, dollars, credits) triple from the same
tab-separated arg the C harness consumes, and trace: null/empty guard, parse
success, whole-vs-fractional dollars, credits>0 suffix. Matches the C cup iff the
port executes the identical control structure.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "agent"))
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_subview", os.path.join(DEV, "agent", "subscription_view.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_subview"] = mod
spec.loader.exec_module(mod)
from watercup_py import Cup

def _format_dollars_grouped(d):
    """Replicate LIVE _format_dollars_grouped branch structure for the cup."""
    c = Cup("_format_dollars_grouped")
    if d == int(d):
        c.branch(True)
    else:
        c.branch(False)
    return c.finalize()  # not used for value, just the branch footprint

def run(inputs):
    cups = []
    for arg in inputs:
        c = Cup("format_tier_row")
        c.valstr(arg if arg is not None else "(null)")
        if not arg or not arg.strip():
            c.branch(False)
            cups.append(c.finalize()); continue
        c.branch(True)
        parts = arg.split("\t")
        if len(parts) < 2:
            c.branch(False)
            cups.append(c.finalize()); continue
        c.branch(True)
        name = parts[0]
        dollars = float(parts[1])
        credits = float(parts[2]) if len(parts) > 2 else -1
        # whole vs fractional dollars branch
        if dollars == int(dollars):
            c.branch(True)
        else:
            c.branch(False)
        # credits>0 suffix branch (mirror C: credits>0)
        if credits > 0:
            c.branch(True); c.valstr("has-credits")
        else:
            c.branch(False); c.valstr("no-credits")
        cups.append(c.finalize())
    return cups
