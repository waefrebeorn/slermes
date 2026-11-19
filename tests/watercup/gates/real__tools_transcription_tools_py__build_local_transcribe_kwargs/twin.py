"""twin for tools/transcription_tools.py:build_local_transcribe_kwargs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "build_local_transcribe_kwargs"
    C_FN = "build_local_transcribe_kwargs"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
