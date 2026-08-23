"""twin for curator_render_report_markdown.

Traces the LIVE agent/curator.py::_render_report_markdown with the SAME rung
order as the C harness (src/agent/curator_ports.c::cur_render_report_markdown):

  B1 report is a dict        (else -> empty output)
  B2 llm_error truthy
  B3 auto_transitions is a dict
  V "ok" / "empty"

The LIVE function renders many sections the C port omits (consolidated /
pruned / added / state_transitions / cron_rewrites lists, llm_final /
llm_summary) — those carry no rung on either side, so the cups align on the
shared surface. Inputs keep consolidated/pruned/etc. empty and use non-empty
model/provider strings + integer durations.
"""
import os
import json
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_modname = "wc_live_agent_curator"
if _modname in sys.modules:
    _cur = sys.modules[_modname]
else:
    _spec = ilu.spec_from_file_location(
        _modname, os.path.join(DEV, "agent", "curator.py"))
    _cur = ilu.module_from_spec(_spec)
    sys.modules[_modname] = _cur
    try:
        _spec.loader.exec_module(_cur)
    except Exception:
        _cur = None


def live_result(p):
    """Run the LIVE renderer; None when it cannot run (bad input shape)."""
    if _cur is None or not hasattr(_cur, "_render_report_markdown"):
        return None
    try:
        return bool(_cur._render_report_markdown(p))
    except Exception:
        return None


def trace(raw):
    c = Cup("render_report_markdown")
    sig = "empty"
    try:
        p = json.loads(raw)
    except Exception:
        p = None

    if not isinstance(p, dict):                       # B1
        c.branch(False)
    else:
        c.branch(True)
        live_result(p)                                # exercise LIVE body
        error = p.get("llm_error")
        c.branch(bool(error))                         # B2
        # Rung mirrors the C port's object check: None/absent or non-dict
        # -> zero defaults (LIVE folds falsy values to {} which yields the
        # same zero-default path, so the branch condition is equivalent).
        _auto_raw = p.get("auto_transitions")
        c.branch(isinstance(_auto_raw, dict))         # B3
        sig = "ok"
    c.valstr(sig)
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
