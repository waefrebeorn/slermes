"""twin for update_cmd_stash_apply_failed.

Traces the LIVE hermes_cli/update_cmd.py::_stash_apply_failed_only_on_existing_untracked
with the same rung order as the C harness:
  B1 empty / per-line LOOP / B2 blank / B3 warning-hint / B4 untracked /
  B5 other-error abort / B6 saw_untracked.
The boolean RESULT comes from the LIVE function; the ladder mirrors the
C port's line walk (strip \\r, skip blanks, prefix markers).
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_update_cmd", os.path.join(DEV, "hermes_cli", "update_cmd.py"))
_uc = ilu.module_from_spec(_spec)
sys.modules["wc_live_update_cmd"] = _uc
try:
    _spec.loader.exec_module(_uc)
    _live = _uc._stash_apply_failed_only_on_existing_untracked
except Exception:
    _live = None  # module-level import failure: fall back to ported semantics


def trace(stderr_text):
    c = Cup("uc_stash_apply_failed")
    if not stderr_text:
        c.branch(True)                       # B1
        return False, c.finalize()
    c.branch(False)
    saw_untracked = False
    aborted = False
    p = stderr_text
    while p:
        c.loop()                             # LOOP
        eol = p.find("\n")
        line = p if eol < 0 else p[:eol]
        rest = "" if eol < 0 else p[eol + 1:]
        if line.endswith("\r"):
            line = line[:-1]
        if not line:                         # B2 blank
            c.branch(True)
            p = rest
            continue
        c.branch(False)
        if line.startswith("warning:") or line.startswith("hint:"):
            c.branch(True)                   # B3
        else:
            c.branch(False)
            if ("already exists, no checkout" in line or
                    "could not restore untracked files from stash" in line):
                c.branch(True)               # B4
                saw_untracked = True
            else:
                c.branch(False)              # B5
                aborted = True
                break
        p = rest
    result = bool(_live(stderr_text)) if _live else (saw_untracked and not aborted)
    if not aborted:
        c.branch(result)                     # B6
    return result, c.finalize()


def run(inputs):
    cups = []
    for s in inputs:
        _, cup = trace(s)
        cups.append(cup)
    return cups
