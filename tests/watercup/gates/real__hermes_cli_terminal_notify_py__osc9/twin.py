"""twin for hermes_cli/terminal_notify.py:osc9 (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/terminal_notify.py"
    PY_FN = "osc9"
    C_FN = "osc9"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
