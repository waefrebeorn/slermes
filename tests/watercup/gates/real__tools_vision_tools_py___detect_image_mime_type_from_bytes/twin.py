"""twin for tools/vision_tools.py:_detect_image_mime_type_from_bytes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/vision_tools.py"
    PY_FN = "_detect_image_mime_type_from_bytes"
    C_FN = "_detect_image_mime_type_from_bytes"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
