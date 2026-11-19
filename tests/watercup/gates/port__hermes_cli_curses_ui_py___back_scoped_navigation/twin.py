"""twin for hermes_cli/curses_ui.py:_back_scoped_navigation (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/curses_ui.py"
    PY_FN = "_back_scoped_navigation"
    C_FN = "curses_ui_back_scoped_navigation"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
