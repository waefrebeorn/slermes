"""twin for hermes_cli/proxy/sse_done.py:_dispatch_event (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/proxy/sse_done.py"
    PY_FN = "_dispatch_event"
    C_FN = "_dispatch_event"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
