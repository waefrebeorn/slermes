"""twin for tools/xai_video_tools.py:_clean_string (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/xai_video_tools.py"
    PY_FN = "_clean_string"
    C_FN = "tools_xai_video_tools_u_clean_string"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
