"""twin for display_split_shell_words.

Imports LIVE agent/display.py::_split_shell_words and traces the same branch
ladder as the C harness (_cli_split_shell_words). The V event is the LIVE
function's real word list.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_display", os.path.join(DEV, "agent", "display.py"))
mod = ilu.module_from_spec(spec); sys.modules["wc_display"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def run(inputs):
    cups = []
    for seg in inputs:
        words = mod._split_shell_words(seg)
        c = Cup("split_shell_words")
        # re-walk the segment tracing the C ladder (identical structure)
        quote = None
        in_word = False
        closed = []
        for ch in seg:
            c.loop()
            if quote:
                c.branch(True)
                if ch == quote: quote = None
                in_word = True
                continue
            else:
                c.branch(False)
            if ch in ("'", '"'):
                c.branch(True)
                if not in_word: c.branch(True); in_word = True
                else: c.branch(False)
                quote = ch
                continue
            else:
                c.branch(False)
            if ch in (" ", "\t", "\n", "\r"):
                c.branch(True)
                if in_word:
                    c.branch(True); in_word = False
                else:
                    c.branch(False)
                continue
            else:
                c.branch(False)
            if not in_word: c.branch(True); in_word = True
            else: c.branch(False)
        c.branch(bool(in_word))
        c.valstr("\x1f".join(words))
        cups.append(c.finalize())
    return cups
