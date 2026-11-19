"""twin for tools/transcription_tools.py:_dispatch_stt_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "_dispatch_stt_provider"
    C_FN = "_dispatch_stt_provider"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
