"""twin for agent/lsp/manager.py:get_diagnostics_sync (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/manager.py"
    PY_FN = "get_diagnostics_sync"
    C_FN = "get_diagnostics_sync"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
