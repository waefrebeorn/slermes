"""twin for agent/message_sanitization.py:_repair_tool_call_arguments (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/message_sanitization.py"
    PY_FN = "_repair_tool_call_arguments"
    C_FN = "_repair_tool_call_arguments"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
