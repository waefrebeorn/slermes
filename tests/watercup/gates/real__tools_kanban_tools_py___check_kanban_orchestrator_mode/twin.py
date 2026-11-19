"""twin for tools/kanban_tools.py:_check_kanban_orchestrator_mode (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/kanban_tools.py"
    PY_FN = "_check_kanban_orchestrator_mode"
    C_FN = "tools"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
