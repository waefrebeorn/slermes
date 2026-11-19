"""twin for hermes_cli/curses_ui.py:__call__ (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/curses_ui.py"
    PY_FN = "__call__"
    C_FN = "__call__"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
