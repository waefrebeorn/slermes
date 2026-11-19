"""twin for agent/tool_executor.py:_cancelled_tool_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_executor.py"
    PY_FN = "_cancelled_tool_result"
    C_FN = "_cancelled_tool_result"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
