"""twin for hermes_cli/nous_billing.py:delete_subscription_pending_change (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/nous_billing.py"
    PY_FN = "delete_subscription_pending_change"
    C_FN = "hermes_cli_nous_billing_delete_subscription_pending_change"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
