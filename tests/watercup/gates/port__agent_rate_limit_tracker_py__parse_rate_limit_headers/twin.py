"""twin for agent/rate_limit_tracker.py:parse_rate_limit_headers."""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_agent_rate_limit_tracker_py", os.path.join(DEV, "agent/rate_limit_tracker.py"))
_pb = ilu.module_from_spec(_spec)
sys.modules["wc_live_agent_rate_limit_tracker_py"] = _pb
_spec.loader.exec_module(_pb)


def trace(inp):
    c = Cup("parse_rate_limit_headers")
    try:
        result = _pb.parse_rate_limit_headers(inp)
        c.valstr(str(result))
    except Exception as e:
        c.valstr(f"ERROR:{type(e).__name__}")
    return c.finalize()


def run(inputs):
    return [trace(inp) for inp in inputs]
