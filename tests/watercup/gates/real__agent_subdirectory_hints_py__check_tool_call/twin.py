"""twin for agent/subdirectory_hints.py:check_tool_call (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/subdirectory_hints.py"
    PY_FN = "check_tool_call"
    C_FN = "check_tool_call"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
