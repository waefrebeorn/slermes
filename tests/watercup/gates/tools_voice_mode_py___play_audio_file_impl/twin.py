"""twin for tools/voice_mode.py:_play_audio_file_impl (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/voice_mode.py"
    PY_FN = "_play_audio_file_impl"
    C_FN = "voice_play_audio_file_impl"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
