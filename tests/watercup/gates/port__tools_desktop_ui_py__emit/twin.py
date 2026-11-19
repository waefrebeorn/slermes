"""twin for tools/desktop_ui.py:emit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/desktop_ui.py"
    PY_FN = "emit"
    C_FN = "tools_desktop_ui_emit"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
