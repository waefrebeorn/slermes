"""twin for usage_pricing_format_token_count_compact.

Traces LIVE agent/usage_pricing.py::format_token_count_compact with the
same rung order as the C harness:
  B1 abs<1000 / B2 >=1e9 / B3 >=1e6 / B4 >=1e3 / V result.
"""
import os
import sys
import importlib.util as ilu
import importlib

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_usage_pricing", os.path.join(DEV, "agent", "usage_pricing.py"))
_up = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_usage_pricing"] = _up
_spec.loader.exec_module(_up)


def trace(value):
    c = Cup("usage_pricing_format_token_count")
    abs_value = abs(int(value))
    if abs_value < 1_000:                     # B1
        c.branch(True)
        result = str(int(value))
        c.valstr(result)
        c.finalize()
        return result, c
    c.branch(False)

    sign = "-" if value < 0 else ""
    result = None
    for threshold, suffix in ((1_000_000_000, "B"), (1_000_000, "M"), (1_000, "K")):
        c.branch(abs_value >= threshold)      # B2/B3/B4
        if abs_value >= threshold:
            scaled = abs_value / threshold
            if scaled < 10:
                text = f"{scaled:.2f}"
            elif scaled < 100:
                text = f"{scaled:.1f}"
            else:
                text = f"{scaled:.0f}"
            if "." in text:
                text = text.rstrip("0").rstrip(".")
            result = f"{sign}{text}{suffix}"
            break
    if result is None:
        result = f"{value:,}"
    c.valstr(result)
    c.finalize()
    return result, c


def run(inputs):
    return [trace(int(p))[1].finalize() for p in inputs]
