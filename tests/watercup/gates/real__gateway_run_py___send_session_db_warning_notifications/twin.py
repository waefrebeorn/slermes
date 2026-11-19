"""twin for gateway/run.py:_send_session_db_warning_notifications (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_send_session_db_warning_notifications"
    C_FN = "_send_session_db_warning_notifications"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
