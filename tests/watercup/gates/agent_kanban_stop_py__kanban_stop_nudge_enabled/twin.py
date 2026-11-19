"""twin for agent/kanban_stop.py:kanban_stop_nudge_enabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/kanban_stop.py"
    PY_FN = "kanban_stop_nudge_enabled"
    C_FN = "agent_kanban_stop_kanban_stop_nudge_enabled"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
