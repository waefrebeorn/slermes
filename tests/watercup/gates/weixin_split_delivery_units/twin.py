"""twin for weixin_split_delivery_units.

Imports LIVE gateway/platforms/weixin.py::_split_delivery_units_for_weixin and
traces the SAME ladder as the C harness. Value event per unit, then count.
Inputs are single-line strings with '|' separating lines (argv-safe).
"""
import importlib.util as ilu
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_weixin", os.path.join(DEV, "gateway", "platforms", "weixin.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_weixin"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        content = raw.replace("|", "\n")
        units = mod._split_delivery_units_for_weixin(content)
        c = Cup("split_delivery_units")
        empty = not content.strip()
        c.branch(empty)
        if not empty:
            blocks = mod._split_markdown_blocks(content)
            for blk in blocks:
                c.loop()
                first = blk.splitlines()[0].strip()
                if mod._FENCE_RE.match(first):
                    c.branch(True)
                    continue
                c.branch(False)
                current = []
                for raw_line in blk.splitlines():
                    c.loop()
                    line = raw_line.rstrip()
                    if not line.strip():
                        c.branch(True)
                        if current:
                            c.branch(True)
                        else:
                            c.branch(False)
                        current = []
                        continue
                    c.branch(False)
                    is_cont = bool(current) and raw_line.startswith((" ", "\t"))
                    if is_cont:
                        c.branch(True)
                        current.append(line)
                        continue
                    c.branch(False)
                    if current:
                        c.branch(True)
                    else:
                        c.branch(False)
                    current = [line]
                if current:
                    c.branch(True)
                else:
                    c.branch(False)
        for u in units:
            c.valstr(u)
        c.valstr("count=%d" % len(units))
        cups.append(c.finalize())
    return cups
