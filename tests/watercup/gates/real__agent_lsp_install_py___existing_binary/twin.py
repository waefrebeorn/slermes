"""twin for agent/lsp/install.py:_existing_binary (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/install.py"
    PY_FN = "_existing_binary"
    C_FN = "_existing_binary"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
