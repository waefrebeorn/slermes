"""twin for agent/context_engine.py:automatic_compaction_status_message."""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_agent_context_engine_py", os.path.join(DEV, "agent/context_engine.py"))
_pb = ilu.module_from_spec(_spec)
sys.modules["wc_live_agent_context_engine_py"] = _pb
_spec.loader.exec_module(_pb)


def trace(inp):
    c = Cup("automatic_compaction_status_message")
    try:
        result = _pb.automatic_compaction_status_message(inp)
        c.valstr(str(result))
    except Exception as e:
        c.valstr(f"ERROR:{type(e).__name__}")
    return c.finalize()


def run(inputs):
    return [trace(inp) for inp in inputs]
