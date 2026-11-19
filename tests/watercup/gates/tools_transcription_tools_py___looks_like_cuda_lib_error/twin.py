"""twin for tools/transcription_tools.py:_looks_like_cuda_lib_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "_looks_like_cuda_lib_error"
    C_FN = "transcribe_looks_like_cuda_lib_error"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
