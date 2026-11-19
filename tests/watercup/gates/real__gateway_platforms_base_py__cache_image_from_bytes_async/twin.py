"""twin for gateway/platforms/base.py:cache_image_from_bytes_async (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "cache_image_from_bytes_async"
    C_FN = "cache_image_from_bytes_async"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
