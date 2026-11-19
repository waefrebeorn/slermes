"""twin for gateway/platforms/yuanbao_media.py:is_image (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao_media.py"
    PY_FN = "is_image"
    C_FN = "ybm_is_image"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
