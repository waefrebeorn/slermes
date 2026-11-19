"""twin for cli.py:_mark_tui_input_modes_active (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_mark_tui_input_modes_active"
    C_FN = "cli_mark_tui_input_modes_active"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
