"""twin for agent/display.py:set_tool_preview_max_len (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/display.py"
    PY_FN = "set_tool_preview_max_len"
    C_FN = "disp_set_tool_preview_max_len"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
