"""twin for agent/anthropic_message_convert.py:_convert_tool_message_to_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_message_convert.py"
    PY_FN = "_convert_tool_message_to_result"
    C_FN = "_convert_tool_message_to_result"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
