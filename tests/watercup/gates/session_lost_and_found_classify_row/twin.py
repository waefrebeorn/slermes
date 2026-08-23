"""twin for session_lost_and_found_classify_lost_and_found_row.

Imports LIVE hermes_cli/session_lost_and_found.py and traces the SAME rung
ladder as the C harness over the same encoded inputs
("<nfield>|<cell>,<cell>,..." with N=NULL, S<text>=string).
Helper predicates (_is_session_id, _looks_like_source) are evaluated by
calling the LIVE functions; the gate traces the classify-level ladder.
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
    "wc_live_laf", os.path.join(DEV, "hermes_cli", "session_lost_and_found.py"))
_laf = ilu.module_from_spec(_spec)
sys.modules["wc_live_laf"] = _laf
_spec.loader.exec_module(_laf)


def parse(raw):
    nf_s, _, cells_s = raw.partition("|")
    nfield = int(nf_s)
    cells = []
    if cells_s:
        for tok in cells_s.split(","):
            if tok == "N":
                cells.append(None)
            elif tok.startswith("S"):
                cells.append(tok[1:])
            else:
                raise ValueError("bad cell %r" % tok)
    return nfield, tuple(cells)


def trace(raw):
    nfield, cells = parse(raw)
    c = Cup("classify_lost_and_found_row")
    b1 = len(cells) >= 3 and cells[0] is None
    c.branch(b1)
    if b1:
        b2 = (isinstance(cells[1], str) and bool(cells[1])
              and isinstance(cells[2], str)
              and cells[2] in _laf.MESSAGE_ROLES)
        c.branch(b2)
        result = "messages" if b2 else "null"
        c.valstr(result)
        return c.finalize()

    first = cells[0] if cells else None
    sid = _laf._is_session_id(first)
    c.branch(sid)
    if not sid:
        c.valstr("null")
        return c.finalize()

    c.branch(nfield == 18)
    if nfield == 18:
        b5 = len(cells) > 1 and isinstance(cells[1], str) and bool(cells[1])
        c.branch(b5)
        c.valstr("session_model_usage" if b5 else "null")
        return c.finalize()

    in_layouts = (nfield in _laf.SESSIONS_LAYOUT_NFIELDS
                  or nfield == _laf.SESSIONS_LEGACY_MINIMAL_NFIELD)
    c.branch(in_layouts)
    if in_layouts:
        b7 = len(cells) > 1 and _laf._looks_like_source(cells[1])
        c.branch(b7)
        c.valstr("sessions" if b7 else "null")
        return c.finalize()

    b8 = nfield >= 30 and len(cells) > 1 and _laf._looks_like_source(cells[1])
    c.branch(b8)
    c.valstr("sessions" if b8 else "null")
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
