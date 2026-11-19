"""twin for tools/vision_tools.py:_vision_analyze_native (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/vision_tools.py"
    PY_FN = "_vision_analyze_native"
    C_FN = "_vision_analyze_native"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
