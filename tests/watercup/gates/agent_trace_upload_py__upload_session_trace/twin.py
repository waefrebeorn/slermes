"""twin for agent/trace_upload.py:upload_session_trace (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/trace_upload.py"
    PY_FN = "upload_session_trace"
    C_FN = "agent_trace_upload_upload_session_trace"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
