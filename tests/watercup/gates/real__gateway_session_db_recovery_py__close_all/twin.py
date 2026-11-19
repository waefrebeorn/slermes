"""twin for gateway/session_db_recovery.py:close_all (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session_db_recovery.py"
    PY_FN = "close_all"
    C_FN = "close_all"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
