"""twin for tools/tts_tool.py:_has_ffmpeg (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_tool.py"
    PY_FN = "_has_ffmpeg"
    C_FN = "tts_has_ffmpeg"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
