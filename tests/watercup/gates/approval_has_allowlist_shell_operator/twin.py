"""twin for approval_has_allowlist_shell_operator.

Imports LIVE tools/approval.py::_has_allowlist_shell_operator and traces the SAME
unquoted-scan branch ladder as the C harness (apr_has_allowlist_shell_operator).
The C port omits the quote-aware refinement, so inputs are restricted to the
agreed unquoted subset; the final boolean comes from LIVE Python itself.
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

CONTROL = set("\n;&|<>`")

def scan_ladder(command):
    """Mirror of the C scan ladder over the unquoted subset."""
    c = Cup("has_allowlist_shell_operator")
    if command is None:
        c.branch(True)
        res = False
    else:
        c.branch(False)
        res = False
        i, n = 0, len(command)
        while i < n:
            ch = command[i]
            c.loop()
            nxt = command[i + 1] if i + 1 < n else ""
            if ch == "\n":
                c.branch(True); res = True; break
            c.branch(False)
            if ch == "&" and nxt == "&":
                c.branch(True); res = True; break
            c.branch(False)
            if ch == "|" and nxt == "|":
                c.branch(True); res = True; break
            c.branch(False)
            if ch in CONTROL:
                c.branch(True); res = True; break
            c.branch(False)
            if ch == "$":
                c.branch(True)
                if nxt == "(":
                    c.branch(True); res = True; break
                c.branch(False)
            else:
                c.branch(False)
            i += 1
    c.valstr("True" if res else "False")
    return c.finalize(), res

def run(inputs):
    cups = []
    for raw in inputs:
        cmd = None if raw == "\x00" else raw
        cup, ladder_res = scan_ladder(cmd)
        live = bool(mod._has_allowlist_shell_operator(cmd or ""))
        assert ladder_res == live, f"LIVE/C-ladder diverge on {raw!r}: {live} vs {ladder_res}"
        cups.append(cup)
    return cups
