"""twin for approval_mask_quoted_newlines.

Imports the LIVE Python tools/approval.py::_mask_quoted_newlines and traces the
SAME branch ladder as the C harness (which is itself a traced copy of the C
port apr2_mask_quoted_newlines). Cup matches iff both execute identical control
structure. The traced result is also asserted equal to the LIVE fn output.
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_approval", os.path.join(DEV, "tools", "approval.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_approval"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def traced(command):
    c = Cup("mask_quoted_newlines")
    has_nl = "\n" in command
    c.branch(has_nl)
    if not has_nl:
        out = command
        c.valstr(out)
        assert out == mod._mask_quoted_newlines(command)
        return c.finalize(), out
    out = []
    quote = None
    i = 0
    while i < len(command):
        c.loop()
        ch = command[i]
        if quote:
            c.branch(True)
            esc = (quote == '"' and ch == "\\" and i + 1 < len(command))
            c.branch(esc)
            if esc:
                out.append(command[i:i + 2])
                i += 2
                continue
            c.branch(ch == quote)
            if ch == quote:
                quote = None
            out.append(" " if ch == "\n" else ch)
            i += 1
            continue
        c.branch(False)
        is_q = ch in ("'", '"')
        c.branch(is_q)
        if is_q:
            quote = ch
        else:
            bs = (ch == "\\" and i + 1 < len(command))
            c.branch(bs)
            if bs:
                out.append(command[i:i + 2])
                i += 2
                continue
        out.append(ch)
        i += 1
    joined = "".join(out)
    live = mod._mask_quoted_newlines(command)
    assert joined == live, f"trace drifted from LIVE: {joined!r} != {live!r}"
    c.valstr(joined)
    return c.finalize(), joined


def run(inputs):
    return [traced(cmd)[0] for cmd in inputs]
