"""twin for agent/acp_openai_bridge.py:extract_tool_calls_from_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/acp_openai_bridge.py"
    PY_FN = "extract_tool_calls_from_text"
    C_FN = "extract_tool_calls_from_text"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
