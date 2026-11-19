"""twin for hermes_cli/cli_billing_mixin.py:_show_subscription (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cli_billing_mixin.py"
    PY_FN = "_show_subscription"
    C_FN = "cmd_subscription"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
