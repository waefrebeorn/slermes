"""twin for hermes_state.py:_disable_close_time_checkpoint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_disable_close_time_checkpoint"
    C_FN = "_disable_close_time_checkpoint"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
