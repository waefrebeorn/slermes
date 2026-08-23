"""twin for hermes_cli_moa_config_normalize_preset.

Traces the LIVE hermes_cli/moa_config.py::_normalize_preset ladder with the
same rung order as the C harness:
  B1 dict / B2 raw_refs string (never true on this walk) / B3 list /
  LOOP clean_slot keep B4 / B5 empty -> defaults / B6 aggregator fallback.
Value events hash enabled + reference-model count from the LIVE result.
"""
import json as _json
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_moa", os.path.join(DEV, "hermes_cli", "moa_config.py"))
_moa = ilu.module_from_spec(_spec)
sys.modules["wc_live_moa"] = _moa
if _spec.loader:
    _spec.loader.exec_module(_moa)

_live_fn = _moa._normalize_preset
_clean_slot = _moa._clean_slot


def trace(raw):
    c = Cup("normalize_preset")
    live = _live_fn(raw)
    if not isinstance(raw, dict):                     # B1
        c.branch(False)
        c.valstr("enabled=%d|refs=%d" % (
            1 if live.get("enabled") else 0, len(live["reference_models"])))
        return c.finalize()
    c.branch(True)

    raw_refs = raw.get("reference_models")
    if isinstance(raw_refs, str):                     # B2 (never on this walk)
        c.branch(True)
        try:
            raw_refs = _json.loads(raw_refs)
        except ValueError:
            raw_refs = []
    else:
        c.branch(False)

    refs = []
    if isinstance(raw_refs, list):                    # B3
        c.branch(True)
        for item in raw_refs:                         # LOOP
            c.loop()
            cleaned = _clean_slot(item, include_enabled=True)
            if cleaned is not None:                   # B4
                c.branch(True)
                refs.append(cleaned)
            else:
                c.branch(False)
    else:
        c.branch(False)

    if not refs:                                      # B5 -> LIVE defaults
        c.branch(True)
        refs = _moa._default_reference_models()
    else:
        c.branch(False)

    cleaned_agg = _clean_slot(raw.get("aggregator"))  # B6 rung
    if cleaned_agg:
        c.branch(True)
    else:
        c.branch(False)
    _ = cleaned_agg

    enabled = bool(live.get("enabled"))
    c.valstr("enabled=%d|refs=%d" % (1 if enabled else 0,
                                     len(live["reference_models"])))
    return c.finalize()


def run(inputs):
    cups = []
    for item in inputs:
        cups.append(trace(_json.loads(item)))
    return cups
