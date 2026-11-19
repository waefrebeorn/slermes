"""twin for cli.py:_redraw_rebuilds_scrollback (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_redraw_rebuilds_scrollback"
    C_FN = "cli_redraw_rebuilds_scrollback"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
