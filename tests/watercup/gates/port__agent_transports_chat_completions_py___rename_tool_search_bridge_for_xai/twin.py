"""twin for agent/transports/chat_completions.py:_rename_tool_search_bridge_for_xai."""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_agent_transports_chat_completions_py", os.path.join(DEV, "agent/transports/chat_completions.py"))
_pb = ilu.module_from_spec(_spec)
sys.modules["wc_live_agent_transports_chat_completions_py"] = _pb
_spec.loader.exec_module(_pb)


def trace(inp):
    c = Cup("_rename_tool_search_bridge_for_xai")
    try:
        result = _pb._rename_tool_search_bridge_for_xai(inp)
        c.valstr(str(result))
    except Exception as e:
        c.valstr(f"ERROR:{type(e).__name__}")
    return c.finalize()


def run(inputs):
    return [trace(inp) for inp in inputs]
