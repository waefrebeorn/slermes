"""twin for agent/retry_utils.py:parse_retry_after_seconds (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/retry_utils.py"
    PY_FN = "parse_retry_after_seconds"
    C_FN = "rtu_parse_retry_after_seconds"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
