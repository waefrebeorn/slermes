"""twin for tools/xai_video_tools.py:_handle_xai_video_edit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/xai_video_tools.py"
    PY_FN = "_handle_xai_video_edit"
    C_FN = "tools_xai_video_tools_u_handle_xai_video_edit"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
