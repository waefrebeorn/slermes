"""twin for tools/browser_supervisor.py:_install_dialog_bridge (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_supervisor.py"
    PY_FN = "_install_dialog_bridge"
    C_FN = "browser_supervisor_install_dialog_bridge"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
