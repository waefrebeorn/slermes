"""twin for credits_tracker_is_free_tier_model — LIVE agent/credits_tracker.py::is_free_tier_model."""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_live_credits", os.path.join(DEV, "agent", "credits_tracker.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_credits"] = mod
try:
    spec.loader.exec_module(mod)
    _fn = mod.is_free_tier_model
except Exception:
    # fall back to source-extracted exec of just the pure fn + its deps
    src = open(os.path.join(DEV, "agent", "credits_tracker.py")).read()
    start = src.index("def is_free_tier_model")
    end = src.index("# ── evaluate_credits_notices")
    ns = {}
    exec(src[start:end], ns)
    _fn = ns["is_free_tier_model"]

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("is_free_tier_model")
        model, _, base_url = (raw or "").partition("\t")
        result = _fn(model, base_url)
        c.branch(not model)
        is_free = False
        if model:
            if model.endswith(":free"):
                c.branch(True)
                is_free = True
            else:
                c.branch(False)
                if model.startswith("stealth/"):
                    c.branch(True)
                    is_free = True
                else:
                    c.branch(False)
                    c.branch(not base_url)
                    if base_url:
                        # LIVE pricing-cache peek: cache is empty here -> False
                        pass
        c.valstr("True" if is_free else "False")
        cups.append(c.finalize())
        assert ("True" if is_free else "False") == ("True" if result else "False"), (raw, result)
    return cups
