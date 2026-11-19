"""twin for tools/skills_tool.py:_is_gateway_surface (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_tool.py"
    PY_FN = "_is_gateway_surface"
    C_FN = "skills_tool_is_gateway_surface"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
