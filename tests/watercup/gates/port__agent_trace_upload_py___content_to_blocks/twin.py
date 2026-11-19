"""twin for agent/trace_upload.py:_content_to_blocks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/trace_upload.py"
    PY_FN = "_content_to_blocks"
    C_FN = "agent_trace_upload_u_content_to_blocks"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
