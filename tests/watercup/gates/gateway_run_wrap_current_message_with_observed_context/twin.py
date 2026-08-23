"""twin for gateway_run_wrap_current_message_with_observed_context.

Traces the LIVE gateway/run.py::_wrap_current_message_with_observed_context
string path with the same rung order as the C harness:
  B1 empty observed_context -> message unchanged / B2 str message -> wrap.
Input encoding: "<observed_context>\t<message>" (tab-separated).
The value event hashes the LIVE function's returned string.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_gw_run", os.path.join(DEV, "gateway", "run.py"))
_gr = ilu.module_from_spec(_spec)
sys.modules["wc_live_gw_run"] = _gr
_spec.loader.exec_module(_gr)

_live_fn = _gr._wrap_current_message_with_observed_context


def split_input(item):
    if "\t" in item:
        ctx, msg = item.split("\t", 1)
        return ctx, msg
    return item, None


def trace(ctx, msg):
    c = Cup("wrap_current_message_with_observed_context")
    if not ctx:                                        # B1
        c.branch(False)
        out = _live_fn(msg, None) if msg is not None else None
        result = msg or ""
    else:
        c.branch(True)
        if isinstance(msg, str):                       # B2
            c.branch(True)
            live = _live_fn(msg, ctx)
            result = live if isinstance(live, str) else ""
        else:
            c.branch(False)
            result = ""
    c.valstr(result)
    return c.finalize()


def run(inputs):
    cups = []
    for item in inputs:
        ctx, msg = split_input(item)
        cups.append(trace(ctx, msg))
    return cups
