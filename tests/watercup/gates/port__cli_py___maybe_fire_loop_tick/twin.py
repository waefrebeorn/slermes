"""twin for cli.py:_maybe_fire_loop_tick (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_maybe_fire_loop_tick"
    C_FN = "hcli_maybe_fire_loop_tick"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
