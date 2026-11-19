"""twin for gateway/sticker_cache.py:cache_sticker_description (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/sticker_cache.py"
    PY_FN = "cache_sticker_description"
    C_FN = "cli_gateway_sticker_cache_cache_sticker_description"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
