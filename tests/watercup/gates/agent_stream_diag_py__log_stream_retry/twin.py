"""twin for agent/stream_diag.py:log_stream_retry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/stream_diag.py"
    PY_FN = "log_stream_retry"
    C_FN = "sdi_log_stream_retry"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
