"""twin for hermes_cli/observability/shared_metrics_sender.py:_retry_after_seconds (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/shared_metrics_sender.py"
    PY_FN = "_retry_after_seconds"
    C_FN = "_retry_after_seconds"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
