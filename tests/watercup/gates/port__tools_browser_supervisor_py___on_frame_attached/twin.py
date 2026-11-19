"""twin for tools/browser_supervisor.py:_on_frame_attached (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_supervisor.py"
    PY_FN = "_on_frame_attached"
    C_FN = "browser_supervisor_on_frame_attached_with_session"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
