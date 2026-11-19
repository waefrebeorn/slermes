"""twin for hermes_cli/local_runtime/growth.py:maybe_grow_window (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/growth.py"
    PY_FN = "maybe_grow_window"
    C_FN = "maybe_grow_window"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
