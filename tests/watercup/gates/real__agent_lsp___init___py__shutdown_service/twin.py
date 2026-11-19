"""twin for agent/lsp/__init__.py:shutdown_service (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/__init__.py"
    PY_FN = "shutdown_service"
    C_FN = "lspi_shutdown_service"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
