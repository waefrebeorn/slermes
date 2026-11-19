"""twin for agent/lsp/install.py:_do_install (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/install.py"
    PY_FN = "_do_install"
    C_FN = "_do_install"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
