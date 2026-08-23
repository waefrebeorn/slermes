"""twin for banner_format_context_length.

Traces the LIVE hermes_cli/banner.py::_format_context_length with the SAME
rung order as the C harness, and pins the formatted result as a V event.
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup
import importlib.util as ilu

spec = ilu.spec_from_file_location(
    "wc_live_banner", os.path.join(DEV, "hermes_cli", "banner.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_banner"] = mod
spec.loader.exec_module(mod)

_live = mod._format_context_length


def run(inputs):
    cups = []
    for raw in inputs:
        tokens = int(raw)
        c = Cup("banner_format_context_length")
        if tokens >= 1_000_000:                              # B1
            c.branch(True)
            val = tokens / 1_000_000
            rounded = round(val)
            if abs(val - rounded) < 0.05:                    # B2
                c.branch(True)
                out = f"{rounded}M"
            else:
                c.branch(False)
                out = f"{val:.1f}M"
        elif tokens >= 1_000:                                # B3
            c.branch(True)
            val = tokens / 1_000
            rounded = round(val)
            if abs(val - rounded) < 0.05:                    # B4
                c.branch(True)
                out = f"{rounded}K"
            else:
                c.branch(False)
                out = f"{val:.1f}K"
        else:
            c.branch(False)
            out = str(tokens)
        assert out == _live(tokens), "twin drift: %s vs LIVE %s" % (out, _live(tokens))
        c.valstr(out)
        cups.append(c.finalize())
    return cups
