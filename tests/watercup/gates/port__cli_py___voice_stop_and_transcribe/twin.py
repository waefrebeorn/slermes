"""twin for cli.py:_voice_stop_and_transcribe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_voice_stop_and_transcribe"
    C_FN = "_voice_stop_and_transcribe"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
