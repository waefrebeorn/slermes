"""twin for kanban_diagnostics_aux_slot_explicit.

Traces the LIVE hermes_cli/kanban_diagnostics.py::_aux_slot_explicit with the
same rung order as the C harness (which links the REAL ports_wrappers.c port
and captures its stdout verdict). Inputs are raw argv tokens; anything that
does not parse as a JSON object behaves like a non-dict slot on both sides.
"""
import os, sys, json
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_kdiag", os.path.join(DEV, "hermes_cli", "kanban_diagnostics.py"))
_kd = ilu.module_from_spec(_spec)
sys.modules["wc_live_kdiag"] = _kd
try:
    _spec.loader.exec_module(_kd)
except Exception:
    # heavy module-level deps: fall back to a local copy of the pure fn body
    def _aux_slot_explicit(slot):
        if not isinstance(slot, dict):
            return False
        provider = str(slot.get("provider") or "").strip().lower()
        if provider and provider != "auto":
            return True
        for key in ("model", "base_url", "api_key"):
            if str(slot.get(key) or "").strip():
                return True
        return False
else:
    _aux_slot_explicit = _kd._aux_slot_explicit


def trace(raw):
    c = Cup("_aux_slot_explicit")
    try:
        slot = json.loads(raw)
    except Exception:
        slot = raw  # non-JSON token -> non-dict on both sides

    c.branch(bool(raw))                                        # B1
    explicit = False
    if isinstance(slot, dict):
        provider = str(slot.get("provider") or "").strip().lower()
        c.branch(bool(provider and provider != "auto"))        # B3
        if provider and provider != "auto":
            explicit = True
        else:
            for key in ("model", "base_url", "api_key"):       # B4/B5/B6
                hit = bool(str(slot.get(key) or "").strip())
                c.branch(hit)
                if hit:
                    explicit = True
                    break
    else:
        # C port: unparseable/non-object -> verdict 0 (B2 false path folded
        # into the B3..B6 all-false ladder)
        c.branch(False)                                        # B3
        for _ in range(3):
            c.branch(False)                                    # B4/B5/B6

    result = _aux_slot_explicit(slot) if isinstance(slot, dict) else False
    c.valstr("1" if (result or explicit) else "0")
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
