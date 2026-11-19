"""twin for gateway/session_db_recovery.py:_publish_health (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session_db_recovery.py"
    PY_FN = "_publish_health"
    C_FN = "_publish_health"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
