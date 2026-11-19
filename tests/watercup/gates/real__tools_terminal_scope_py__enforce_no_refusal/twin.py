"""twin for tools/terminal_scope.py:enforce_no_refusal (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_scope.py"
    PY_FN = "enforce_no_refusal"
    C_FN = "enforce_no_refusal"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
