"""twin for tools/computer_use/tool.py:_capture_lost_detail (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/tool.py"
    PY_FN = "_capture_lost_detail"
    C_FN = "_capture_lost_detail"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
