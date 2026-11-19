"""twin for tools/video_generation_tool.py:_normalize_reference_images (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/video_generation_tool.py"
    PY_FN = "_normalize_reference_images"
    C_FN = "_normalize_reference_images"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
