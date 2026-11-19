"""twin for agent/video_gen_provider.py:error_response (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/video_gen_provider.py"
    PY_FN = "error_response"
    C_FN = "error_response"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
