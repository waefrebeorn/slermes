"""twin for hermes_cli/voice.py:_play_beep (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/voice.py"
    PY_FN = "_play_beep"
    C_FN = "play_beep"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
