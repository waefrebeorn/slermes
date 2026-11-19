"""twin for tools/tts_tool.py:_resolve_max_text_length (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_tool.py"
    PY_FN = "_resolve_max_text_length"
    C_FN = "tts_resolve_max_text_length"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
