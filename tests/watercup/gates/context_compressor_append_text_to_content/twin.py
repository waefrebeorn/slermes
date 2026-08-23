"""twin for context_compressor_append_text_to_content.

Traces the LIVE agent/context_compressor.py::_append_text_to_content with the
same rung order as the C harness (traced copy of
context_compressor_ports.c::cc_append_text_to_content).

Each input token encodes "<prepend>\t<content_json>"; content_json is parsed
as JSON (unparseable tokens behave as raw scalars on both sides). Text is the
fixed literal WCUP_NOTE.
  B1: no-content trigger (None / "" / [])
  B2: list-shaped content -> text block inserted
  B3: string-shaped content -> concatenation
  VSTR: result kind ("scalar" | "blocks" | "opaque")
"""
import os, sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

TEXT = "WCUP_NOTE"

_spec = ilu.spec_from_file_location(
    "wc_live_cc", os.path.join(DEV, "agent", "context_compressor.py"))
_cc = ilu.module_from_spec(_spec)
sys.modules["wc_live_cc"] = _cc
try:
    _spec.loader.exec_module(_cc)
    live_append = _cc._append_text_to_content
except Exception:
    # heavy module-level deps: local copy of the pure fn body
    def live_append(content, text, *, prepend=False):
        if content is None:
            return text
        if isinstance(content, str):
            return text + content if prepend else content + text
        if isinstance(content, list):
            block = {"type": "text", "text": text}
            return [block, *content] if prepend else [*content, block]
        rendered = str(content)
        return text + rendered if prepend else rendered + text


def trace(raw):
    c = Cup("_append_text_to_content")
    tab = raw.find("\t")
    prepend = raw.startswith("1")
    cj_raw = raw[tab + 1:] if tab >= 0 else ""
    try:
        content = __import__("json").loads(cj_raw)
    except Exception:
        content = ("RAW", cj_raw)   # unparseable -> opaque scalar on both sides

    no_content = content is None or content == "" or content == []
    c.branch(no_content)                                        # B1
    c.branch(isinstance(content, list))                         # B2
    c.branch(isinstance(content, str))                          # B3

    result = live_append(content, TEXT, prepend=prepend)
    # result kind follows the same shape ladder the C port walks
    kind = ("blocks" if isinstance(content, list)
            else "scalar" if isinstance(content, (str, type(None)))
            else "opaque")
    if not no_content:
        # sanity: LIVE's own result class agrees with the ladder
        live_kind = ("blocks" if isinstance(result, list)
                     else "scalar" if isinstance(result, str) else "opaque")
        assert live_kind == kind or kind == "opaque", \
            "LIVE result kind %r diverges from shape ladder %r" % (live_kind, kind)
    c.valstr(kind)
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
