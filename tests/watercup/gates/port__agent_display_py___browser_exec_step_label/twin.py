"""twin for agent/display.py:_browser_exec_step_label (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/display.py"
    PY_FN = "_browser_exec_step_label"
    C_FN = "_browser_exec_step_label"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
