"""twin for gateway_helpers_md_chunk_markdown_paragraphs.

Imports LIVE gateway/platforms/helpers.py::_chunk_markdown_paragraphs and traces
the SAME short-circuit + per-chunk ladder as the harness, which links the REAL C
port (helpers_md_chunk_markdown_paragraphs). Input encoding: max\x1ftext.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_gwhelpers", os.path.join(DEV, "gateway", "platforms", "helpers.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_gwhelpers"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        mx_s, _, text = raw.partition("\x1f")
        mx = int(mx_s)
        c = Cup("chunk_markdown_paragraphs")
        if not text or len(text) <= mx:
            c.branch(True)
            live = mod._chunk_markdown_paragraphs(text, mx)
            assert live == [text], f"LIVE diverges on short-circuit {raw!r}: {live}"
            cups.append(c.finalize())
            continue
        c.branch(False)
        live = mod._chunk_markdown_paragraphs(text, mx)
        for ch in live:
            c.loop()
            c.valstr(ch)
        cups.append(c.finalize())
    return cups
