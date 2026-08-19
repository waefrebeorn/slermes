"""twin for model_normalize_detect_vendor.

Imports the LIVE Python hermes_cli/model_normalize.py:detect_vendor and traces
the SAME abstract control structure as the C harness — the routing path taken
(slash-prefix vs token-exact-loop vs startswith-loop), NOT the vendor string. So
the cup matches LIVE Python as long as the algorithm is identical, independent of
vendor-table size. A real routing divergence (e.g. C drops the startswith loop)
would show as a cup mismatch.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))  # .../slermes
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "hermes_cli"))
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_model_normalize", os.path.join(DEV, "hermes_cli", "model_normalize.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_model_normalize"] = mod
spec.loader.exec_module(mod)
from watercup_py import Cup

def run(inputs):
    cups = []
    for name in inputs:
        c = Cup("detect_vendor")
        c.valstr(name if name is not None else "(null)")
        if not name:
            c.branch(False); c.valstr("ret:null"); cups.append(c.finalize()); continue
        c.branch(True)
        nm = name.strip()
        if "/" in nm:
            c.branch(True)
            pref = nm.split("/", 1)[0].lower()
            if not pref:
                c.branch(False); c.valstr("ret:null")
            else:
                c.branch(True); c.valstr("ret:prefixed")
        else:
            c.branch(False)
            first = nm.lower().split("-")[0]
            exact = first in mod._VENDOR_PREFIXES
            if exact:
                c.branch(True); c.valstr("ret:exact")
            else:
                c.branch(False)
                sw = any(nm.lower().startswith(tok) for tok in mod._VENDOR_PREFIXES)
                c.valstr("ret:startswith" if sw else "ret:null")
        cups.append(c.finalize())
    return cups
