"""twin for gateway/sticker_cache.py:get_cached_description (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/sticker_cache.py"
    PY_FN = "get_cached_description"
    C_FN = "get_cached_description"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
