"""twin for agent/tool_executor.py:_run_agent_tool_execution_middleware (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_executor.py"
    PY_FN = "_run_agent_tool_execution_middleware"
    C_FN = "_run_agent_tool_execution_middleware"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
