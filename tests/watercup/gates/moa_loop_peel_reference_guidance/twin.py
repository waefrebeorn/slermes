"""twin for moa_loop_peel_reference_guidance.

Traces LIVE agent/moa_loop.py::peel_reference_guidance with the same rung
order as the C harness: B1 peel applicable / V result messages JSON.
Input encoding: "<messages_json>|<guidance>".
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

import importlib.util as ilu

spec = ilu.spec_from_file_location("wc_live_moa_loop",
                                   os.path.join(DEV, "agent", "moa_loop.py"))
_moa = ilu.module_from_spec(spec)
sys.modules["wc_live_moa_loop"] = _moa
spec.loader.exec_module(_moa)

_peel = _moa.peel_reference_guidance


def trace(pair):
    msgs_json, _, guidance = pair.partition("|")
    c = Cup("peel_reference_guidance")
    try:
        msgs = json.loads(msgs_json) if msgs_json else []
    except ValueError:
        msgs = None
    applicable = bool(guidance) and isinstance(msgs, list) and len(msgs) > 0 \
        and isinstance(msgs[-1], dict)
    c.branch(applicable)                            # B1
    out = _peel(msgs, guidance) if applicable else None
    val = json.dumps(out, separators=(",", ":")) if out is not None else ""
    c.valstr(val)                                   # V
    return c.finalize()


def run(inputs):
    return [trace(p) for p in inputs]
