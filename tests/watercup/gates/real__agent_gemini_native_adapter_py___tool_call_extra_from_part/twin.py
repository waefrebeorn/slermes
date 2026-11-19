"""twin for agent/gemini_native_adapter.py:_tool_call_extra_from_part (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/gemini_native_adapter.py"
    PY_FN = "_tool_call_extra_from_part"
    C_FN = "_tool_call_extra_from_part"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
