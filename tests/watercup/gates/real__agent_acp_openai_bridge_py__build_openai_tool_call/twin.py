"""twin for agent/acp_openai_bridge.py:build_openai_tool_call (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/acp_openai_bridge.py"
    PY_FN = "build_openai_tool_call"
    C_FN = "build_openai_tool_call"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
