"""twin for hermes_cli/nous_subscription.py:get_nous_subscription_features (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/nous_subscription.py"
    PY_FN = "get_nous_subscription_features"
    C_FN = "get_nous_subscription_features"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
