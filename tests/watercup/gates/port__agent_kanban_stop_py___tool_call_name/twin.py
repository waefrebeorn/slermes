"""twin for agent/kanban_stop.py:_tool_call_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/kanban_stop.py"
    PY_FN = "_tool_call_name"
    C_FN = "agent_kanban_stop_u_tool_call_name"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
