"""twin for hermes_cli/nous_billing.py:_resolve_token_and_base (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/nous_billing.py"
    PY_FN = "_resolve_token_and_base"
    C_FN = "_resolve_token_and_base"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
