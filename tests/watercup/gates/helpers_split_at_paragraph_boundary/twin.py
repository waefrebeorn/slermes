"""twin for helpers_split_at_paragraph_boundary.

Imports LIVE gateway/platforms/helpers.py::split_at_paragraph_boundary and
traces the SAME rung ladder as the C harness (fits / blank-line /
sentence-newline / last-newline / force-split) plus V events for head+tail.
Branch decisions replicate the LIVE implementation's own operations
(rfind / regex scan) over the same inputs; head/tail come from the LIVE fn.
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_gw_helpers", os.path.join(DEV, "gateway", "platforms", "helpers.py"))
_h = ilu.module_from_spec(_spec)
sys.modules["wc_live_gw_helpers"] = _h
_spec.loader.exec_module(_h)


def trace(raw):
    max_s, _, text = raw.partition("|")
    max_chars = int(max_s)

    c = Cup("split_at_paragraph_boundary")
    # LIVE decision ladder (len_fn=None path)
    if len(text) <= max_chars:
        c.branch(True)
        head, tail = text, ""
    else:
        c.branch(False)
        window = text[:max_chars]

        pos = window.rfind("\n\n")
        blank = pos > 0
        c.branch(blank)                                       # B2
        if blank:
            head, tail = text[:pos + 2], text[pos + 2:]
        else:
            best_pos = -1
            for m in _h._SENTENCE_END_NEWLINE_RE.finditer(window):
                best_pos = m.end()
            c.branch(best_pos > 0)                            # B3
            if best_pos > 0:
                head, tail = text[:best_pos], text[best_pos:]
            else:
                npos = window.rfind("\n")
                c.branch(npos > 0)                            # B4
                if npos > 0:
                    head, tail = text[:npos + 1], text[npos + 1:]
                else:
                    head, tail = text[:len(window)], text[len(window):]
    c.valstr(head)
    c.valstr(tail)
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
