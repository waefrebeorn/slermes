"""twin for tools/file_tools.py:_is_blocked_device (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/file_tools.py"
    PY_FN = "_is_blocked_device"
    C_FN = "process"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
