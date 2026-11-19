"""twin for agent/stream_diag.py:stream_diag_init (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/stream_diag.py"
    PY_FN = "stream_diag_init"
    C_FN = "stream_diag_init"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
