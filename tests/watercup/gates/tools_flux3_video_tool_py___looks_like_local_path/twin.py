"""twin for tools/flux3_video_tool.py:_looks_like_local_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/flux3_video_tool.py"
    PY_FN = "_looks_like_local_path"
    C_FN = "f3_looks_like_local_path"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
