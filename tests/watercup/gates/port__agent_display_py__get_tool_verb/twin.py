"""twin for agent/display.py:get_tool_verb (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/display.py"
    PY_FN = "get_tool_verb"
    C_FN = "get_tool_verb"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
