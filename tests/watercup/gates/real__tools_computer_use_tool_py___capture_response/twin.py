"""twin for tools/computer_use/tool.py:_capture_response (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/tool.py"
    PY_FN = "_capture_response"
    C_FN = "stream_diag_capture_response"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
