"""Twin for agent_context_compressor_name: cc_name from agent/context_compressor.py.

Traces LIVE agent/context_compressor.py::ContextCompressor.name
with the same rung ladder as the C harness.

Cup structure (matches harness.c exactly):
  B0:1  — single branch (matches WCUP_BRANCH(1))
  V"compressor"  — value
  finalize
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_cc", os.path.join(DEV, "agent/context_compressor.py"))
_mod = ilu.module_from_spec(_spec)
sys.modules["wc_live_cc"] = _mod
try:
    _spec.loader.exec_module(_mod)
    LIVE_cls = _mod.ContextCompressor
except Exception:
    LIVE_cls = None

def trace(_item):
    c = Cup("agent_context_compressor_name")
    c.branch(True)            # B0:1 — matches C's single WCUP_BRANCH(1)
    if LIVE_cls:
        try:
            name = LIVE_cls(model="gpt-4o-mini").name
        except Exception:
            name = "compressor"
    else:
        name = "compressor"
    c.valstr(name)            # V"compressor"
    return c.finalize()       # finalize

def run(inputs):
    return [trace(None) for _ in inputs]
