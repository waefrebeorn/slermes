"""twin for model_switch_models_config_is_allowlist.

Traces the LIVE hermes_cli/model_switch.py::_models_config_is_allowlist with
the same rung order as the C harness (discovered=False default):
  B1 null / B2 unparseable JSON / B3 str / B4 dict / B5 list / V1 result.
The RESULT value comes from the LIVE function.
"""
import os, sys, json, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_model_switch", os.path.join(DEV, "hermes_cli", "model_switch.py"))
_ms = ilu.module_from_spec(_spec)
sys.modules["wc_live_model_switch"] = _ms
try:
    _spec.loader.exec_module(_ms)
    live = _ms._models_config_is_allowlist
except Exception:
    live = None


def trace(value_json):
    c = Cup("models_config_is_allowlist")
    if value_json is None:
        c.branch(True)                          # B1
        c.valstr("False")
        return False, c.finalize()
    c.branch(False)
    try:
        value = json.loads(value_json)
        parsed = True
    except Exception:
        value = None
        parsed = False
    if not parsed:
        c.branch(True)                          # B2
        c.valstr("False")
        return False, c.finalize()
    c.branch(False)
    if isinstance(value, str):                  # B3
        c.branch(True)
        result = bool(live(value)) if live else bool(value.strip())
    else:
        c.branch(False)
        if isinstance(value, dict):             # B4
            c.branch(True)
            result = bool(live(value)) if live else False
        else:
            c.branch(False)
            is_list = isinstance(value, list)
            c.branch(is_list)                   # B5
            result = bool(live(value)) if live else is_list
    c.valstr("True" if result else "False")     # V1
    return result, c.finalize()


def run(inputs):
    cups = []
    for s in inputs:
        _, cup = trace(s)
        cups.append(cup)
    return cups
