"""twin for tools/kanban_tools.py:_handle_request_changes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/kanban_tools.py"
    PY_FN = "_handle_request_changes"
    C_FN = "_handle_request_changes"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
