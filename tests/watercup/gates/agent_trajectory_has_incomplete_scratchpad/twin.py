"""Twin for agent_trajectory_has_incomplete_scratchpad: has_incomplete_scratchpad from agent/trajectory.py."""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_traj", os.path.join(DEV, "agent/trajectory.py"))
_mod = ilu.module_from_spec(_spec)
sys.modules["wc_live_traj"] = _mod
try:
    _spec.loader.exec_module(_mod)
    LIVE = _mod.has_incomplete_scratchpad
except Exception:
    LIVE = None

def trace(input_str):
    c = Cup("has_incomplete_scratchpad")
    if input_str == "NULL":
        c.valstr("false")
        return c.finalize()
    if LIVE:
        try:
            result = LIVE(input_str)
        except Exception:
            result = False
    else:
        result = False
    c.valstr(str(result).lower())
    return c.finalize()

def run(inputs):
    return [trace(inp) for inp in inputs]
