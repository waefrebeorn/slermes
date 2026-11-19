"""twin for hermes_cli/terminal_notify.py:_write_tty (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/terminal_notify.py"
    PY_FN = "_write_tty"
    C_FN = "_write_tty"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
