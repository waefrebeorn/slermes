"""twin for hermes_cli/observability/relay_shared_metrics.py:_reconcile_send_consent_once (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/relay_shared_metrics.py"
    PY_FN = "_reconcile_send_consent_once"
    C_FN = "_reconcile_send_consent_once"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
