"""twin for gateway/readiness.py:_probe_session_store (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/readiness.py"
    PY_FN = "_probe_session_store"
    C_FN = "_probe_session_store"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
