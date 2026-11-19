"""twin for hermes_cli/cli_billing_mixin.py:_billing_require_admin (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cli_billing_mixin.py"
    PY_FN = "_billing_require_admin"
    C_FN = "cbm_billing_require_admin"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
