"""twin for agent/lsp/manager.py:_open_and_wait_async (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/manager.py"
    PY_FN = "_open_and_wait_async"
    C_FN = "_open_and_wait_async"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
