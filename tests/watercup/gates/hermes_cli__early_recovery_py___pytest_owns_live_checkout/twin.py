"""twin for hermes_cli/_early_recovery.py:_pytest_owns_live_checkout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_early_recovery.py"
    PY_FN = "_pytest_owns_live_checkout"
    C_FN = "_pytest_owns_live_checkout"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
