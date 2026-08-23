"""twin for chat_completion_helpers_estimate_chunk_bytes.

Imports the LIVE agent/chat_completion_helpers.py::_estimate_chunk_bytes,
builds a REAL chat-completions chunk object per input triple
"content_len\\treasoning_len\\ttoolargs_len", calls the LIVE fn, and traces
the SAME shared ladder as the C harness:

  V content_len / V reasoning_len / V toolargs_len /
  B payload-present / V final size.
"""
import os
import sys
import types
import importlib
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_cch",
    os.path.join(DEV, "agent", "chat_completion_helpers.py"))
_cch = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_cch"] = _cch
_spec.loader.exec_module(_cch)


def _chunk(clen, rlen, tlen):
    delta = types.SimpleNamespace(
        content="x" * clen if clen else None,
        reasoning_content="y" * rlen if rlen else None,
        reasoning=None,
        tool_calls=[],
    )
    if tlen:
        delta.tool_calls.append(types.SimpleNamespace(
            function=types.SimpleNamespace(arguments="z" * tlen, name=None)))
    return types.SimpleNamespace(choices=[types.SimpleNamespace(delta=delta)])


def run(inputs):
    cups = []
    for raw in inputs:
        parts = [int(p) for p in raw.split("\t")]
        while len(parts) < 3:
            parts.append(0)
        clen, rlen, tlen = parts[:3]
        c = Cup("estimate_chunk_bytes")
        for v in (clen, rlen, tlen):
            c.valstr(str(v))
        size = _cch._estimate_chunk_bytes(_chunk(clen, rlen, tlen))
        c.branch(size != 40)
        c.valstr(str(size))
        cups.append(c.finalize())
    return cups
