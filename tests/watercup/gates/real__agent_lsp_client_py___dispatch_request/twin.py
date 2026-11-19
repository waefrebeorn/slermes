"""twin for agent/lsp/client.py:_dispatch_request (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/client.py"
    PY_FN = "_dispatch_request"
    C_FN = "_dispatch_request"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
