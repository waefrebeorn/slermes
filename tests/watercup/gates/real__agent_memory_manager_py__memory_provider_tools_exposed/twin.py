"""twin for agent/memory_manager.py:memory_provider_tools_exposed."""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_agent_memory_manager_py", os.path.join(DEV, "agent/memory_manager.py"))
_pb = ilu.module_from_spec(_spec)
sys.modules["wc_live_agent_memory_manager_py"] = _pb
_spec.loader.exec_module(_pb)


def trace(inp):
    c = Cup("memory_provider_tools_exposed")
    try:
        result = _pb.memory_provider_tools_exposed(inp)
        c.valstr(str(result))
    except Exception as e:
        c.valstr(f"ERROR:{type(e).__name__}")
    return c.finalize()


def run(inputs):
    return [trace(inp) for inp in inputs]
