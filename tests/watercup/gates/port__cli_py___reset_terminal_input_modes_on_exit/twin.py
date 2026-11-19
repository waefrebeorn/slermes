"""twin for cli.py:_reset_terminal_input_modes_on_exit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_reset_terminal_input_modes_on_exit"
    C_FN = "_reset_terminal_input_modes_on_exit"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
