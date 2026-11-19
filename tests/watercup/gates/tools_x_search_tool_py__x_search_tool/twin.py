"""twin for tools/x_search_tool.py:x_search_tool (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/x_search_tool.py"
    PY_FN = "x_search_tool"
    C_FN = "tools_x_search_tool_x_search_tool"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
