"""twin for tools/transcription_tools.py:_apply_pre_transcription_hook (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "_apply_pre_transcription_hook"
    C_FN = "_apply_pre_transcription_hook"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
