"""twin for agent/tool_executor.py:execute_tool_calls_sequential (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_executor.py"
    PY_FN = "execute_tool_calls_sequential"
    C_FN = "execute_tool_calls_sequential"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
