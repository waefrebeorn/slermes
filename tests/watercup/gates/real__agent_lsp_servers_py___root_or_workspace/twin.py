"""twin for agent/lsp/servers.py:_root_or_workspace (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/servers.py"
    PY_FN = "_root_or_workspace"
    C_FN = "_root_or_workspace"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
