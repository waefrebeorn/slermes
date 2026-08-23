"""twin for approval_strip_shell_word_syntax — LIVE tools/approval.py::_strip_shell_word_syntax."""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_live_approval", os.path.join(DEV, "tools", "approval.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_approval"] = mod
try:
    spec.loader.exec_module(mod)
except Exception:
    # heavy module deps — fall back to a source-extracted exec of just the fn
    src = open(os.path.join(DEV, "tools", "approval.py")).read()
    start = src.index("def _strip_shell_word_syntax")
    end = src.index("def _deobfuscate_shell_word_for_detection")
    ns = {}
    exec(src[start:end], ns)
    _fn = ns["_strip_shell_word_syntax"]

from watercup_py import Cup

try:
    _fn = mod._strip_shell_word_syntax
except Exception:
    pass


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("strip_shell_word_syntax")
        word = "" if raw is None else raw
        result = _fn(word)
        chars = []
        quote = None
        i = 0
        while i < len(word):
            ch = word[i]
            if quote:
                c.branch(True)
                if ch == "\\" and quote == '"' and i + 1 < len(word):
                    c.branch(True)
                    chars.append(word[i + 1])
                    i += 2
                    continue
                c.branch(False)
                if ch == quote:
                    c.branch(True)
                    quote = None
                    i += 1
                    continue
                c.branch(False)
                chars.append(ch)
                i += 1
                continue
            c.branch(False)
            if ch in ("'", '"'):
                c.branch(True)
                quote = ch
                i += 1
                continue
            c.branch(False)
            if ch == "\\" and i + 1 < len(word):
                c.branch(True)
                chars.append(word[i + 1])
                i += 2
                continue
            c.branch(False)
            chars.append(ch)
            i += 1
        c.valstr("".join(chars))
        cups.append(c.finalize())
        assert "".join(chars) == result, (raw, chars, result)
    return cups
