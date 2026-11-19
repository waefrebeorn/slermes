"""twin for tools/tts_tool.py:_is_command_tts_voice_compatible (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_tool.py"
    PY_FN = "_is_command_tts_voice_compatible"
    C_FN = "tts_is_command_tts_voice_compatible"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
