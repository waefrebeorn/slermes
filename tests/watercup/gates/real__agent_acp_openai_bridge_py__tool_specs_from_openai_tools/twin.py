"""twin for agent/acp_openai_bridge.py:tool_specs_from_openai_tools (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/acp_openai_bridge.py"
    PY_FN = "tool_specs_from_openai_tools"
    C_FN = "tool_specs_from_openai_tools"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
