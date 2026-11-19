"""twin for agent/chat_completion_helpers.py:_report_stale_nonstream_kill (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/chat_completion_helpers.py"
    PY_FN = "_report_stale_nonstream_kill"
    C_FN = "_report_stale_nonstream_kill"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
