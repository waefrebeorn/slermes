"""twin for curator_extract_absorbed_into_declarations.

Imports LIVE agent/curator.py::_extract_absorbed_into_declarations and traces
the same result-shape rungs as the C harness (which compiles the REAL port).
Input: JSON array of tool-call dicts.
"""
import os, sys, json as _json
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_curator", os.path.join(DEV, "agent", "curator.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_curator"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("extract_absorbed_into_declarations")
        try:
            calls = _json.loads(raw)
        except Exception:
            calls = []
        out = mod._extract_absorbed_into_declarations(calls)
        c.branch(len(out) > 0)
        for name in sorted(out):
            c.loop()
            c.valstr("%s|%s" % (name, out[name]["into"]))
        cups.append(c.finalize())
    return cups
