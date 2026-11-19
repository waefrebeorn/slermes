"""twin for tools/xai_video_tools.py:_check_xai_video_requirements (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/xai_video_tools.py"
    PY_FN = "_check_xai_video_requirements"
    C_FN = "tools_xai_video_tools_u_check_xai_video_requirements"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
