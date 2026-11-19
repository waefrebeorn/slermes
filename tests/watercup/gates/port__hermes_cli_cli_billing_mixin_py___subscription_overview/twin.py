"""twin for hermes_cli/cli_billing_mixin.py:_subscription_overview (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cli_billing_mixin.py"
    PY_FN = "_subscription_overview"
    C_FN = "hermes_cli_cli_billing_mixin_u_subscription_overview"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
