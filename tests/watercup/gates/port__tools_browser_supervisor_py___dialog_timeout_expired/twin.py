"""twin for tools/browser_supervisor.py:_dialog_timeout_expired (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_supervisor.py"
    PY_FN = "_dialog_timeout_expired"
    C_FN = "browser_supervisor_dialog_timeout_expired"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
