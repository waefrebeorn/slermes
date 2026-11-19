"""twin for cli.py:_clear_active_overlays_for_interrupt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_clear_active_overlays_for_interrupt"
    C_FN = "cli__clear_active_overlays_for_interrupt"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
