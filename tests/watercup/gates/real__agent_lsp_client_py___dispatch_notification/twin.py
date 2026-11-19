"""twin for agent/lsp/client.py:_dispatch_notification (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/client.py"
    PY_FN = "_dispatch_notification"
    C_FN = "dispatch_notification"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
