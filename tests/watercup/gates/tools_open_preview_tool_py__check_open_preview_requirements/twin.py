"""twin for tools/open_preview_tool.py:check_open_preview_requirements (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/open_preview_tool.py"
    PY_FN = "check_open_preview_requirements"
    C_FN = "tools_open_preview_tool_check_open_preview_requirements"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
