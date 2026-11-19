"""twin for agent/review_engine.py:format_dispatch_note (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/review_engine.py"
    PY_FN = "format_dispatch_note"
    C_FN = "format_dispatch_note"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
