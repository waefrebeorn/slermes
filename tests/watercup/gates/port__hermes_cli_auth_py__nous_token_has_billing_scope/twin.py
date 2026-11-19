"""twin for hermes_cli/auth.py:nous_token_has_billing_scope (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth.py"
    PY_FN = "nous_token_has_billing_scope"
    C_FN = "ath_nous_token_has_billing_scope"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
