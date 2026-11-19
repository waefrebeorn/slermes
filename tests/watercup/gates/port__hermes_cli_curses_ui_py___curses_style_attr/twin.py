"""twin for hermes_cli/curses_ui.py:_curses_style_attr (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/curses_ui.py"
    PY_FN = "_curses_style_attr"
    C_FN = "hermes_cli_curses_ui_u_curses_style_attr"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
