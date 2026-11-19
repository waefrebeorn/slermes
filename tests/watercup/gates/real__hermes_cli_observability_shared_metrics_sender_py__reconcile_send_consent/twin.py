"""twin for hermes_cli/observability/shared_metrics_sender.py:reconcile_send_consent (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/shared_metrics_sender.py"
    PY_FN = "reconcile_send_consent"
    C_FN = "reconcile_send_consent"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
