"""twin for agent/display.py:_is_patch_stdout_proxy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/display.py"
    PY_FN = "_is_patch_stdout_proxy"
    C_FN = "disp_spinner_is_patch_stdout_proxy"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
