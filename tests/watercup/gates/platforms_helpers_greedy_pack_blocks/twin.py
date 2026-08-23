"""twin for platforms_helpers_greedy_pack_blocks.

Imports LIVE gateway/platforms/helpers.py::greedy_pack_blocks and traces the
SAME rung ladder as helpers_md_greedy_pack_blocks with exact packing parity.

Input encoding: "maxlen|SEP|b1|b2|..." where SEP is the literal escape "\\n"
(backslash-n) for a real newline (decoded identically on both sides).
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_platforms_helpers",
                                   os.path.join(DEV, "gateway", "platforms", "helpers.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_platforms_helpers"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def decode_sep(esc):
    return esc.replace("\\n", "\n")


def trace(inp):
    c = Cup("greedy_pack_blocks")
    head, sep_esc, rest = inp.split("|", 2)
    max_length = int(head)
    sep = decode_sep(sep_esc)
    blocks = rest.split("|")
    live = mod.greedy_pack_blocks(blocks, max_length, sep=sep)

    packed = []
    current = ""
    for block in blocks:
        c.loop()
        candidate = block if not current else f"{current}{sep}{block}"
        if len(candidate) <= max_length:
            c.branch(True)
            current = candidate
            continue
        c.branch(False)
        if current:
            c.branch(True)
            packed.append(current)
            current = ""
        else:
            c.branch(False)
        if len(block) <= max_length:
            c.branch(True)
            current = block
        else:
            c.branch(False)
            packed.append(block)
    if current:
        packed.append(current)

    assert packed == live, "packing divergence: %r vs %r" % (packed, live)
    c.valstr("\x1e".join(packed))
    return c.finalize()


def run(inputs):
    return [trace(i) for i in inputs]
