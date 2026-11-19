"""twin for gateway/platforms/media_cache.py:ext_for_mime (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/media_cache.py"
    PY_FN = "ext_for_mime"
    C_FN = "ext_for_mime"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
