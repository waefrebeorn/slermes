"""twin for hermes_constants_canonical_model_variants.

Traces the LIVE hermes_constants.py::_canonical_model_variants with the same
rung order as the C harness:
  B1 len(parts) >= 2 / B2 len(parts) >= 3 /
  VSTR per final variant (insertion order) + VSTR count.
The full ordered variant list comes from the LIVE function, so any C-side
divergence in derivative generation or dedup order shows up as a cup
mismatch.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_hermes_constants", os.path.join(DEV, "hermes_constants.py"))
_hc = ilu.module_from_spec(_spec)
sys.modules["wc_live_hermes_constants"] = _hc
_spec.loader.exec_module(_hc)
_canonical = _hc._canonical_model_variants


def trace(model):
    c = Cup("reasoning_canonical_model_variants")
    variants = list(_canonical(model))
    nparts = model.split("/").__len__()
    c.branch(nparts >= 2)                       # B1
    c.branch(nparts >= 3)                       # B2
    for v in variants:
        c.valstr(v)
    c.valstr(str(len(variants)))
    return c.finalize()


def run(inputs):
    return [trace(m) for m in inputs]
