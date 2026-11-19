"""twin for agent/lsp/manager.py:_client_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/manager.py"
    PY_FN = "_client_key"
    C_FN = "_client_key"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
