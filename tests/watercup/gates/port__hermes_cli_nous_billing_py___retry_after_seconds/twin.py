"""twin for hermes_cli/nous_billing.py:_retry_after_seconds (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/nous_billing.py"
    PY_FN = "_retry_after_seconds"
    C_FN = "nb2_retry_after_seconds"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
