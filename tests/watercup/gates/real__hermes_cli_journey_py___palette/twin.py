"""twin for hermes_cli/journey.py:_palette (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/journey.py"
    PY_FN = "_palette"
    C_FN = "app_desktop_toggle_command_palette"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
