"""twin for cli.py:_schedule_focus_regain_redraw (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_schedule_focus_regain_redraw"
    C_FN = "hcli_schedule_focus_regain_redraw"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
