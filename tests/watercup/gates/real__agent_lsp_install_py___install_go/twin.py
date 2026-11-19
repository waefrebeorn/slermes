"""twin for agent/lsp/install.py:_install_go (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/install.py"
    PY_FN = "_install_go"
    C_FN = "_install_go"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
