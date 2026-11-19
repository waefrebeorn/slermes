"""twin for tools/terminal_scope.py:reset_terminal_scope (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_scope.py"
    PY_FN = "reset_terminal_scope"
    C_FN = "reset_terminal_scope"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
