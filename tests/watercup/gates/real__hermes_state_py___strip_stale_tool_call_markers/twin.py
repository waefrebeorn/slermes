"""twin for hermes_state.py:_strip_stale_tool_call_markers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_strip_stale_tool_call_markers"
    C_FN = "_strip_stale_tool_call_markers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
