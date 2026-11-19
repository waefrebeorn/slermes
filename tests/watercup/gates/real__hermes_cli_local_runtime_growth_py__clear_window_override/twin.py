"""twin for hermes_cli/local_runtime/growth.py:clear_window_override (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/growth.py"
    PY_FN = "clear_window_override"
    C_FN = "clear_window_override"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
