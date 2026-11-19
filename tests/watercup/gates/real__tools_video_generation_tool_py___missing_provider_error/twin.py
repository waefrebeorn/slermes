"""twin for tools/video_generation_tool.py:_missing_provider_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/video_generation_tool.py"
    PY_FN = "_missing_provider_error"
    C_FN = "_missing_provider_error"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
