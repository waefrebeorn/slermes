"""twin for agent/lsp/range_shift.py:shift_diagnostic_range (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/range_shift.py"
    PY_FN = "shift_diagnostic_range"
    C_FN = "shift_diagnostic_range"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
