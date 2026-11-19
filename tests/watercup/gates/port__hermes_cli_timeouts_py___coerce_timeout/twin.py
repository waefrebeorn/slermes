"""twin for hermes_cli/timeouts.py:_coerce_timeout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/timeouts.py"
    PY_FN = "_coerce_timeout"
    C_FN = "timeouts_coerce_timeout"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
