"""twin for gateway/platforms/media_cache.py:mime_for_ext (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/media_cache.py"
    PY_FN = "mime_for_ext"
    C_FN = "mime_for_ext"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
