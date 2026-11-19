"""twin for hermes_cli/kanban_decompose.py:decompose_task (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_decompose.py"
    PY_FN = "decompose_task"
    C_FN = "decompose_task"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
