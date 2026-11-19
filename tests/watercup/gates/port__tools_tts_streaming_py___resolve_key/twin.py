"""twin for tools/tts_streaming.py:_resolve_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_streaming.py"
    PY_FN = "_resolve_key"
    C_FN = "stts_resolve_key"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
