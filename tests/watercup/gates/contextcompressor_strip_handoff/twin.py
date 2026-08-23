"""twin for contextcompressor_strip_handoff.

Traces the LIVE agent/context_compressor.py::
ContextCompressor._strip_context_summary_handoff_message with the SAME rung
order as the C harness. The real LIVE result content is hashed at the same
rung, so the real unwrapping logic is exercised on both sides.
"""
import os
import sys
import json
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

# Import the LIVE module (heavy module; load by path like wc_run does).
_spec = ilu.spec_from_file_location(
    "wc_live_context_compressor", os.path.join(DEV, "agent", "context_compressor.py"))
_cc = ilu.module_from_spec(_spec)
sys.modules["wc_live_context_compressor"] = _cc
try:
    _spec.loader.exec_module(_cc)
    CC = _cc.ContextCompressor
except Exception as _e:  # pragma: no cover
    raise

_MERGED = _cc._MERGED_SUMMARY_DELIMITER
_HEADER = _cc._MERGED_PRIOR_CONTEXT_HEADER
_MARKER = _cc._SUMMARY_END_MARKER
_META_KEY = _cc.COMPRESSED_SUMMARY_METADATA_KEY


def classify(content):
    """Mirror of cc_classify_summary_content via LIVE helpers."""
    if not isinstance(content, str):
        return "none"
    s = content.strip(" \t\r\n") if content else ""
    # LIVE uses a contains-style check; mirror the C's strstr semantics.
    idx = s.find(_MERGED)
    if idx >= 0:
        after = s[idx + len(_MERGED):].lstrip()
        return "merged" if CC._starts_with_summary_prefix(after) else "none"
    return "standalone" if CC._starts_with_summary_prefix(s) else "none"


def trace(raw):
    c = Cup("strip_handoff")
    try:
        msg = json.loads(raw)
    except Exception:
        msg = None
    is_obj = isinstance(msg, dict)
    c.branch(is_obj)                                             # B0
    if not is_obj:
        c.valstr("none")
        c.valstr("0")
        c.valstr("<unchanged>")
        c.branch(False)
        return c.finalize()

    content = msg.get("content")
    clsbuf = classify(content)
    c.valstr(clsbuf)                                             # V class
    meta = 1 if (_META_KEY in msg) else 0
    # Mirror of cc_has_compressed_summary_metadata (key presence).
    c.valstr(str(meta))                                          # V metadata
    is_summary = (clsbuf != "none") or bool(meta)
    c.branch(is_summary)                                         # B1

    in_content = content if isinstance(content, str) else ""

    result = CC._strip_context_summary_handoff_message(msg)
    oc = result.get("content") if isinstance(result, dict) else None
    if isinstance(oc, str):
        out_label = oc
        diff = oc != in_content
    else:
        out_label = "<unchanged>"
        diff = False
    c.valstr(out_label)                                          # V result
    c.branch(diff)                                               # B2
    return c.finalize()


def run(inputs):
    return [trace(p) for p in inputs]
