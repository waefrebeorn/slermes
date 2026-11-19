"""twin for tools/transcription_tools.py:_unload_local_model (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "_unload_local_model"
    C_FN = "trt_unload_local_model"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
