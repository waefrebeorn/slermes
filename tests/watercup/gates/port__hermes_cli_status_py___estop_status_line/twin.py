"""twin for hermes_cli/status.py:_estop_status_line (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/status.py"
    PY_FN = "_estop_status_line"
    C_FN = "estop_paused_reply"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
