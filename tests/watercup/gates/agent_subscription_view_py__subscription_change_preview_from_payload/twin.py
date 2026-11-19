"""twin for agent/subscription_view.py:subscription_change_preview_from_payload (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/subscription_view.py"
    PY_FN = "subscription_change_preview_from_payload"
    C_FN = "agent_subscription_view_subscription_change_preview_from_pay_ad"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
