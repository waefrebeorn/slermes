"""twin for tools/transcription_tools.py:_trim_silence_for_cloud_stt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "_trim_silence_for_cloud_stt"
    C_FN = "_trim_silence_for_cloud_stt"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
