"""twin for gateway/session.py:_clear_dirty_transcript (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session.py"
    PY_FN = "_clear_dirty_transcript"
    C_FN = "_clear_dirty_transcript"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
