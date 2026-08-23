"""twin for auxiliary_client_extract_content_or_reasoning.

Traces LIVE agent/auxiliary_client.py::extract_content_or_reasoning with the
same rung order as the C harness: B1 content resolved non-empty / V text.
The input JSON encodes the message object; the twin wraps it in a duck-typed
response (response.choices[0].message).

Agreed sub-contract (documented in gate.json): probes where the C port's
field-scan agrees with LIVE — plain content, or reasoning_content-only
fallbacks. Think-block stripping and the reasoning/reasoning_details rungs
are C-port gaps and are excluded.
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

spec = ilu.spec_from_file_location("wc_live_aux_client",
                                   os.path.join(DEV, "agent", "auxiliary_client.py"))
_aux = ilu.module_from_spec(spec)
sys.modules["wc_live_aux_client"] = _aux
spec.loader.exec_module(_aux)

_extract = _aux.extract_content_or_reasoning


class _Msg:
    content = ""
    reasoning = None
    reasoning_content = None
    reasoning_details = None


def _response(message_dict):
    resp = _Msg()
    msg = _Msg()
    for k, v in message_dict.items():
        setattr(msg, k, v)
    choice = _Msg()
    choice.message = msg
    resp.choices = [choice]
    return resp


def trace(message_json):
    c = Cup("extract_content_or_reasoning")
    try:
        msgd = json.loads(message_json)
    except ValueError:
        msgd = {}
    if not isinstance(msgd, dict):
        msgd = {}
    text = _extract(_response(msgd))
    c.branch(bool(text))         # B1
    c.valstr(text)               # V
    return c.finalize()


def run(inputs):
    return [trace(m) for m in inputs]
