"""twin for agent/message_sanitization.py:coalesce_tool_call_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/message_sanitization.py"
    PY_FN = "coalesce_tool_call_id"
    C_FN = "coalesce_tool_call_id"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
