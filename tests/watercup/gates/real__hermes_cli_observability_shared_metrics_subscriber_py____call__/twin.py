"""twin for hermes_cli/observability/shared_metrics_subscriber.py:__call__ (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/shared_metrics_subscriber.py"
    PY_FN = "__call__"
    C_FN = "sm_subscriber_call"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
