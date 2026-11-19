"""twin for tools/vision_tools.py:_rasterize_svg_to_png (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/vision_tools.py"
    PY_FN = "_rasterize_svg_to_png"
    C_FN = "_rasterize_svg_to_png"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
