"""twin for agent/lsp/workspace.py:nearest_root (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/workspace.py"
    PY_FN = "nearest_root"
    C_FN = "nearest_root"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
