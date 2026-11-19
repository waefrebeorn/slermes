"""twin for tools/tts_streaming.py:_collect_async (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_streaming.py"
    PY_FN = "_collect_async"
    C_FN = "_collect_async"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
