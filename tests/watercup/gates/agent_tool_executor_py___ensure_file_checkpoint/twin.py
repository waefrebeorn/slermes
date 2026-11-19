"""twin for agent/tool_executor.py:_ensure_file_checkpoint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_executor.py"
    PY_FN = "_ensure_file_checkpoint"
    C_FN = "agent_tool_executor_u_ensure_file_checkpoint"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
