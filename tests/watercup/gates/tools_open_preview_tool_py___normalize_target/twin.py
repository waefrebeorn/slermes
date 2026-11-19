"""twin for tools/open_preview_tool.py:_normalize_target (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/open_preview_tool.py"
    PY_FN = "_normalize_target"
    C_FN = "tools_open_preview_tool_u_normalize_target"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
