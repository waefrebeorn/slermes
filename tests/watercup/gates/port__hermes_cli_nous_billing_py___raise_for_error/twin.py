"""twin for hermes_cli/nous_billing.py:_raise_for_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/nous_billing.py"
    PY_FN = "_raise_for_error"
    C_FN = "_raise_for_error"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
