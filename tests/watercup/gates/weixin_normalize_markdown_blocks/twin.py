"""twin for weixin_normalize_markdown_blocks.

Traces LIVE gateway/platforms/weixin.py::_normalize_markdown_blocks with the
same rung order as the C harness.
"""
import os
import sys
import importlib.util as ilu
import importlib

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)
sys.path.insert(0, os.path.join(SLERMES, "tests", "watercup"))

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_weixin",
    os.path.join(DEV, "gateway", "platforms", "weixin.py"))
_wx = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_weixin"] = _wx
_spec.loader.exec_module(_wx)


def trace(content):
    c = Cup("normalize_markdown_blocks")
    lines = content.splitlines()
    result = []
    in_code_block = False
    blank_run = 0

    for raw_line in lines:
        c.loop()                                         # L
        line = raw_line.rstrip()
        c.valstr(line)                                   # V1
        if _wx._FENCE_RE.match(line.strip()):
            c.branch(True)                               # B1
            in_code_block = not in_code_block
            result.append(line)
            blank_run = 0
            continue
        c.branch(False)
        if in_code_block:
            c.branch(True)                               # B2
            result.append(line)
            continue
        c.branch(False)
        if not line.strip():
            c.branch(True)                               # B3
            blank_run += 1
            if blank_run <= 1:
                c.branch(True)                           # B4
                result.append("")
            else:
                c.branch(False)
            continue
        c.branch(False)                                  # B5
        blank_run = 0
        result.append(line)

    joined = "\n".join(result).strip()
    c.valstr(joined)                                     # V2
    return c.finalize()


def run(inputs):
    return [trace(t) for t in inputs]
