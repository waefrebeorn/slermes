"""twin for tools/browser_supervisor.py:_on_frame_navigated (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_supervisor.py"
    PY_FN = "_on_frame_navigated"
    C_FN = "browser_supervisor_on_frame_navigated_with_url"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
