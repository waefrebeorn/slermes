"""twin for hermes_cli/proxy/sse_done.py:mark_interrupted (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/proxy/sse_done.py"
    PY_FN = "mark_interrupted"
    C_FN = "mark_interrupted"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
