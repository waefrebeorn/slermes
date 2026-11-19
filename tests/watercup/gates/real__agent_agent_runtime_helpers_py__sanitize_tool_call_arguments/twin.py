"""twin for agent/agent_runtime_helpers.py:sanitize_tool_call_arguments (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "sanitize_tool_call_arguments"
    C_FN = "sanitize_tool_call_arguments"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
