"""twin for hermes_cli/observability/shared_metrics.py:_validate_counter (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/shared_metrics.py"
    PY_FN = "_validate_counter"
    C_FN = "metrics_validate_counter"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
