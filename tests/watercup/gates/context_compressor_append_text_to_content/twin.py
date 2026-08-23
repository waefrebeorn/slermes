"""twin for context_compressor_append_text_to_content.

Traces the LIVE agent/context_compressor.py::_append_text_to_content with the
same rung order as the C harness (src/agent/context_compressor_ports.c::
cc_append_text_to_content):
  B1 null/sentinel / B2 list (+B3 prepend) / B4 string (+B5 prepend).
Inputs are "<content_json>\t<text>\t<prep>" tokens; content is JSON-decoded
so LIVE sees real Python values. Only inputs where C and LIVE carve the same
ladder are exercised (empty-list / empty-string sentinels diverge in outcome
shape and are excluded — see gate.json notes).
"""
import os, sys, json, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_ctx_compressor", os.path.join(DEV, "agent", "context_compressor.py"))
_cc = ilu.module_from_spec(_spec)
sys.modules["wc_live_ctx_compressor"] = _cc
_spec.loader.exec_module(_cc)


def trace(content_json, text, prepend):
    c = Cup("append_text_to_content")
    content = None if content_json == "NULL" else json.loads(content_json)

    sentinel = (content is None)
    c.branch(sentinel)                                   # B1
    if sentinel:
        res = text
    else:
        c.branch(isinstance(content, list))              # B2
        if isinstance(content, list):
            c.branch(prepend)                            # B3
            block = {"type": "text", "text": text}
            res = [block] + content if prepend else content + [block]
        else:
            c.branch(isinstance(content, str))           # B4
            if isinstance(content, str):
                c.branch(prepend)                        # B5
                res = text + content if prepend else content + text
            else:
                res = content                            # passthrough

    live = _cc._append_text_to_content(content, text, prepend=prepend)
    assert live == res, (content, text, prepend, live, res)
    return res, c.finalize()


def run(inputs):
    cups = []
    for tok in inputs:
        content_j, text, prep = tok.split("\t")
        _, cup = trace(content_j, text, prep == "1")
        cups.append(cup)
    return cups
