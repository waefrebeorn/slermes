"""twin for gateway/platforms/yuanbao_media.py:get_image_format (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao_media.py"
    PY_FN = "get_image_format"
    C_FN = "yuanbao_get_image_format"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
