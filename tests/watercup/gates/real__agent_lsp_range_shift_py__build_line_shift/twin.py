"""twin for agent/lsp/range_shift.py:build_line_shift (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/range_shift.py"
    PY_FN = "build_line_shift"
    C_FN = "build_line_shift"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
