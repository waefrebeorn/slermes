"""twin for tools/vision_tools.py:_crop_image_region (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/vision_tools.py"
    PY_FN = "_crop_image_region"
    C_FN = "_crop_image_region"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
