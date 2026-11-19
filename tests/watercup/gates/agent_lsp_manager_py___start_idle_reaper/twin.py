"""twin for agent/lsp/manager.py:_start_idle_reaper (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/manager.py"
    PY_FN = "_start_idle_reaper"
    C_FN = "lsp_service_start_idle_reaper"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
