"""twin for agent/session_activity.py:reset_session_activity_persist_window."""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_agent_session_activity_py", os.path.join(DEV, "agent/session_activity.py"))
_pb = ilu.module_from_spec(_spec)
sys.modules["wc_live_agent_session_activity_py"] = _pb
_spec.loader.exec_module(_pb)


def trace(inp):
    c = Cup("reset_session_activity_persist_window")
    try:
        result = _pb.reset_session_activity_persist_window(inp)
        c.valstr(str(result))
    except Exception as e:
        c.valstr(f"ERROR:{type(e).__name__}")
    return c.finalize()


def run(inputs):
    return [trace(inp) for inp in inputs]
