"""twin for hermes_cli/terminal_notify.py:warp_osc777 (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/terminal_notify.py"
    PY_FN = "warp_osc777"
    C_FN = "warp_osc777"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
