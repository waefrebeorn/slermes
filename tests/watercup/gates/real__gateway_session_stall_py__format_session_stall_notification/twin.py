"""twin for gateway/session_stall.py:format_session_stall_notification (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session_stall.py"
    PY_FN = "format_session_stall_notification"
    C_FN = "format_session_stall_notification"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
