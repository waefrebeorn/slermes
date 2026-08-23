"""twin for commands_score_path.

Traces the LIVE hermes_cli/commands.py fuzzy path scorer with the SAME rung
order as the C harness. The LIVE ladder is a strict superset (path-contains
rung -> 40, boundary-bonus rung -> 35); this gate's inputs are restricted to
the agreed sub-contract where C and LIVE coincide (see gate.json notes).
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup


def trace(path, query):
    c = Cup("score_path")
    c.branch(not path)                               # B1 empty path
    if not path:
        c.valstr("0")
        return c.finalize()
    if not query:                                    # B2
        c.branch(True)
        c.valstr("1")
        return c.finalize()
    c.branch(False)
    filename = os.path.basename(path)
    lower_file = filename.lower()
    lower_q = query.lower()

    c.branch(lower_file == lower_q)                  # B3 exact
    if lower_file == lower_q:
        c.valstr("100")
        return c.finalize()
    c.branch(lower_file.startswith(lower_q))         # B4 starts-with
    if lower_file.startswith(lower_q):
        c.valstr("80")
        return c.finalize()
    c.branch(lower_q in lower_file)                  # B5 in filename
    if lower_q in lower_file:
        c.valstr("60")
        return c.finalize()
    # LIVE path-contains rung (40): inputs never hit it — assert that.
    assert lower_q not in path.lower(), "input hits LIVE-only 40 rung"
    qi = 0
    for ch in lower_file:
        if qi < len(lower_q) and ch == lower_q[qi]:
            qi += 1
    subseq = qi == len(lower_q)
    if subseq:
        # LIVE boundary bonus (35): inputs must not reach it — assert that.
        hits = 0
        qi = 0
        prev = "_"
        for ch in lower_file:
            if qi < len(lower_q) and ch == lower_q[qi]:
                if prev in "_-. /":
                    hits += 1
                qi += 1
            prev = ch
        assert not (hits >= len(lower_q) * 0.5), "input hits LIVE-only 35 rung"
    c.branch(subseq)                                 # B6 subsequence
    c.valstr("25" if subseq else "0")
    return c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        path, query = raw.rsplit("|", 1)
        cups.append(trace(path, query))
    return cups
