"""twin for tools/browser_tool.py:_maybe_stop_recording (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_maybe_stop_recording"
    C_FN = "browser__maybe_stop_recording"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
