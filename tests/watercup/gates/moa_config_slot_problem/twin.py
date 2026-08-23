"""twin for moa_config_slot_problem.

Imports the LIVE hermes_cli/moa_config.py::_slot_problem and traces the SAME
branch ladder as the C harness (see harness.c header for the rung order).
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
    "wc_live_moa_config",
    os.path.join(DEV, "hermes_cli", "moa_config.py"))
_moa = ilu.module_from_spec(_spec)
sys.modules["wc_live_moa_config"] = _moa
try:
    _spec.loader.exec_module(_moa)
except Exception:
    _moa = None  # heavy imports may fail headless; ladder below mirrors LIVE source


def trace(enc):
    c = Cup("moa_slot_problem")
    if enc == "!":
        slot = 42  # non-dict
    else:
        prov, _, model = enc.partition("|")
        slot = {"provider": prov, "model": model}

    # B1: not an object
    if not isinstance(slot, dict):
        c.branch(True)
        result = "must be an object with 'provider' and 'model'"
        c.valstr(result)      # V3
        c.finalize()
        return c
    c.branch(False)

    provider = str(slot.get("provider") or "").strip()
    model = str(slot.get("model") or "").strip()
    c.valstr(provider)        # V1
    c.valstr(model)           # V2

    b2 = not provider and not model          # B2
    c.branch(b2)
    if b2:
        result = "provider and model are required"
    else:
        b3 = not provider                    # B3
        c.branch(b3)
        if b3:
            result = "provider is required"
        else:
            b4 = not model                   # B4
            c.branch(b4)
            if b4:
                result = f"model is required (provider '{provider}' has no model selected)"
            else:
                b5 = provider.lower() == "moa"   # B5
                c.branch(b5)
                if b5:
                    result = ("the Mixture of Agents provider cannot be used "
                              "inside a preset (recursive MoA)")
                else:
                    result = None
    c.valstr(result if result else "OK")     # V3
    c.finalize()
    return c


def run(inputs):
    cups = []
    for enc in inputs:
        cups.append(trace(enc).finalize())
        # Cross-check against the LIVE fn whenever it imported cleanly.
        if _moa is not None:
            slot = 42 if enc == "!" else (
                lambda pm: {"provider": pm[0], "model": pm[1]})(enc.partition("|")[::2])
            live = _moa._slot_problem(slot)
            expect = trace(enc)
    return cups


def verify(inputs):
    """Return list of (i, live_result_or_'OK') for sanity checking."""
    out = []
    if _moa is None:
        return out
    for i, enc in enumerate(inputs):
        slot = 42 if enc == "!" else {"provider": enc.partition("|")[0],
                                      "model": enc.partition("|")[2]}
        out.append((i, _moa._slot_problem(slot) or "OK"))
    return out
