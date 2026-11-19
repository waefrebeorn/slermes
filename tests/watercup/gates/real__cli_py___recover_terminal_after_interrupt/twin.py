"""twin for cli.py:_recover_terminal_after_interrupt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_recover_terminal_after_interrupt"
    C_FN = "cli__recover_terminal_after_interrupt"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
