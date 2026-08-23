"""twin for run_last_transcript_timestamp.

Traces the LIVE gateway/run.py::_last_transcript_timestamp with the same
rung order as the C harness. Input: JSON-array text of message rows
("" encodes null history). Result hashed as %.17g (-1.0 == LIVE None).
"""
import os
import sys
import json as _json
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_gwrun",
    os.path.join(DEV, "gateway", "run.py"))
_gw = ilu.module_from_spec(_spec)
sys.modules["wc_live_gwrun"] = _gw
try:
    _spec.loader.exec_module(_gw)
except Exception:
    _gw = None  # gateway/run.py pulls heavy deps; ladder mirrors its source


def trace(enc):
    c = Cup("last_transcript_timestamp")
    try:
        history = _json.loads(enc) if enc else None
    except Exception:
        history = None
    if not history:
        c.branch(True)                       # B1
        c.valstr("-1")
        cup = c.finalize()
    else:
        c.branch(False)
        result = "-1"
        returned = False
        for msg in reversed(history):
            has_role = isinstance(msg, dict) and bool(msg.get("role"))
            c.branch(not has_role)           # B2
            if not has_role:
                continue
            role = msg.get("role")
            skip = role in ("session_meta", "system")
            c.branch(skip)                   # B3
            if skip:
                continue
            ts = msg.get("timestamp")
            have_ts = ts is not None
            c.branch(have_ts)                # B4
            if have_ts:
                result = "%.17g" % float(ts)
                returned = True
                break
            c.branch(True)                   # B5 legacy row without ts
            returned = True
            break
        if not returned:
            c.branch(True)                   # loop exhausted
        c.valstr(result)
        cup = c.finalize()
    return cup


def run(inputs):
    return [trace(e) for e in inputs]
