"""twin for agent/tool_executor.py:_run_tool_activity_heartbeat (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_executor.py"
    PY_FN = "_run_tool_activity_heartbeat"
    C_FN = "_run_tool_activity_heartbeat"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
