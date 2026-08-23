"""twin for think_scrubber_strip_orphan_close_tags.

Imports LIVE agent/think_scrubber.py::StreamingThinkScrubber._strip_orphan_close_tags
and traces the SAME rung ladder as the C harness (strip_orphan).
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_live_think_scrubber",
                                   os.path.join(DEV, "agent", "think_scrubber.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_think_scrubber"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

CLOSE_TAGS = ["</think>", "</thinking>", "</reasoning>", "</thought>",
              "</REASONING_SCRATCHPAD>"]


def run(inputs):
    cups = []
    fn = mod.StreamingThinkScrubber._strip_orphan_close_tags
    for text in inputs:
        c = Cup("strip_orphan")
        # NOTE: the LIVE method has an early "</" not-in-text fast path that
        # returns text unchanged; the C port has no equivalent short-circuit.
        # We call through anyway (the "</" probe is not traced) so the traced
        # loop below is the shared shape of both sides.
        n = len(text)
        i = 0
        out = []
        while i < n:
            if i + 1 < n and text[i] == "<" and text[i + 1] == "/":
                c.branch(True)
                hit = False
                for tag in CLOSE_TAGS:
                    tl = len(tag)
                    if i + tl > n:
                        continue          # mirrored C guard: no event
                    m = text[i:i + tl].lower() == tag.lower()
                    c.branch(m)
                    if m:
                        j = i + tl
                        while j < n and text[j] in " \t\n\r":
                            j += 1
                        i = j
                        hit = True
                        break
                if hit:
                    c.branch(True)
                    continue
                c.branch(False)
            else:
                c.branch(False)
            out.append(text[i])
            i += 1
        result = "".join(out)
        # sanity: traced copy must agree with LIVE fn
        live = fn(text)
        assert live == result, (text, live, result)
        c.valstr(result)
        cups.append(c.finalize())
    return cups
