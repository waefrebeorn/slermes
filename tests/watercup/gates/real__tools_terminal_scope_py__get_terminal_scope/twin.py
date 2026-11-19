"""twin for tools/terminal_scope.py:get_terminal_scope (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_scope.py"
    PY_FN = "get_terminal_scope"
    C_FN = "get_terminal_scope"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
