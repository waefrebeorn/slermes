"""twin for agent/lsp/reporter.py:truncate (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/reporter.py"
    PY_FN = "truncate"
    C_FN = "truncate"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
