"""twin for hermes_cli/observability/shared_metrics.py:_utc_now (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/shared_metrics.py"
    PY_FN = "_utc_now"
    C_FN = "sm_utc_now"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
