"""twin for tools/image_source.py:_detect_video_mime (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/image_source.py"
    PY_FN = "_detect_video_mime"
    C_FN = "_detect_video_mime"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
