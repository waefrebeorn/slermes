"""twin for hermes_cli/nous_billing.py:_billing_not_logged_in (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/nous_billing.py"
    PY_FN = "_billing_not_logged_in"
    C_FN = "_billing_not_logged_in"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
