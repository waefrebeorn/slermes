"""twin for cron/scheduler.py:_close_late_session_db_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_close_late_session_db_result"
    C_FN = "_close_late_session_db_result"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
