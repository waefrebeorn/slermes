"""twin for agent/conversation_loop.py:_canonicalize_tool_call_arguments (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_loop.py"
    PY_FN = "_canonicalize_tool_call_arguments"
    C_FN = "_canonicalize_tool_call_arguments"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
