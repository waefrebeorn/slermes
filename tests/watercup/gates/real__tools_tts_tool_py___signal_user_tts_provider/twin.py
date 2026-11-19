"""twin for tools/tts_tool.py:_signal_user_tts_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_tool.py"
    PY_FN = "_signal_user_tts_provider"
    C_FN = "_signal_user_tts_provider"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
