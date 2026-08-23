"""twin for image_routing_coerce_capability_bool.

Imports LIVE agent/image_routing.py::_coerce_capability_bool and traces the
SAME strcmp branch ladder as the C harness
(image_routing_ports.c::imgr_coerce_capability_bool). None maps to "-1".
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_live_image_routing", os.path.join(DEV, "agent", "image_routing.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_image_routing"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

TRUE_TOKENS = ("true", "1", "yes")
FALSE_TOKENS = ("false", "0", "no")


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("coerce_capability_bool")
        value = None if raw == "null" else raw
        if value is None:
            c.branch(True)
            res = "-1"
        else:
            c.branch(False)
            l = value.lower()
            r = -1
            matched = False
            for i, tok in enumerate(TRUE_TOKENS + FALSE_TOKENS):
                if l == tok:
                    c.branch(True)
                    r = 1 if i < len(TRUE_TOKENS) else 0
                    matched = True
                    break
                c.branch(False)
            if not matched:
                r = -1
            res = str(r)
        c.valstr(res)
        cups.append(c.finalize())
    return cups
