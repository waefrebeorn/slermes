"""twin for hermes_state.py:get_compression_failure_cooldown_row (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "get_compression_failure_cooldown_row"
    C_FN = "get_compression_failure_cooldown_row"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
