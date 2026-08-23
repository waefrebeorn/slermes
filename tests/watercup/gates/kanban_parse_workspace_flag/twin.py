"""twin for kanban_parse_workspace_flag.

Traces the LIVE hermes_cli/kanban.py::_parse_workspace_flag with the same
rung order as the C harness (see harness.c header). Outcome hashed as
'kind\\tpath' / 'ERR\\t<message>'.
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
    "wc_live_kanban",
    os.path.join(DEV, "hermes_cli", "kanban.py"))
_kanban = ilu.module_from_spec(_spec)
sys.modules["wc_live_kanban"] = _kanban
_spec.loader.exec_module(_kanban)

_fn = _kanban._parse_workspace_flag


def trace(value):
    c = Cup("parse_workspace_flag")
    if not value:                                   # B1
        c.branch(True)
        c.valstr("scratch\t")
        c.finalize()
        return c.finalize()
    c.branch(False)
    v = value.strip()
    c.valstr(v)                                     # V1
    if v in ("scratch", "worktree"):                # B2
        c.branch(True)
        c.valstr("%s\t" % v)
        return c.finalize()
    c.branch(False)
    for prefix, kind in (("dir:", "dir"), ("worktree:", "worktree")):
        hit = v.startswith(prefix)                  # B3 / B4
        c.branch(hit)
        if not hit:
            continue
        path = v[len(prefix):].strip()
        c.valstr(path)                              # V2
        if not path:                                # B5
            c.branch(True)
            try:
                _fn(value)
                msg = "NO-ERROR"
            except Exception as e:
                msg = str(e)
            c.valstr("ERR\t" + msg)                 # V4
            return c.finalize()
        c.branch(False)
        expanded = os.path.expanduser(path)
        c.valstr(expanded)                          # V3
        c.valstr("%s\t%s" % (kind, expanded))       # V4
        return c.finalize()
    c.branch(True)                                  # B6 unknown
    try:
        _fn(value)
        msg = "NO-ERROR"
    except Exception as e:
        msg = str(e)
    c.valstr("ERR\t" + msg)                         # V4
    return c.finalize()


def run(inputs):
    return [trace(v) for v in inputs]
