"""twin for hermes_cli/observability/shared_metrics_sender.py:send_pending (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/shared_metrics_sender.py"
    PY_FN = "send_pending"
    C_FN = "send_pending"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
