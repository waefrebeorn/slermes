"""twin for tools/interrupt.py:clear (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/interrupt.py"
    PY_FN = "clear"
    C_FN = "tools_interrupt_clear"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
