"""twin for gateway/run.py:_dump_wedged_turn_stacks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_dump_wedged_turn_stacks"
    C_FN = "gw_dump_wedged_turn_stacks"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
