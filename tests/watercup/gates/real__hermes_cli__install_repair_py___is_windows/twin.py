"""twin for hermes_cli/_install_repair.py:_is_windows (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_install_repair.py"
    PY_FN = "_is_windows"
    C_FN = "_is_windows"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
