# Twin for agent_account_usage_title_case_slug
# Mirrors Python: agent/account_usage.py::_title_case_slug
#
# Python: s=str(value or "").strip(); if not s: return None
#         return s.replace("_", " ").replace("-", " ").title()
#
# The C harness converts "NULL" argv → NULL pointer.
# We must do the same: if input == "NULL", treat as None.

import sys
import os
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.dirname(HERE)))))
DEV = SLERMES

_spec = ilu.spec_from_file_location("wc_live_ac", os.path.join(DEV, "agent", "account_usage.py"))
assert _spec is not None
_mod = ilu.module_from_spec(_spec)
sys.modules["wc_live_ac"] = _mod
_spec.loader.exec_module(_mod)
LIVE = _mod._title_case_slug

from watercup_py import Cup

def trace(_input):
    # Mirror C harness: "NULL" string → None
    if _input == "NULL":
        value = None
    else:
        value = _input
    c = Cup("account_usage_title_case_slug")
    c.branch(True)
    result = LIVE(value)
    c.valstr(result if result is not None else "")
    return c.finalize()

def run(inputs):
    return [trace(inp) for inp in inputs]

def inputs_for_json():
    return [
        {"args": ["foo_bar"], "kwargs": {}},
        {"args": ["hello-world"], "kwargs": {}},
        {"args": ["  spaced  out  "], "kwargs": {}},
        {"args": ["NULL"], "kwargs": {}},
        {"args": ["aBc"], "kwargs": {}},
        {"args": ["already-title"], "kwargs": {}},
    ]
