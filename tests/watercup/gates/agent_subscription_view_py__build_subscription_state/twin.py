"""twin for agent/subscription_view.py:build_subscription_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/subscription_view.py"
    PY_FN = "build_subscription_state"
    C_FN = "agent_subscription_view_build_subscription_state"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
