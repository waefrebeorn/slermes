"""twin for hermes_cli/observability/shared_metrics_subscriber.py:deactivate (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/shared_metrics_subscriber.py"
    PY_FN = "deactivate"
    C_FN = "sm_subscriber_deactivate"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
