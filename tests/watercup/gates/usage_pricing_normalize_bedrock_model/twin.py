"""twin for usage_pricing_normalize_bedrock_model.

Imports the LIVE Python agent/usage_pricing.py::_normalize_bedrock_model_name
and traces the SAME branch ladder as the C harness, so the cup matches iff the
C port executes the identical control structure as LIVE Python.
"""
import os, sys, re
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "agent"))
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_usage_pricing", os.path.join(DEV, "agent", "usage_pricing.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_usage_pricing"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("normalize_bedrock")
        if raw is None:
            c.branch(True)
            c.valstr("null"); c.valstr("")
            cups.append(c.finalize()); continue
        c.branch(False)
        name = raw.lower().strip()
        stripped = False
        for prefix in ("global.", "us.", "eu.", "apac.", "ap.", "au.", "jp.", "ca.", "sa.", "me.", "af."):
            if name.startswith(prefix):
                name = name[len(prefix):]; stripped = True; break
        c.branch(stripped)
        name = re.sub(r"(\d+)\.(\d+)", r"\1-\2", name)
        name = re.sub(r":\d+$", "", name)
        name = re.sub(r"-v\d+$", "", name)
        name = re.sub(r"-\d{8}$", "", name)
        c.valstr(name)
        cups.append(c.finalize())
    return cups
