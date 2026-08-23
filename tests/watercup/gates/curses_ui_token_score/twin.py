"""twin for curses_ui_token_score.

Imports LIVE hermes_cli/curses_ui.py::_token_score and traces the SAME rung
ladder as the C harness (per-char find/contiguous/penalty/boundary/first,
prefix bonus, exact-match bonus, final score value).
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_curses_ui", os.path.join(DEV, "hermes_cli", "curses_ui.py"))
_cui = ilu.module_from_spec(_spec)
sys.modules["wc_live_curses_ui"] = _cui
try:
    _spec.loader.exec_module(_cui)
except Exception:
    _cui = None  # curses import may fail headless; fall back below

if _cui is None or not hasattr(_cui, "_token_score"):
    raise RuntimeError("LIVE hermes_cli/curses_ui.py::_token_score unavailable")


def trace(orig, lower, token):
    c = Cup("token_score")
    live = _cui._token_score
    if not orig or not lower or not token or not token:
        c.branch(True)
    else:
        c.branch(False)
        prev = -1
        positions = []
        # mirror the C ladder, computing each decision with the same data the
        # LIVE fn uses (same lower/orig strings), while LIVE computes score
        score_live = None
        # replay: replicate loop decisions from lower/orig exactly as C does,
        # and let LIVE's result arbitrate the value event.
        search_from = 0
        nomatch = False
        for j, ch in enumerate(token):
            idx = lower.find(ch, search_from)
            if idx < 0:
                c.branch(True)
                nomatch = True
                break
            c.branch(False)
            positions.append(idx)
            c.branch(prev >= 0 and idx == prev + 1)
            if not (prev >= 0 and idx == prev + 1):
                c.branch(prev >= 0)
            c.branch(_cui._is_boundary(orig, idx))
            c.branch(idx == 0)
            prev = idx
            search_from = idx + 1
        if not nomatch:
            c.branch(bool(positions) and positions[0] == 0 and
                     positions[-1] == len(positions) - 1)
            c.branch(lower == token)
        score_live = live(orig, lower, token)
        val = -1.0 if score_live is None else float(score_live)
        c.valstr("%.17g" % val)
    return c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        orig, _, token = raw.partition("|")
        cups.append(trace(orig, orig.lower(), token))
    return cups
