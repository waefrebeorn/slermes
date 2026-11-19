"""twin for tools/tts_tool.py:_tts_cache_get_or_load (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_tool.py"
    PY_FN = "_tts_cache_get_or_load"
    C_FN = "_tts_cache_get_or_load"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
