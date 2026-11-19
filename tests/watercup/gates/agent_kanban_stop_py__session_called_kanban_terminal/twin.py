"""twin for agent/kanban_stop.py:session_called_kanban_terminal (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/kanban_stop.py"
    PY_FN = "session_called_kanban_terminal"
    C_FN = "agent_kanban_stop_session_called_kanban_terminal"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
