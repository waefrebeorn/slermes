"""twin for weixin_split_markdown_blocks.

LIVE gateway/platforms/weixin.py::_split_markdown_blocks:

    if not content: return []
    lines = content.splitlines(); current=[]; in_code_block=False
    for raw_line in lines:
        line = raw_line.rstrip()
        if _FENCE_RE.match(line.strip()):      # ^```([^\\n`]*)\\s*$
            if not in_code_block and current:
                blocks.append("\\n".join(current).strip()); current=[]
            current.append(line)
            in_code_block = not in_code_block
            if not in_code_block:
                blocks.append("\\n".join(current).strip()); current=[]
            continue
        if in_code_block:
            current.append(line); continue
        if not line.strip():
            if current:
                blocks.append("\\n".join(current).strip()); current=[]
            continue
        current.append(line)
    if current: blocks.append("\\n".join(current).strip())
    return [b for b in blocks if b]

Cup value: str(len(filtered)) + '\\x1f' + '\\x1f'.join(blocks).
"""
from watercup_py import Cup
import re

_FENCE_RE = re.compile(r"^```([^\n`]*)\s*$")


def run(inputs):
    cups = []
    for content in inputs:
        c = Cup("split_markdown_blocks")
        blocks = []
        lines = content.split("\n")
        # C strips trailing CR per line; LIVE uses rstrip() — equivalent for
        # our inputs (no trailing whitespace/CR), and both keep interior text.
        current = []
        in_code = False
        for raw_line in lines:
            line = raw_line.rstrip("\r")
            ls = line.lstrip(" \t")
            is_fence = ls.startswith("```") and _FENCE_RE.match(ls) is not None
            if is_fence:
                c.branch(True)
                if not in_code and current:
                    c.branch(True)
                    blocks.append("\n".join(current).strip())
                    current = []
                else:
                    c.branch(False)
                current.append(line)
                in_code = not in_code
                if not in_code:
                    c.branch(True)
                    blocks.append("\n".join(current).strip())
                    current = []
                else:
                    c.branch(False)
                continue
            else:
                c.branch(False)
            if in_code:
                c.branch(True)
                current.append(line)
                continue
            else:
                c.branch(False)
            if line == "":
                c.branch(True)
                if current:
                    c.branch(True)
                    blocks.append("\n".join(current).strip())
                    current = []
                else:
                    c.branch(False)
                continue
            else:
                c.branch(False)
            current.append(line)
        if current:
            c.branch(True)
            blocks.append("\n".join(current).strip())
        else:
            c.branch(False)
        kept = [b for b in blocks if b]
        joined = str(len(kept))
        for b in kept:
            joined += "\x1f" + b
        c.valstr(joined)
        cups.append(c.finalize())
    return cups
