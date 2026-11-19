"""twin for agent/nous_rate_guard.py:_parse_reset_seconds (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/nous_rate_guard.py"
    PY_FN = "_parse_reset_seconds"
    C_FN = "nrg_parse_reset_seconds"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
