"""twin for tools/video_generation_tool.py:_resolve_active_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/video_generation_tool.py"
    PY_FN = "_resolve_active_provider"
    C_FN = "tools_video_generation_tool_u_resolve_active_provider"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
